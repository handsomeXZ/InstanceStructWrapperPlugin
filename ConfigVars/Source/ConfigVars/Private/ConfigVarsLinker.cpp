#include "ConfigVarsLinker.h"

#include "HAL/FileManagerGeneric.h"

#include "PrivateAccessor.h"
#include "ConfigVarsTypes.h"

#include "ConfigVarsLinkerEditorData.h"

PRIVATE_DEFINE_VAR(FLinkerLoad, TOptional<FStructuredArchive::FRecord>, StructuredArchiveRootRecord);
PRIVATE_DEFINE_VAR(FConfigVarsBag, int32, ExportIndex);

DEFINE_LOG_CATEGORY_STATIC(LogConfigVarsLinker, Log, All);

namespace LinkerUtils
{

	uint16 IndexRangeToBegin(void* IndexRange) {
		// 传入时即为int32，所以可以直接无视warning
		uint32 Range = (uint64)(IndexRange);
		return ((Range & 0xFFFF0000) >> 16) - 1;
	}
	uint16 IndexRangeToEnd(void* IndexRange) {
		// 传入时即为int32，所以可以直接无视warning
		uint32 Range = (uint64)(IndexRange);
		return (Range & 0x0000FFFF ) - 1;
	}

}

struct FSerialSizeScope
{
	FSerialSizeScope(FArchive& Ar, int32& InSerialSize)
		: HeadOffset(Ar.Tell())
		, Archive(Ar)
		, SerialSize(InSerialSize)
	{
		Archive << SerialSize;

		if (Ar.IsSaving())
		{
			InitialOffset = Archive.Tell();
		}
	}
	~FSerialSizeScope()
	{
		if (Archive.IsSaving())
		{
			const int64 FinalOffset = Archive.Tell();

			Archive.Seek(HeadOffset);	// 覆写占位数据
			SerialSize = (int32)(FinalOffset - InitialOffset);
			Archive << SerialSize;
			Archive.Seek(FinalOffset);	// 还原偏移
		}
	}

private:
	int64 HeadOffset;
	int64 InitialOffset;
	FArchive& Archive;
	int32& SerialSize;
};

class FConfigVarsUtils
{
public:
	template<typename T>
	static void SerializeObject(FStructuredArchive::FRecord Record, UConfigVarsLinker* Linker, T*& OtherObj)
	{
		UObject* Obj = OtherObj;
		SerializeObject(Record, Linker, Obj);
		OtherObj = (T*)Obj;
	}

	static void SerializeObject(FStructuredArchive::FRecord Record, UConfigVarsLinker* Linker, UObject * &Obj)
	{
		FArchive& Ar = Record.GetUnderlyingArchive();

		if (Ar.IsSaving())
		{
			if (IsValid(Obj))
			{
				int32 ImportIndex = Linker->ImportObject(Obj);
				Record << SA_VALUE(TEXT("ImportIndex"), ImportIndex);
			}
			else
			{
				int32 NullImportIndex = INDEX_NONE;
				Record << SA_VALUE(TEXT("ImportIndex"), NullImportIndex);
			}
		}
		else if (Ar.IsLoading())
		{
			int32 ImportIndex = INDEX_NONE;
			Record << SA_VALUE(TEXT("ImportIndex"), ImportIndex);

			Obj = nullptr;

			if (ImportIndex == INDEX_NONE)
			{
				return;
			}

			FConfigVarsImport& Import = Linker->ImportTable[ImportIndex];

			if (UPackage* ExistingPackage = FindObjectFast<UPackage>(/*Outer =*/nullptr, Import.ObjectPath.GetLongPackageFName()))
			{
				if (Import.ObjectPath.IsAsset())
				{
					if (UObject* ExistingObject = FindObjectFast<UObject>(ExistingPackage, Import.ObjectPath.GetAssetFName()))
					{
						Obj = ExistingObject;
					}
				}
				else if (Import.ObjectPath.IsSubobject())
				{
					if (UObject* ExistingObject = FindObject<UObject>(ExistingPackage, *Import.ObjectPath.GetSubPathString()))
					{
						Obj = ExistingObject;
					}
				}
			}
		}

	}
	static void SerializeConfigVars(FStructuredArchive::FRecord ExportRecord, UConfigVarsLinker* Linker, FStructView ConfigVarsData);

	static bool ShouldSerializeValue(FArchive& Ar, FProperty* Property);

	template<typename SrcType>
	static void SerializeProperties(FStructuredArchive::FRecord ExportRecord, UConfigVarsLinker* Linker, const UStruct* DataStruct, SrcType* SrcData);

	template<typename SrcType>
	static void SerializeItem(FStructuredArchive::FRecord PropertyRecord, FProperty* ChildProperty, UConfigVarsLinker* Linker, SrcType* SrcData);

	template<typename SrcType>
	static void VerifyNestedDataStruct(FArchive& Ar, UConfigVarsLinker* Linker, const UStruct* DataStruct, SrcType* SrcData, int32 Depth);
	template<typename SrcType>
	static void VerifyNestedDataProperties(FArchive& Ar, UConfigVarsLinker* Linker, FProperty* ChildProperty, SrcType* SrcData, int32 Depth);
};

FArchive& operator<<(FArchive& Ar, FConfigVarsImport& Import)
{
	Ar << Import.ObjectPath;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FConfigVarsExport& Export)
{
	Ar << Export.SerialLocation;
	Ar << Export.ClassIndex;
	Ar << Export.Depth;
	Ar << Export.ImportSet;

	return Ar;
}

//////////////////////////////////////////////////////////////////////////
void UConfigVarsLinker::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.IsSaving())
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			//return;
		}

		VerifyData(Ar, Cooking | PreSerialize,
			[this](){
				VerifyPendingRemovedExport();
				VerifyAllExportLoaded();
			}
		);
	}

	Super::Serialize(Record);

	if (Ar.IsSaving())
	{
		SerializeHeadData(Record);
		SerializeExportData(Record);
		SerializeTableData(Record);

	}
	else if (Ar.IsLoading())
	{
		if (PendingLoadExports_Async.IsEmpty())
		{
			// 仅第一次加载Linker 和 编辑器的同步加载 LoadOrAddData() 会走这里
			SerializeHeadData(Record);
			SerializeExportData(Record);
			SerializeTableData(Record);
		}
		else
		{
			ProcessPendingLoadExports(Record);
		}
	}

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << LinkerEditorData;
	}
#endif
}

bool UConfigVarsLinker::Rename(const TCHAR* NewName /* = nullptr */, UObject* NewOuter /* = nullptr */, ERenameFlags Flags /* = REN_None */)
{
#if WITH_EDITOR
	//LinkerEditorData = nullptr;
#endif
	bool bSuccess = Super::Rename(NewName, NewOuter, Flags);

	return bSuccess;
}

void UConfigVarsLinker::SerializeHeadData(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.IsSaving())
	{
		// 序列化时必须确保LinkerEditorData存在
		UConfigVarsLinkerEditorData* EditorData = GetLinkerEditorData();
		if (!EditorData)
		{
			return;
		}

		int32 ExportObjectsNum = 0;

		EditorData->ExportDataSerializeOrderSet.Empty();

#if WITH_EDITOR
		// 先处理确定有序的ExportIndex。
		for (int32 OrderIndex : EditorData->ExportDataOrderSet)
		{
			if (IsValid(ExportDataOuter[OrderIndex]) && !(ExportDataOuter[OrderIndex]->HasAnyFlags(RF_Transient)))
			{
				++ExportObjectsNum;
				EditorData->ExportDataSerializeOrderSet.Add(OrderIndex);
			}
		}

		for (int32 index = 0; index < ExportData.Num(); ++index)
		{
			if (!EditorData->ExportDataSerializeOrderSet.Contains(index) && ExportData[index].IsValid() &&
				IsValid(ExportDataOuter[index]) && !(ExportDataOuter[index]->HasAnyFlags(RF_Transient)))
			{
				EditorData->ExportDataSerializeOrderSet.Add(index);
				++ExportObjectsNum;
			}
		}
#else
		// 先处理确定有序的ExportIndex。
		for (int32 OrderIndex : EditorData->ExportDataOrderSet)
		{
			++ExportObjectsNum;
			EditorData->ExportDataSerializeOrderSet.Add(OrderIndex);
		}

		for (int32 index = 0; index < ExportData.Num(); ++index)
		{
			if (!EditorData->ExportDataSerializeOrderSet.Contains(index) && ExportData[index].IsValid())
			{
				EditorData->ExportDataSerializeOrderSet.Add(index);
				++ExportObjectsNum;
			}
		}
#endif

		Ar << ExportObjectsNum;

		int32 InitialLocation = Ar.Tell();
		Ar << InitialLocation;
	}
	else if (Ar.IsLoading())
	{
		int32 ExportObjectsNum = 0;
		Ar << ExportObjectsNum;

		// 常规反序列化流程
		{
			ExportData.SetNum(ExportObjectsNum);
		}

#if WITH_EDITOR
		ExportDataOuter.SetNum(ExportObjectsNum);
#endif

		int32 InitialLocation = 0;
		Ar << InitialLocation;
	}
}

void UConfigVarsLinker::SerializeExportData(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.IsSaving())
	{
		// 序列化时必须确保LinkerEditorData存在
		UConfigVarsLinkerEditorData* EditorData = GetLinkerEditorData();
		if (!EditorData)
		{
			return;
		}

		int32 ExportDataSize = 0;
		FSerialSizeScope Scope(Ar, ExportDataSize);	// ExportDataSize

		// 验证所有嵌套的数据
		VerifyData(Ar, PreSerialize, [this, EditorData, &Ar](){
			 // 先假设所有数据都是顶层的
			EditorData->TopOrderSet = EditorData->ExportDataSerializeOrderSet;
			TSet<int32> TopOrderSet = EditorData->TopOrderSet;

			// 第一次遍历，从TopOrderSet中剔除嵌套数据的Index
			for (int32 OrderIndex : TopOrderSet)
			{
				VerifyNestedData(Ar, OrderIndex);
			}

			// 开始记录嵌套数据的Index和Depth
			EditorData->ExportDataDepthSet.Empty();
			EditorData->ExportDataOrderSet.Empty();
			EditorData->ExportDataSerializeOrderSet.Empty();
			for (int32 OrderIndex : EditorData->TopOrderSet)
			{
				EditorData->ExportDataDepthSet.Add(1);
				EditorData->ExportDataOrderSet.Add(OrderIndex);
				EditorData->ExportDataSerializeOrderSet.Add(OrderIndex);
				VerifyNestedData(Ar, OrderIndex);
			}
		});

		ImportTable.Empty();
		ExportTable.Empty();

		// 此时所有ExportIndex都是有序的。
		for (int32 OrderIndex : EditorData->ExportDataSerializeOrderSet)
		{
			ExportStruct(Record, ExportData[OrderIndex]);
		}
	}
	else if (Ar.IsLoading())
	{
		int32 ExportDataSize = 0;
		FSerialSizeScope Scope(Ar, ExportDataSize);	// ExportDataSize

		// 跳过这部分数据的反序列化
		//FArchiveFileReaderGeneric& FileReader = static_cast<FArchiveFileReaderGeneric&>(Ar.GetLoader());
		Ar.Seek(Ar.Tell() + ExportDataSize);
	}
}

void UConfigVarsLinker::SerializeTableData(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.IsSaving())
	{
		int32 TableDataSize = 0;
		FSerialSizeScope Scope(Ar, TableDataSize);	// TableDataSize
		Ar << ImportTable;
		Ar << ExportTable;
	}
	else if (Ar.IsLoading())
	{
		int32 TableDataSize = 0;
		FSerialSizeScope Scope(Ar, TableDataSize);	// TableDataSize

		Ar << ImportTable;
		Ar << ExportTable;
	}
}

void UConfigVarsLinker::ProcessPendingLoadExports(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	{
		FScopeLock ScopeLock(&ExportDataCritical);
		int32 ExportObjectsNum = ExportData.Num();
		Ar << ExportObjectsNum;
	}

	// 计算序列化地址偏移
	int32 RealLoaderLocation = Ar.Tell();
	int32 RecordLoaderLocation = 0;
	Ar << RecordLoaderLocation;
	int32 SerializeHeadOffset = RecordLoaderLocation - RealLoaderLocation;

	int32 ExportDataSize = 0;
	int32 TableDataSize = 0;

	Ar << ExportDataSize;
	int32 InitialOffset = Ar.Tell();

	//////////////////////////////////////////////////////////////////////////
	// 反序列化核心逻辑

	TArray<void*> PendingIndexRanges;
	PendingLoadExports_Async.PopAll(PendingIndexRanges);
	for (void* IndexRange : PendingIndexRanges)
	{
		uint32 ExportIndexBegin	= LinkerUtils::IndexRangeToBegin(IndexRange);
		uint32 ExportIndexEnd	= LinkerUtils::IndexRangeToEnd(IndexRange);
		for (uint32 ExportIndex = ExportIndexBegin; ExportIndex <= ExportIndexEnd; ++ExportIndex)
		{
			FConfigVarsExport& Export = ExportTable[ExportIndex];
			FConfigVarsImport& Import = ImportTable[Export.ClassIndex];

			{
				FScopeLock ScopeLock(&ExportDataCritical);
				if (ExportData[ExportIndex].IsValid())
				{
					continue;
				}
			}

			UScriptStruct* ExportStruct = Cast<UScriptStruct>(Import.ObjectPath.ResolveObject());
			if (ExportStruct)
			{
				FLoadedConfigVarsData* NewData = new FLoadedConfigVarsData(ExportIndex, ExportStruct);

				// 反序列化Object的数据
				Ar.Seek(Export.SerialLocation - SerializeHeadOffset);

				FConfigVarsUtils::SerializeConfigVars(Record, this, NewData->Data);

				LoadedConfigVarsDatas_Async.Push(NewData);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////

	// 跳过ExportData的序列化
	Ar.Seek(InitialOffset + ExportDataSize);

	// 跳过TableData的序列化
	Ar << TableDataSize;
	Ar.Seek(Ar.Tell() + TableDataSize);


}

int32 UConfigVarsLinker::ImportObject(const UObject* ImportObj)
{
	FSoftObjectPath ImportObjectPath(ImportObj);

	for (int32 Index = 0; Index < ImportTable.Num(); ++Index)
	{
		if (ImportTable[Index].ObjectPath == ImportObjectPath)
		{
			return Index;
		}
	}
	
	ImportTable.Emplace(ImportObjectPath);

	return ImportTable.Num() - 1;
}

void UConfigVarsLinker::ExportStruct(FStructuredArchive::FRecord Record, FStructView StructData)
{
	UConfigVarsLinkerEditorData* EditorData = GetLinkerEditorData();
	FArchive& Ar = Record.GetUnderlyingArchive();

	int32 InitialImportNum = ImportTable.Num();

	int32 InitialOffset = Ar.Tell();
	{
		FConfigVarsUtils::SerializeConfigVars(Record, this, StructData);
	}

	int32 FinalImportNum = ImportTable.Num();

	FConfigVarsExport& Export = ExportTable.AddDefaulted_GetRef();
	Export.SerialLocation = InitialOffset;
	Export.ClassIndex = ImportObject(StructData.GetScriptStruct());
	Export.Depth = EditorData->ExportDataDepthSet[ExportTable.Num() - 1];

	if (FinalImportNum > InitialImportNum)
	{
		Export.ImportSet.Reset(FinalImportNum);
		Export.ImportSet.AddRange(InitialImportNum, FinalImportNum - 1);
	}

}

void UConfigVarsLinker::VerifyAllExportLoaded()
{
	// 在Editor模式下，需要在序列化前加载完成所有未加载的Export对象。
	for (int32 Index = 0; Index < ExportTable.Num(); ++Index)
	{
		// 注意：已经加载过的对象可能已经过时了，需要结合ClassIndex来判断。

		bool bExportDataValid = false;
		{
			FScopeLock ScopeLock(&ExportDataCritical);
			bExportDataValid = ExportData[Index].IsValid();
		}

		if (!bExportDataValid && ExportTable[Index].ClassIndex != INDEX_NONE)
		{
#if WITH_EDITOR
			FConfigVarsBag Bag;
			Bag.ExportIndex = Index;
			// 临时以Package填充Outer
			LoadOrAddData(Bag, nullptr, nullptr);
#else
			// LoadData 会走异步加载，Cook时不能使用
			LoadData(Index);
#endif
		}
	}
}

void UConfigVarsLinker::VerifyData(FArchive& Ar, EConfigVarsSerialStage Stage, TFunction<void()> Func)
{
	// 只有FLinkerLoad or FLinkerSave可以取到Linker。
	// 而FPackageHarvester不是我们的真正FileWriter的Ar，仅仅是记录一些额外的信息，例如引用到的FName。

	if (Ar.IsCooking() && Stage & EConfigVarsSerialStage::Cooking)
	{
		Func();
	}
	
	if (!Ar.GetLinker() && Stage & EConfigVarsSerialStage::PreSerialize)
	{
		Func();
	}
	
	if (Ar.GetLinker() && Stage & EConfigVarsSerialStage::Serialize)
	{
		Func();
	}
}

void UConfigVarsLinker::VerifyNestedData(FArchive& Ar, int32 ExportIndex)
{
	if (!ExportData.IsValidIndex(ExportIndex) || !ExportData[ExportIndex].IsValid())
	{
		return;
	}

	FConfigVarsUtils::VerifyNestedDataStruct(Ar, this, ExportData[ExportIndex].GetScriptStruct(), ExportData[ExportIndex].GetMutableMemory(), 1);
}

void UConfigVarsLinker::VerifyPendingRemovedExport()
{
	UConfigVarsLinkerEditorData* EditorData = GetLinkerEditorData();
	if (!EditorData)
	{
		return;
	}

	for (int32 RemovedIndex : EditorData->PendingRemovedSet)
	{
		if (ExportTable.IsValidIndex(RemovedIndex))
		{
			ExportTable[RemovedIndex].ClassIndex = INDEX_NONE;
		}

		if (ExportData.IsValidIndex(RemovedIndex))
		{
			ExportData[RemovedIndex].Reset();
		}
	}
}

void UConfigVarsLinker::LoadImports_Sync(uint16 ExportIndexBegin, uint16 ExportIndexEnd)
{
	TArray<int32> AsyncLoadRequestIDs;

	for (int32 ExportIndex = ExportIndexBegin; ExportIndex <= ExportIndexEnd; ++ExportIndex)
	{
		FConfigVarsExport& Export = ExportTable[ExportIndex];
		// Class
		AsyncLoadRequestIDs.Add(LoadImport_Async(Export.ClassIndex, FLoadPackageAsyncDelegate(), AsyncLoadHighPriority));

		// Dependency
		for (FBitArray::FIterator It(Export.ImportSet); It; ++It)
		{
			int32 Index = *It;
			AsyncLoadRequestIDs.Add(LoadImport_Async(Index, FLoadPackageAsyncDelegate(), AsyncLoadHighPriority));
		}
	}

	FlushAsyncLoading(AsyncLoadRequestIDs);
}

int32 UConfigVarsLinker::LoadImport_Async(int32 ExportIndex, FLoadPackageAsyncDelegate LoadPackageAsyncDelegate, int32 Priority)
{
	// 暂时不支持UObjectRedirector

	FConfigVarsImport& Import = ImportTable[ExportIndex];
	if (UPackage* ExistingPackage = FindObjectFast<UPackage>(/*Outer =*/nullptr, Import.ObjectPath.GetLongPackageFName()))
	{
		if (Import.ObjectPath.IsAsset())
		{
			if (UObject* ExistingObject = FindObjectFast<UObject>(ExistingPackage, Import.ObjectPath.GetAssetFName()))
			{
				return INDEX_NONE;
			}
		}
		else if (Import.ObjectPath.IsSubobject())
		{
			if (UObject* ExistingObject = FindObject<UObject>(ExistingPackage, *Import.ObjectPath.GetSubPathString()))
			{
				return INDEX_NONE;
			}
		}
	}

	constexpr int32 PIEInstanceID = INDEX_NONE;
	return LoadPackageAsync(Import.ObjectPath.GetAssetPath().GetPackageName().ToString(), LoadPackageAsyncDelegate, Priority, PKG_None, PIEInstanceID);
}

void UConfigVarsLinker::PushToPendingLoadExports(uint16 ExportIndexBegin, uint16 ExportIndexEnd)
{
	if (ExportIndexBegin > MAX_EXPORTINDEX || ExportIndexEnd > MAX_EXPORTINDEX)
	{
		return;
	}

	// Index 最大值 == 65534
	// 因为Index必须 +1，否则当 Index == 0 == NULL时，无锁队列无法取值。
	uint32 IndexRange = ((ExportIndexBegin + 1) << 16) | (ExportIndexEnd + 1);
	// 不需要每次都new一个对象，直接将Index转指针就行。
#pragma warning(disable: 4312)
	PendingLoadExports_Async.Push(reinterpret_cast<void*>(IndexRange));
#pragma warning(default: 4312)
}

void UConfigVarsLinker::LoadExports_Sync(uint16 ExportIndexBegin, uint16 ExportIndexEnd, TArray<FStructView>& OutExportData)
{
	if (ExportIndexBegin > ExportIndexEnd)
	{
		return;
	}

	// 先将异步加载的IndexRange取出，放入等待队列。
	TArray<void*> PendingIndexRanges;
	PendingLoadExports_Async.PopAll(PendingIndexRanges);

	// 将同步加载的IndexRange放入加载队列。
	PushToPendingLoadExports(ExportIndexBegin, ExportIndexEnd);

	constexpr int32 PIEInstanceID = INDEX_NONE;
	constexpr int32 Priority = AsyncLoadHighPriority;

	UPackage* Package = GetPackage();

	EObjectFlags ReLoadFlags = RF_Public | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded;

#if WITH_EDITOR
	ReLoadFlags |= RF_NeedLoad;
#endif

	this->ClearFlags(RF_NeedLoad | RF_WasLoaded | RF_LoadCompleted);
	this->SetFlags(ReLoadFlags);

	int32 AsyncLoadRequestID = LoadPackageAsync(Package->GetLoadedPath(), Package->GetFName(), FLoadPackageAsyncDelegate::CreateWeakLambda(this, [this, PendingIndexRanges](const FName&, UPackage*, EAsyncLoadingResult::Type Result) {
		// if (Result != EAsyncLoadingResult::Succeeded)
		// 这里不需要关心是否成果加载完成，因为如果未成功，PendingIndexRanges必然不为空
		
		if (PendingIndexRanges.IsEmpty())
		{
			return;
		}
		// 因为每个AsyncPackage只会存在一个，所以在回调执行时，已经不存在待加载包了，如果PendingIndexRanges不为空，则需要另起请求。
		for (void* Range : PendingIndexRanges)
		{
			PendingLoadExports_Async.Push(Range);
		}

		LoadExports_Async_LoadExports(OVER_EXPORTINDEX, OVER_EXPORTINDEX, LastAsyncLoadPriority);
		
	}), PKG_None, PIEInstanceID, Priority, nullptr, LOAD_NoVerify);

	if (AsyncLoadRequestID != INDEX_NONE)
	{
		// 仅适用于ZenLoader：
		// 在执行Flush后，会等待当前正在执行的AsyncPackage完成，然后尝试加入新的AsyncPackage，当已有AsyncPackage存在时，会跳过创建。
		FlushAsyncLoading(AsyncLoadRequestID);
	}

	this->ClearFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded);
	this->SetFlags(RF_Public | RF_WasLoaded | RF_LoadCompleted);

	{
		TArray<FLoadedConfigVarsData*> LoadedConfigVarsDatas;
		LoadedConfigVarsDatas_Async.PopAll(LoadedConfigVarsDatas);

		FScopeLock ScopeLock(&ExportDataCritical);
		for (FLoadedConfigVarsData* LoadedData : LoadedConfigVarsDatas)
		{
			ExportData[LoadedData->ExportIndex] = MoveTemp(LoadedData->Data);
			delete LoadedData;
		}
	}

	for (int32 Index = ExportIndexBegin; Index <= ExportIndexEnd; ++Index)
	{
		OutExportData.Add(ExportData[Index]);
	}
}

void UConfigVarsLinker::LoadExports_Async_Request(uint16 ExportIndexBegin, uint16 ExportIndexEnd, int32 Priority)
{
	if (ExportIndexBegin > ExportIndexEnd)
	{
		return;
	}

	LoadExports_Async_LoadImports(ExportIndexBegin, ExportIndexEnd, Priority);
}

void UConfigVarsLinker::LoadExports_Async_LoadImports(uint16 ExportIndexBegin, uint16 ExportIndexEnd, int32 Priority)
{
	FLoadPackageAsyncDelegate LoadPackageAsyncDelegate;
	FGuid CounterID;

	CounterID = FGuid::NewGuid();
	LoadPackageAsyncDelegate = FLoadPackageAsyncDelegate::CreateWeakLambda(this, [this, Priority, ExportIndexBegin, ExportIndexEnd, CounterID](const FName&, UPackage*, EAsyncLoadingResult::Type Result)
		{
			// GameThread

			if (Result != EAsyncLoadingResult::Succeeded)
			{
				// 依赖加载失败
				LoadingImportCounter.Remove(CounterID);
				return;
			}

			if (!LoadingImportCounter.Contains(CounterID))
			{
				// 因为某些原因计数器被移除，所以这里不需要再进行下去了。
				return;
			}

			if (LoadingImportCounter[CounterID]-- == 1)
			{
				LoadingImportCounter.Remove(CounterID);
				LoadExports_Async_LoadExports(ExportIndexBegin, ExportIndexEnd, Priority);
			}
		});

	int32 LoadNum = 0;
	for (int32 ExportIndex = ExportIndexBegin; ExportIndex <= ExportIndexEnd; ++ExportIndex)
	{
		FConfigVarsExport& Export = ExportTable[ExportIndex];
		// Class
		if (LoadImport_Async(Export.ClassIndex, LoadPackageAsyncDelegate, Priority) != INDEX_NONE)
		{
			++LoadNum;
		}
		

		// Dependency
		for (FBitArray::FIterator It(Export.ImportSet); It; ++It)
		{
			int32 Index = *It;
			if (LoadImport_Async(Index, LoadPackageAsyncDelegate, Priority) != INDEX_NONE)
			{
				++LoadNum;
			}
		}
	}

	if (LoadNum)
	{
		LoadingImportCounter.Add(CounterID, LoadNum);
	}
	else
	{
		LoadExports_Async_LoadExports(ExportIndexBegin, ExportIndexEnd, Priority);
	}
}

void UConfigVarsLinker::LoadExports_Async_LoadExports(uint16 ExportIndexBegin, uint16 ExportIndexEnd, int32 Priority)
{
	LastAsyncLoadPriority = Priority;

	UPackage* Package = GetPackage();

	AddAsyncLoadFlag();

	constexpr int32 PIEInstanceID = INDEX_NONE;

	PushToPendingLoadExports(ExportIndexBegin, ExportIndexEnd);

	LoadPackageAsync(Package->GetLoadedPath(), Package->GetFName(), FLoadPackageAsyncDelegate::CreateWeakLambda(this, [this](const FName&, UPackage*, EAsyncLoadingResult::Type Result) {
		ClearAsyncLoadFlag();

		if (Result != EAsyncLoadingResult::Succeeded)
		{
			// 加载失败
			return;
		}

		{
			TArray<FLoadedConfigVarsData*> LoadedConfigVarsDatas;
			LoadedConfigVarsDatas_Async.PopAll(LoadedConfigVarsDatas);

			FScopeLock ScopeLock(&ExportDataCritical);
			for (FLoadedConfigVarsData* LoadedData : LoadedConfigVarsDatas)
			{
				ExportData[LoadedData->ExportIndex] = MoveTemp(LoadedData->Data);
				delete LoadedData;
			}
		}

		// 还有未加载完成的数据，可能是在异步加载执行过程中被加入的请求。这里需要进一步处理。
		if (!PendingLoadExports_Async.IsEmpty())
		{
			AddAsyncLoadFlag();

			LoadExports_Async_LoadExports(OVER_EXPORTINDEX, OVER_EXPORTINDEX, LastAsyncLoadPriority);
		}

	}), PKG_None, PIEInstanceID, Priority, nullptr, LOAD_NoVerify);
}

void UConfigVarsLinker::AddAsyncLoadFlag()
{
	EObjectFlags ReLoadFlags = RF_Public | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded;

#if WITH_EDITOR
	ReLoadFlags |= RF_NeedLoad;
#endif
	this->ClearFlags(RF_NeedLoad | RF_WasLoaded | RF_LoadCompleted);
	this->SetFlags(ReLoadFlags);
}

void UConfigVarsLinker::ClearAsyncLoadFlag()
{
	this->ClearFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded);
	this->SetFlags(RF_Public | RF_WasLoaded | RF_LoadCompleted);
}

FStructView UConfigVarsLinker::LoadData(int32 ExportIndex)
{
	if (ExportIndex == INDEX_NONE)
	{
		return FStructView();
	}

	// 第二级，在本身的数组中寻找。
	if (ExportData[ExportIndex].IsValid())
	{
		return ExportData[ExportIndex];
	}

	if (!ExportTable.IsValidIndex(ExportIndex))
	{
		return FStructView();
	}

	FConfigVarsExport& Export = ExportTable[ExportIndex];

	/************************************************************************/
	/* 第四级，反序列化															*/
	/************************************************************************/
	if (Export.ClassIndex != INDEX_NONE)
	{
		// 确保所有依赖已经加载
		LoadImports_Sync(ExportIndex, ExportIndex);

		TArray<FStructView> OutExportData;
		LoadExports_Sync(ExportIndex, ExportIndex, OutExportData);
		if (OutExportData.Num() == 1)
		{
			return OutExportData[0];
		}
	}

	return FStructView();
}

void UConfigVarsLinker::LoadData_Async(int32 ExportIndex, int32 Priority)
{
	if (ExportIndex == INDEX_NONE || !ExportTable.IsValidIndex(ExportIndex))
	{
		return;
	}

	FConfigVarsExport& Export = ExportTable[ExportIndex];

	// 第二级，在本身的数组中寻找。
	if (ExportData[ExportIndex].IsValid())
	{
		return;
	}

	/************************************************************************/
	/* 第四级，反序列化															*/
	/************************************************************************/
	if (Export.ClassIndex != INDEX_NONE)
	{
		LoadExports_Async_Request(ExportIndex, ExportIndex, Priority);
	}
}

void UConfigVarsLinker::LoadData_Multi_Async(int32 BeginExportIndex, int32 EndExportIndex, int32 Priority)
{
	if (BeginExportIndex == INDEX_NONE || !ExportTable.IsValidIndex(BeginExportIndex) || EndExportIndex == INDEX_NONE || !ExportTable.IsValidIndex(EndExportIndex))
	{
		return;
	}


	if (ExportTable[BeginExportIndex].ClassIndex != INDEX_NONE && ExportTable[EndExportIndex].ClassIndex != INDEX_NONE)
	{
		LoadExports_Async_Request(BeginExportIndex, EndExportIndex, Priority);
	}
}

void UConfigVarsLinker::LoadData_Nested_Async(int32 ExportIndex, int32 Priority)
{
	if (ExportIndex == INDEX_NONE || !ExportTable.IsValidIndex(ExportIndex))
	{
		return;
	}

	int32 EndExportIndex = ExportIndex;
	FConfigVarsExport& BeginExport = ExportTable[ExportIndex];
	for (int32 i = ExportIndex + 1; i < ExportTable.Num(); ++i)
	{
		++EndExportIndex;
		if (BeginExport.Depth >= ExportTable[i].Depth)
		{
			--EndExportIndex;
			break;
		}
	}

	if (BeginExport.ClassIndex != INDEX_NONE && ExportTable[EndExportIndex].ClassIndex != INDEX_NONE)
	{
		LoadExports_Async_Request(ExportIndex, EndExportIndex, Priority);
	}
}

UConfigVarsLinkerEditorData* UConfigVarsLinker::GetLinkerEditorData()
{
#if WITH_EDITOR
	if (IsValid(LinkerEditorData))
	{
		return LinkerEditorData;
	}

	LinkerEditorData = NewObject<UConfigVarsLinkerEditorData>(this);
	LinkerEditorData->SetFlags(RF_Transient);
	LinkerEditorData->Rename(nullptr, this, REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);

	return LinkerEditorData;
#else
	return nullptr;
#endif
}

#if WITH_EDITOR
FLinkerLoad* UConfigVarsLinker::CreateLinker_Sync()
{
	UPackage* LinkerRoot = GetPackage();

	FLinkerLoad* Linker = FLinkerLoad::FindExistingLinkerForPackage(LinkerRoot);

	if (!Linker)
	{
		// 这一步存在开销，比仅获取Linker开销高。
		TRefCountPtr<FUObjectSerializeContext> LoadContext(FUObjectThreadContext::Get().GetSerializeContext());
		FPackagePath Path = LinkerRoot->GetLoadedPath();
		Linker = FLinkerLoad::CreateLinker(LoadContext, LinkerRoot, Path, LOAD_NoVerify, nullptr);
	}

	return Linker;
}

FStructView UConfigVarsLinker::LoadOrAddData(FConfigVarsBag& ConfigVarsBag, const UScriptStruct* TemplateDataStruct, UObject* Outermost)
{

	// 如果Outermost有效，则覆盖ConfigVarsBag记录的Outer
	if (IsValid(Outermost))
	{
		ConfigVarsBag.Outermost = Outermost;
	}
	else
	{
		Outermost = ConfigVarsBag.Outermost;
	}

	
	ConfigVarsBag.Linker = this;
	int32& InOutExportIndex = ConfigVarsBag.ExportIndex;

	if (InOutExportIndex != INDEX_NONE)
	{
		// 第二级，在本身的数组中寻找。
		if (ExportData.IsValidIndex(InOutExportIndex) && ExportData[InOutExportIndex].IsValid())
		{
			ExportDataOuter[InOutExportIndex] = Outermost;

			if (TemplateDataStruct && ExportData[InOutExportIndex].GetScriptStruct() != TemplateDataStruct)
			{
				ExportData[InOutExportIndex].InitializeAs(TemplateDataStruct);
			}

			return ExportData[InOutExportIndex];
		}


		if (ExportTable.IsValidIndex(InOutExportIndex))
		{
			FConfigVarsExport& Export = ExportTable[InOutExportIndex];

			/************************************************************************/
			/* 第四级，反序列化															*/
			/************************************************************************/
			if (Export.ClassIndex != INDEX_NONE)
			{
				// 确保所有依赖已经加载
				LoadImports_Sync(InOutExportIndex, InOutExportIndex);

				FConfigVarsImport& Import = ImportTable[Export.ClassIndex];
				UScriptStruct* ExportStruct = Cast<UScriptStruct>(Import.ObjectPath.TryLoad());
				if (ExportStruct)
				{
					ExportData[InOutExportIndex].InitializeAs(ExportStruct);

					FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
					// Set up a load context
					TRefCountPtr<FUObjectSerializeContext> LoadContext = ThreadContext.GetSerializeContext();
					// Try to load.
					BeginLoad(LoadContext, *(GetPackage()->GetName()));
					{
						// 反序列化Object的数据
						FLinkerLoad* LinkerLoad = CreateLinker_Sync();
						check(LinkerLoad);

						((FArchive*)LinkerLoad)->Seek(Export.SerialLocation);

						FConfigVarsUtils::SerializeConfigVars(FStructuredArchiveFromArchive(*LinkerLoad).GetSlot().EnterRecord(), this, ExportData[InOutExportIndex]);
					}
					EndLoad(LoadContext);

					ExportDataOuter[InOutExportIndex] = Outermost;

					if (TemplateDataStruct && ExportData[InOutExportIndex].GetScriptStruct() != TemplateDataStruct)
					{
						ExportData[InOutExportIndex].InitializeAs(TemplateDataStruct);
					}

					return ExportData[InOutExportIndex];
				}
			}
		}
	}

	// 第五级，重新创建
	if (TemplateDataStruct)
	{
		auto GetAvailableExportDataIndex= [this]() -> int32 {
			UConfigVarsLinkerEditorData* EditorData = GetLinkerEditorData();
			if (!EditorData)
			{
				return INDEX_NONE;
			}

			if (!EditorData->PendingRemovedSet.IsEmpty())
			{
				auto FirstIt = EditorData->PendingRemovedSet.CreateIterator();
				int32 AvailableIndex = *FirstIt;
				FirstIt.RemoveCurrent();

				return AvailableIndex;
			}

			return INDEX_NONE;
		};

		InOutExportIndex = GetAvailableExportDataIndex();
		if (InOutExportIndex != INDEX_NONE)
		{
			// 有空余就用空余。
			ExportDataOuter[InOutExportIndex] = Outermost;
			ExportData[InOutExportIndex].InitializeAs(TemplateDataStruct);
			return ExportData[InOutExportIndex];
		}
		else
		{
			// 没有可用的空间，则尝试额外分配。
			ExportData.Emplace(TemplateDataStruct);
			ExportDataOuter.Add(Outermost);

			// Mark the package dirty...
			GetPackage()->MarkPackageDirty();

			InOutExportIndex = ExportData.Num() - 1;
			
			return ExportData.Last();
		}
	}

	return FStructView();
}

void UConfigVarsLinker::ImmediateRemoveData(struct FConfigVarsBag& ConfigVarsBag)
{
	if (ConfigVarsBag.ExportIndex >= 0)
	{
		ExportData[ConfigVarsBag.ExportIndex].Reset();
		ExportDataOuter[ConfigVarsBag.ExportIndex] = nullptr;
	}

	MarkPendingRemoved(ConfigVarsBag.ExportIndex, true);
	ConfigVarsBag.ExportIndex = INDEX_NONE;
}

void UConfigVarsLinker::MarkPendingRemoved(int32 ExportIndex, bool bIsPendingRemoved)
{
	if (GetLinkerEditorData() && ExportIndex >= 0)
	{
		if (bIsPendingRemoved)
		{
			LinkerEditorData->PendingRemovedSet.Add(ExportIndex);

			// 如果新增数据，则原有排序很可能不再紧凑，需要清空。
			LinkerEditorData->ExportDataOrderSet.Empty();
			LinkerEditorData->ExportDataDepthSet.Empty();
		}
		else
		{
			LinkerEditorData->PendingRemovedSet.Remove(ExportIndex);
			ClearFlags(RF_Transient);
			SetFlags(RF_Public | RF_Standalone);
		}

		if (LinkerEditorData->PendingRemovedSet.Num() == ExportData.Num())
		{
			ClearFlags(RF_Public | RF_Standalone);
			SetFlags(RF_Transient);
		}
	}
}

int32 UConfigVarsLinker::GetSerialExportIndex(int32 OldExportIndex)
{
	if (GetLinkerEditorData())
	{
		FSetElementId ElementId = LinkerEditorData->ExportDataOrderSet.FindId(OldExportIndex);
		if (ElementId.IsValidId())
		{
			return ElementId.AsInteger();
		}
		else
		{
			LinkerEditorData->ExportDataOrderSet.Add(OldExportIndex);
			return LinkerEditorData->ExportDataOrderSet.Num() - 1;
		}
	}

	return INDEX_NONE;
}
#endif

//////////////////////////////////////////////////////////////////////////

void FConfigVarsUtils::SerializeConfigVars(FStructuredArchive::FRecord ExportRecord, UConfigVarsLinker* Linker, FStructView ConfigVarsData)
{
	FStructuredArchive::FRecord RealRecord = ExportRecord.EnterField(TEXT("ConfigVarsData")).EnterRecord();

	for (const UStruct* DataStruct = ConfigVarsData.GetScriptStruct(); DataStruct; DataStruct = DataStruct->GetSuperStruct())
	{
		SerializeProperties(RealRecord, Linker, DataStruct, ConfigVarsData.GetMemory());
	}
}

bool FConfigVarsUtils::ShouldSerializeValue(FArchive& Ar, FProperty* Property)
{
	if (!Property->ShouldSerializeValue(Ar))
	{
		return false;
	}
	//FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	//if (StructProperty)
	//{
	//	if (StructProperty->Struct->IsChildOf(FInstancedStruct::StaticStruct()))
	//	{
	//		return true;
	//	}
	//	if (StructProperty->Struct->GetCppStructOps()->HasSerializer())
	//	{
	//		return true;
	//	}

	//}
	
	return true;
}

template<typename SrcType>
void FConfigVarsUtils::SerializeProperties(FStructuredArchive::FRecord ExportRecord, UConfigVarsLinker* Linker, const UStruct* DataStruct, SrcType* SrcData)
{
	if (!DataStruct)
	{
		return;
	}
	FArchive& UnderlyingArchive = ExportRecord.GetUnderlyingArchive();

	FStructuredArchive::FStream PropertiesStream = ExportRecord.EnterStream(*DataStruct->GetName());

	if (UnderlyingArchive.IsSaving())
	{
		int32 SerialPropCount = 0;
		int32 InitialOffset = UnderlyingArchive.Tell();

		UnderlyingArchive << SerialPropCount;

		for (TFieldIterator<FProperty> PropertyIter(DataStruct); PropertyIter; ++PropertyIter)
		{
			FProperty* ChildProperty = *PropertyIter;

			if (!ChildProperty)
			{
				continue;
			}
			if (!ShouldSerializeValue(UnderlyingArchive, ChildProperty))
			{
				ChildProperty = ChildProperty->PropertyLinkNext;
				continue;
			}

			++SerialPropCount;

			FStructuredArchive::FRecord PropertyRecord = PropertiesStream.EnterElement().EnterRecord();
			FArchive& PropertyArchive = PropertyRecord.GetUnderlyingArchive();

			int32 PropertySize = 0;
			FSerialSizeScope Scope(PropertyArchive, PropertySize);

			FName PropertyName = ChildProperty->GetFName();
			FName PropertyType = ChildProperty->GetID();
			PropertyRecord << SA_VALUE(TEXT("PropertyName"), PropertyName);
			PropertyRecord << SA_VALUE(TEXT("PropertyType"), PropertyType);


			// 开始序列化属性值
			SerializeItem(PropertyRecord, ChildProperty, Linker, SrcData);
		}

		int32 FinalOffset = UnderlyingArchive.Tell();

		UnderlyingArchive.Seek(InitialOffset);
		UnderlyingArchive << SerialPropCount;
		UnderlyingArchive.Seek(FinalOffset);
	}
	else if (UnderlyingArchive.IsLoading())
	{
		FProperty* ChildProperty = DataStruct->PropertyLink;

		int32 SerialPropCount;
		UnderlyingArchive << SerialPropCount;

		for (; SerialPropCount; --SerialPropCount)
		{
			FStructuredArchive::FRecord PropertyRecord = PropertiesStream.EnterElement().EnterRecord();
			FArchive& PropertyArchive = PropertyRecord.GetUnderlyingArchive();

			int32 PropertySize = 0;
			FSerialSizeScope Scope(PropertyArchive, PropertySize);

			int32 InitialOffset = PropertyArchive.Tell();

			FName PropertyName;
			FName PropertyType;
			PropertyRecord << SA_VALUE(TEXT("PropertyName"), PropertyName);
			PropertyRecord << SA_VALUE(TEXT("PropertyType"), PropertyType);

			// 处理属性乱序、丢失的情况
			if (ChildProperty == nullptr || ChildProperty->GetFName() != PropertyName)
			{
				FProperty* CurrentProperty = ChildProperty;
				// 向后续继续搜索
				for (; ChildProperty; ChildProperty = ChildProperty->PropertyLinkNext)
				{
					if (ChildProperty->GetFName() == PropertyName)
					{
						break;
					}
				}
				// 从头开始搜索
				if (ChildProperty == nullptr)
				{
					for (ChildProperty = DataStruct->PropertyLink; ChildProperty && ChildProperty != CurrentProperty; ChildProperty = ChildProperty->PropertyLinkNext)
					{
						if (ChildProperty->GetFName() == PropertyName)
						{
							break;
						}
					}

					if (ChildProperty == CurrentProperty)
					{
						ChildProperty = nullptr;
					}
				}

				// 未能正常处理，尝试直接跳过
				if (ChildProperty == nullptr)
				{
					PropertyArchive.Seek(InitialOffset + PropertySize);
					continue;
				}
				else if (ChildProperty->GetID() != PropertyType)
				{
					FString PackageName = TEXT("unkown Package");
					if (Linker->GetPackage())
					{
						Linker->GetPackage()->GetName(PackageName);
					}
					UE_LOG(LogConfigVarsLinker, Warning, TEXT("Type mismatch in %s of %s - Previous (%s) Current(%s) for package:  %s"), *PropertyName.ToString(), *DataStruct->GetName(), *PropertyType.ToString(), *ChildProperty->GetID().ToString(), *PackageName);

					PropertyArchive.Seek(InitialOffset + PropertySize);
					continue;
				}

			}


			// 开始反序列化属性值
			SerializeItem(PropertyRecord, ChildProperty, Linker, SrcData);


			ChildProperty = ChildProperty->PropertyLinkNext;
		}


	}

	SerializeProperties(ExportRecord, Linker, DataStruct->GetSuperStruct(), SrcData);
}

template<typename SrcType>
void FConfigVarsUtils::SerializeItem(FStructuredArchive::FRecord PropertyRecord, FProperty* ChildProperty, UConfigVarsLinker* Linker, SrcType* SrcData)
{
	FArchive& PropertyArchive = PropertyRecord.GetUnderlyingArchive();

	if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ChildProperty))
	{
		bool bIsExportObject = ChildProperty->HasAnyPropertyFlags(CPF_ExportObject);

#if WITH_EDITOR
		static const FName NAME_ForceLazyLoadExportObject = "ConfigVars";
		bool bForceLazyLoadExportObject = ChildProperty->HasMetaData(NAME_ForceLazyLoadExportObject) && bIsExportObject;
#else
		bool bForceLazyLoadExportObject = false;
#endif

		PropertyRecord << SA_VALUE(TEXT("ForceLazyLoadExportObject"), bForceLazyLoadExportObject);

		if (PropertyArchive.IsSaving())
		{
			UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue((uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
			if (!IsValid(ObjectValue))
			{
				ObjectValue = nullptr;
			}

			if (!bIsExportObject)
			{
				// 非ExportObject的依赖Object都会懒加载
				FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);
			}
			else if (bForceLazyLoadExportObject)
			{
				// 强制ExportObject被懒加载
				bool bIsNullExportObject = ObjectValue == nullptr;
				PropertyRecord << SA_VALUE(TEXT("IsNullExportObject"), bIsNullExportObject);
				if (!bIsNullExportObject)
				{
					FName ExportObjectName = ObjectValue->GetFName();
					UClass* ExportObjectClass = ObjectValue->GetClass();
					UObject* ExportObjectOuter = ObjectValue->GetOuter();
					static_assert(sizeof(ObjectValue->GetFlags()) <= sizeof(uint32), "Expect EObjectFlags to be uint32");
					uint32 ExportObjectFlags = (uint32)ObjectValue->GetFlags();

					PropertyRecord << SA_VALUE(TEXT("ExportObjectOuter"), ExportObjectOuter);
					PropertyRecord << SA_VALUE(TEXT("ExportObjectName"), ExportObjectName);
					PropertyRecord << SA_VALUE(TEXT("ExportObjectFlags"), ExportObjectFlags);
					FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ExportObjectClass);
					SerializeProperties(PropertyRecord, Linker, ExportObjectClass, ObjectValue);
				}
			}
			else
			{
				// ExportObject在资产反序列化时会一起被处理
				PropertyRecord << SA_VALUE(TEXT("ExportObject"), ObjectValue);
			}
		}
		else if (PropertyArchive.IsLoading())
		{
			UObject* ObjectValue = nullptr;
			
			if (!bIsExportObject)
			{
				// 非ExportObject的依赖Object都会懒加载
				FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);
			}
			else if (bForceLazyLoadExportObject)
			{
				// 强制ExportObject被懒加载
				bool bIsNullExportObject = false;
				PropertyRecord << SA_VALUE(TEXT("IsNullExportObject"), bIsNullExportObject);
				if (!bIsNullExportObject)
				{
					FName ExportObjectName = NAME_None;
					UClass* ExportObjectClass = nullptr;
					UObject* ExportObjectOuter = nullptr;
					uint32 ExportObjectFlags = RF_NoFlags;

					PropertyRecord << SA_VALUE(TEXT("ExportObjectOuter"), ExportObjectOuter);
					PropertyRecord << SA_VALUE(TEXT("ExportObjectName"), ExportObjectName);
					PropertyRecord << SA_VALUE(TEXT("ExportObjectFlags"), ExportObjectFlags);
					FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ExportObjectClass);

					FStaticConstructObjectParameters Params(ExportObjectClass);
					Params.Outer = ExportObjectOuter;
					Params.Name = ExportObjectName;
					Params.SetFlags = (EObjectFlags)ExportObjectFlags;
					ObjectValue = StaticConstructObject_Internal(Params);
					SerializeProperties(PropertyRecord, Linker, ExportObjectClass, ObjectValue);
				}
			}
			else
			{
				// 在资产反序列化时会一起被处理
				PropertyRecord << SA_VALUE(TEXT("ExportObject"), ObjectValue);
			}

			ObjectProperty->SetObjectPropertyValue((uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), ObjectValue);
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(ChildProperty))
	{
		if (StructProperty->Struct->GetCppStructOps()->HasSerializer())
		{
//////////////////////////////////////////////////////////////////////////
// 主动调用它的序列化函数
			StructProperty->Struct->GetCppStructOps()->Serialize(PropertyArchive, (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
//////////////////////////////////////////////////////////////////////////
		}
		else
		{
			SerializeProperties(PropertyRecord, Linker, StructProperty->Struct, (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ChildProperty))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(SrcData));
		int32 Num = ArrayHelper.Num();
		PropertyRecord << SA_VALUE(TEXT("ArrayNum"), Num);

		if (FObjectProperty* ItemObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
		{
			if (PropertyArchive.IsSaving())
			{
				for (int32 Index = 0; Index < Num; ++Index)
				{
					UObject* ObjectValue = ItemObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
					if (!IsValid(ObjectValue))
					{
						ObjectValue = nullptr;
					}

					FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);
				}
			}
			else if (PropertyArchive.IsLoading())
			{
				ArrayHelper.EmptyValues(Num);

				for (; Num; --Num)
				{
					UObject* ObjectValue = nullptr;
					FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);

					int32 Index = ArrayHelper.AddUninitializedValue();
					ItemObjectProperty->SetObjectPropertyValue(ArrayHelper.GetRawPtr(Index), ObjectValue);
				}
			}
		}
		else
		{
			if (PropertyArchive.IsSaving())
			{
				for (int32 Index = 0; Index < Num; ++Index)
				{
					SerializeItem(PropertyRecord, ArrayProperty->Inner, Linker, ArrayHelper.GetRawPtr(Index));
				}
			}
			else if (PropertyArchive.IsLoading())
			{
				ArrayHelper.EmptyValues(Num);

				for (; Num; --Num)
				{
					int32 Index = ArrayHelper.AddUninitializedValue();
					SerializeItem(PropertyRecord, ArrayProperty->Inner, Linker, ArrayHelper.GetRawPtr(Index));
				}
			}

			//ArrayProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
		}
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(ChildProperty))
	{
		FProperty* KeyProperty = MapProperty->KeyProp;
		FProperty* ValueProperty = MapProperty->ValueProp;
		FObjectProperty* KeyObjectProperty = CastField<FObjectProperty>(MapProperty->KeyProp);
		FObjectProperty* ValueObjectProperty = CastField<FObjectProperty>(MapProperty->ValueProp);

		FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(SrcData));
		int32 Num = MapHelper.Num();
		FStructuredArchive::FArray EntriesArray = PropertyRecord.EnterArray(TEXT("Entries"), Num);

		if (KeyObjectProperty || ValueObjectProperty)
		{
			if (PropertyArchive.IsSaving())
			{
				// Map 是稀疏数组，必须判断Index有效性
				for (int32 Index = 0; Num; ++Index)
				{
					if (MapHelper.IsValidIndex(Index))
					{
						FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();

						uint8* MapKeyData = MapHelper.GetKeyPtr(Index);
						uint8* MapValueData = MapHelper.GetValuePtr(Index);

						if (KeyObjectProperty)
						{
							UObject* ObjectValue = KeyObjectProperty->GetObjectPropertyValue(MapKeyData);
							if (!IsValid(ObjectValue))
							{
								ObjectValue = nullptr;
							}

							FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);
						}
						else
						{
							FSerializedPropertyScope SerializedProperty(PropertyArchive, KeyProperty, MapProperty);
							KeyProperty->SerializeItem(EntryRecord.EnterField(TEXT("Key")), MapKeyData, nullptr);
						}

						if (ValueObjectProperty)
						{
							UObject* ObjectValue = ValueObjectProperty->GetObjectPropertyValue(MapValueData);
							if (!IsValid(ObjectValue))
							{
								ObjectValue = nullptr;
							}

							FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);
						}
						else
						{
							FSerializedPropertyScope SerializedProperty(PropertyArchive, ValueProperty, MapProperty);
							ValueProperty->SerializeItem(EntryRecord.EnterField(TEXT("Value")), MapValueData, nullptr);
						}

						--Num;
					}
				}
			}
			else if (PropertyArchive.IsLoading())
			{
				MapHelper.EmptyValues(Num);

				for (; Num; --Num)
				{
					FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();
					int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					uint8* MapKeyData = MapHelper.GetKeyPtr(Index);
					uint8* MapValueData = MapHelper.GetValuePtr(Index);

					if (KeyObjectProperty)
					{
						UObject* ObjectValue = nullptr;
						FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);

						KeyObjectProperty->SetObjectPropertyValue(MapKeyData, ObjectValue);
					}
					else
					{
						FSerializedPropertyScope SerializedProperty(PropertyArchive, KeyProperty, MapProperty);
						KeyProperty->SerializeItem(EntryRecord.EnterField(TEXT("Key")), MapKeyData, nullptr);
					}


					if (ValueObjectProperty)
					{
						UObject* ObjectValue = nullptr;
						FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);

						ValueObjectProperty->SetObjectPropertyValue(MapValueData, ObjectValue);
					}
					else
					{
						FSerializedPropertyScope SerializedProperty(PropertyArchive, ValueProperty, MapProperty);
						ValueProperty->SerializeItem(EntryRecord.EnterField(TEXT("Value")), MapValueData, nullptr);
					}
				}

				MapHelper.Rehash();
			}
		}
		else
		{
			// FConfigVars不能作为Key键
			// FProperty* KeyProperty = MapProperty->KeyProp;

			if (PropertyArchive.IsSaving())
			{
				for (int32 Index = 0; Num; ++Index)
				{
					// Map 是稀疏数组，必须判断Index有效性
					if (MapHelper.IsValidIndex(Index))
					{
						FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();

						uint8* MapKeyData = MapHelper.GetKeyPtr(Index);
						uint8* MapValueData = MapHelper.GetValuePtr(Index);

						FSerializedPropertyScope SerializedProperty(PropertyArchive, KeyProperty, MapProperty);
						KeyProperty->SerializeItem(EntryRecord.EnterField(TEXT("Key")), MapKeyData, nullptr);

						SerializeItem(PropertyRecord, ValueProperty, Linker, MapValueData);
					}
				}
			}
			else if (PropertyArchive.IsLoading())
			{
				MapHelper.EmptyValues(Num);
				for (; Num; --Num)
				{
					FStructuredArchive::FRecord EntryRecord = EntriesArray.EnterElement().EnterRecord();

					int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					uint8* MapKeyData = MapHelper.GetKeyPtr(Index);
					uint8* MapValueData = MapHelper.GetValuePtr(Index);


					FSerializedPropertyScope SerializedProperty(PropertyArchive, KeyProperty, MapProperty);
					KeyProperty->SerializeItem(EntryRecord.EnterField(TEXT("Key")), MapKeyData, nullptr);

					SerializeItem(PropertyRecord, ValueProperty, Linker, MapValueData);
				}
			}

			//MapProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
		}
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(ChildProperty))
	{
		FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(SrcData));
		// 这里用Num而不是MaxIndex，可以减少遍历数量
		int32 Num = SetHelper.Num();
		FStructuredArchive::FArray ElementsArray = PropertyRecord.EnterArray(TEXT("Elements"), Num);

		if (FObjectProperty* ItemObjectProperty = CastField<FObjectProperty>(SetProperty->ElementProp))
		{
			if (PropertyArchive.IsSaving())
			{
				FSerializedPropertyScope SerializedProperty(PropertyArchive, ItemObjectProperty, SetProperty);

				// Set 是稀疏数组，必须判断Index有效性
				for (int32 Index = 0; Num; ++Index)
				{
					if (SetHelper.IsValidIndex(Index))
					{
						UObject* ObjectValue = ItemObjectProperty->GetObjectPropertyValue(SetHelper.GetElementPtr(Index));

						if (!IsValid(ObjectValue))
						{
							ObjectValue = nullptr;
						}

						FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);

						--Num;
					}
				}

				SetHelper.Rehash();

			}
			else if (PropertyArchive.IsLoading())
			{
				FSerializedPropertyScope SerializedProperty(PropertyArchive, ItemObjectProperty, SetProperty);

				SetHelper.EmptyElements(Num);

				for (; Num; --Num)
				{
					UObject* ObjectValue = nullptr;
					FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, ObjectValue);

					int32 Index = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
					ItemObjectProperty->SetObjectPropertyValue(SetHelper.GetElementPtr(Index), ObjectValue);
				}
			}
		}
		else
		{
			if (PropertyArchive.IsSaving())
			{
				// Set 是稀疏数组，必须判断Index有效性
				for (int32 Index = 0; Num; ++Index)
				{
					if (SetHelper.IsValidIndex(Index))
					{
						SerializeItem(PropertyRecord, SetProperty->ElementProp, Linker, SetHelper.GetElementPtr(Index));
					}
				}
			}
			else if (PropertyArchive.IsLoading())
			{
				FSerializedPropertyScope SerializedProperty(PropertyArchive, ItemObjectProperty, SetProperty);

				SetHelper.EmptyElements(Num);

				for (; Num; --Num)
				{
					int32 Index = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
					SerializeItem(PropertyRecord, SetProperty->ElementProp, Linker, SetHelper.GetElementPtr(Index));
				}
			}

			// SetProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
		}
	}
	else
	{
		FSerializedPropertyScope SerializedProperty(PropertyArchive, ChildProperty);
		ChildProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
	}
}

template<typename SrcType>
void FConfigVarsUtils::VerifyNestedDataStruct(FArchive& Ar, UConfigVarsLinker* Linker, const UStruct* DataStruct, SrcType* SrcData, int32 Depth)
{
	if (!DataStruct || !SrcData)
	{
		return;
	}

	check(Ar.IsSaving());

	for (TFieldIterator<FProperty> PropertyIter(DataStruct); PropertyIter; ++PropertyIter)
	{
		FProperty* ChildProperty = *PropertyIter;
		VerifyNestedDataProperties(Ar, Linker, ChildProperty, SrcData, Depth);
	}
}

template<typename SrcType>
void FConfigVarsUtils::VerifyNestedDataProperties(FArchive& Ar, UConfigVarsLinker* Linker, FProperty* ChildProperty, SrcType* SrcData, int32 Depth)
{
	if (!ChildProperty)
	{
		return;
	}

	check(Ar.IsSaving());

	++Depth;

	if (FStructProperty* StructProperty = CastField<FStructProperty>(ChildProperty))
	{
		const UScriptStruct* ScriptStruct = StructProperty->Struct;
		if (ScriptStruct->IsChildOf(FConfigVarsBag::StaticStruct()))
		{
			FConfigVarsBag* ConfigVarsBag = StructProperty->ContainerPtrToValuePtr<FConfigVarsBag>((void*)SrcData);
			UConfigVarsLinkerEditorData* EditorData = Linker->GetLinkerEditorData();

			// 二级及以上的ConfigVars的ExportIndex都还未被映射

			int32 ExportIndex = PRIVATE_GET_VAR(ConfigVarsBag, ExportIndex);

			// 更新适配了嵌套后的数据Depth和SerializeOrderExportIndex

			EditorData->TopOrderSet.Remove(ExportIndex);

			EditorData->ExportDataDepthSet.Add(Depth);
			EditorData->ExportDataOrderSet.Add(ExportIndex);
			EditorData->ExportDataSerializeOrderSet.Add(ExportIndex);

			if (!Linker->ExportData.IsValidIndex(ExportIndex) || !Linker->ExportData[ExportIndex].IsValid())
			{
				return;
			}
			VerifyNestedDataStruct(Ar, Linker, Linker->ExportData[ExportIndex].GetScriptStruct(), Linker->ExportData[ExportIndex].GetMutableMemory(), Depth);
		}
		else if (ScriptStruct->IsChildOf(FInstancedStruct::StaticStruct()))
		{
			//////////////////////////////////////////////////////////////////////////
			// FInstancedStruct 特殊处理
			FInstancedStruct* InstancedStruct = StructProperty->ContainerPtrToValuePtr<FInstancedStruct>((void*)SrcData);
			VerifyNestedDataStruct(Ar, Linker, InstancedStruct->GetScriptStruct(), InstancedStruct->GetMutableMemory(), Depth);
			//////////////////////////////////////////////////////////////////////////
		}
		else
		{
			VerifyNestedDataStruct(Ar, Linker, ScriptStruct, (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), Depth);
		}
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ChildProperty))
	{
		static const FName NAME_ForceLazyLoadExportObject = "ConfigVars";

		bool bIsExportObject = ChildProperty->HasAnyPropertyFlags(CPF_ExportObject);
		bool bForceLazyLoadExportObject = ChildProperty->HasMetaData(NAME_ForceLazyLoadExportObject) && bIsExportObject;
		if (!bForceLazyLoadExportObject)
		{
			return;
		}
		UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue((uint8*)SrcData + ChildProperty->GetOffset_ForInternal());

		if (IsValid(ObjectValue))
		{
			VerifyNestedDataStruct(Ar, Linker, ObjectValue->GetClass(), ObjectValue, Depth);
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ChildProperty))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(SrcData));
		int32 Num = ArrayHelper.Num();

		for (int32 Index = 0; Index < Num; ++Index)
		{
			VerifyNestedDataProperties(Ar, Linker, ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index), Depth);
		}
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(ChildProperty))
	{
		// FConfigVars不能作为Key键
		// FProperty* KeyProperty = MapProperty->KeyProp;
		FProperty* ValueProperty = MapProperty->ValueProp;

		FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(SrcData));
		int32 Num = MapHelper.Num();
		for (int32 Index = 0; Num; ++Index)
		{
			// Map 是稀疏数组，必须判断Index有效性
			if (MapHelper.IsValidIndex(Index))
			{
				//uint8* MapKeyData = MapHelper.GetKeyPtr(Index);
				uint8* MapValueData = MapHelper.GetValuePtr(Index);
				VerifyNestedDataProperties(Ar, Linker, ValueProperty, MapValueData, Depth);
			}
		}
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(ChildProperty))
	{
		FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(SrcData));
		int32 Num = SetHelper.Num();
		// Set 是稀疏数组，必须判断Index有效性
		for (int32 Index = 0; Num; ++Index)
		{
			if (SetHelper.IsValidIndex(Index))
			{	
				VerifyNestedDataProperties(Ar, Linker, SetProperty->ElementProp, SetHelper.GetElementPtr(Index), Depth);
			}
		}
	}
}