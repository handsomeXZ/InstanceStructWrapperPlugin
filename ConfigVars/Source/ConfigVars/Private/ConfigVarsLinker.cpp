#include "ConfigVarsLinker.h"

#include "HAL/FileManagerGeneric.h"

#include "PrivateAccessor.h"
#include "ConfigVarsTypes.h"

#include "ConfigVarsLinkerEditorData.h"

PRIVATE_DEFINE_VAR(FLinkerLoad, TOptional<FStructuredArchive::FRecord>, StructuredArchiveRootRecord);

DEFINE_LOG_CATEGORY_STATIC(LogConfigVarsLinker, Log, All);

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
	Ar << Export.ImportSet;

	return Ar;
}

//////////////////////////////////////////////////////////////////////////
void UConfigVarsLinker::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.IsSaving())
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			//return;
		}

		// 只有FLinkerLoad or FLinkerSave可以取到Linker。
		// 而FPackageHarvester不是我们的真正FileWriter的Ar，仅仅是记录一些额外的信息，例如引用到的FName。
		if (!Ar.GetLinker() || Ar.IsCooking())
		{
			VerifyPendingRemovedExport();
			VerifyAllExportLoaded();
		}

		SerializeHeadData(Record);
		SerializeExportData(Record);
		SerializeTableData(Record);

	}
	else if (Ar.IsLoading())
	{
		if (PendingLoadExports_Async.IsEmpty())
		{
			// 仅编辑器的同步加载 LoadOrAddData() 会走这里
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

		ImportTable.Empty();
		ExportTable.Empty();

		int32 ExportObjectsNum = 0;

		EditorData->ExportDataSerializeOrderSet.Empty();

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

	TArray<void*> PendingIndexs;
	PendingLoadExports_Async.PopAll(PendingIndexs);
	for (void* Index_C : PendingIndexs)
	{
#pragma warning(disable: 4311)
		// 传入时即为int32，所以可以直接无视warning
		int32 ExportIndex = reinterpret_cast<int64>(Index_C) - 1;
#pragma warning(default: 4311)
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

	//////////////////////////////////////////////////////////////////////////

	// 跳过ExportData的序列化
	Ar.Seek(InitialOffset + ExportDataSize);

	// 跳过TableData的序列化
	Ar << TableDataSize;
	Ar.Seek(Ar.Tell() + TableDataSize);


}

int32 UConfigVarsLinker::ImportObject(class UObject* ImportObj)
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
	FArchive& Ar = Record.GetUnderlyingArchive();

	int32 InitialImportNum = ImportTable.Num();

	int32 InitialOffset = Ar.Tell();
	{
		FConfigVarsUtils::SerializeConfigVars(Record, this, StructData);
	}

	int32 FinalImportNum = ImportTable.Num();

	FConfigVarsExport& Export = ExportTable.AddDefaulted_GetRef();
	Export.SerialLocation = InitialOffset;
	Export.ClassIndex = ImportTable.Num();

	if (FinalImportNum > InitialImportNum)
	{
		Export.ImportSet.Reset(FinalImportNum);
		Export.ImportSet.AddRange(InitialImportNum, FinalImportNum - 1);
	}

	ImportTable.Emplace(StructData.GetScriptStruct());

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
			LoadOrAddData(Bag, nullptr);
#else
			// LoadData 会走异步加载，Cook时不能使用
			LoadData(Index);
#endif
		}
	}
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

void UConfigVarsLinker::LoadImports_Sync(TArray<int32> ExportIndexs)
{
	TArray<int32> AsyncLoadRequestIDs;

	for (int32 ExportIndex : ExportIndexs)
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

int32 UConfigVarsLinker::LoadImport_Async(int32 ExportIndex, FLoadPackageAsyncDelegate CallBack, int32 Priority /* = DefaultAsyncLoadPriority */)
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
	return LoadPackageAsync(Import.ObjectPath.GetAssetPath().GetPackageName().ToString(), CallBack, Priority, PKG_None, PIEInstanceID);
}

void UConfigVarsLinker::PushToPendingLoadExports(const TArray<int32>& ExportIndexs)
{
	for (auto Index : ExportIndexs)
	{
		// 不需要每次都new一个对象，直接将Index转指针就行。但必须 +1，因为 0 == NULL
#pragma warning(disable: 4312)
		PendingLoadExports_Async.Push(reinterpret_cast<void*>(Index + 1));
#pragma warning(default: 4312)
	}
}

void UConfigVarsLinker::LoadExports_Sync(TArray<int32> ExportIndexs, TArray<FStructView>& OutExportData)
{
	if (ExportIndexs.IsEmpty())
	{
		return;
	}

	PushToPendingLoadExports(ExportIndexs);

	constexpr int32 PIEInstanceID = INDEX_NONE;
	constexpr int32 Priority = AsyncLoadHighPriority;

	UPackage* Package = GetPackage();

	EObjectFlags ReLoadFlags = RF_Public | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded;

#if WITH_EDITOR
	ReLoadFlags |= RF_NeedLoad;
#endif

	this->ClearFlags(RF_NeedLoad | RF_WasLoaded | RF_LoadCompleted);
	this->SetFlags(ReLoadFlags);

	int32 AsyncLoadRequestID = LoadPackageAsync(Package->GetLoadedPath(), Package->GetFName(), FLoadPackageAsyncDelegate(), PKG_None, PIEInstanceID, Priority, nullptr, LOAD_NoVerify);

	if (AsyncLoadRequestID != INDEX_NONE)
	{
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

	for (int32 Index : ExportIndexs)
	{
		OutExportData.Add(ExportData[Index]);
	}
}

void UConfigVarsLinker::LoadExports_Async_Request(TArray<int32> ExportIndexs, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority)
{
	if (ExportIndexs.IsEmpty())
	{
		TArray<FStructView> NullData;
		CallBack.ExecuteIfBound(NullData);
		return;
	}

	PushToPendingLoadExports(ExportIndexs);

	LoadExports_Async_LoadImports(ExportIndexs, CallBack, Priority);
}

void UConfigVarsLinker::LoadExports_Async_LoadImports(TArray<int32> ExportIndexs, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority)
{
	FLoadPackageAsyncDelegate LoadPackageAsyncDelegate;
	FGuid CounterID;

	CounterID = FGuid::NewGuid();
	LoadPackageAsyncDelegate = FLoadPackageAsyncDelegate::CreateWeakLambda(this, [this, Priority, ExportIndexs, CounterID, CallBack](const FName&, UPackage*, EAsyncLoadingResult::Type Result)
		{
			// GameThread

			if (Result != EAsyncLoadingResult::Succeeded)
			{
				// 依赖加载失败
				LoadingImportCounter.Remove(CounterID);
				TArray<FStructView> NullData;
				CallBack.ExecuteIfBound(NullData);
				return;
			}

			if (!LoadingImportCounter.Contains(CounterID))
			{
				// 因为某些原因计数器被移除，所以这里不需要再进行下去了。
				return;
			}

			if (LoadingImportCounter[CounterID]-- == 1)
			{
				LoadExports_Async_LoadExports(ExportIndexs, CallBack, Priority);
			}
		});

	int32 LoadNum = 0;
	for (int32 ExportIndex : ExportIndexs)
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
		LoadExports_Async_LoadExports(ExportIndexs, CallBack, Priority);
	}
}

void UConfigVarsLinker::LoadExports_Async_LoadExports(TArray<int32> ExportIndexs, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority)
{
	UPackage* Package = GetPackage();
	EObjectFlags ReLoadFlags = RF_Public | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded;

#if WITH_EDITOR
	ReLoadFlags |= RF_NeedLoad;
#endif
	this->ClearFlags(RF_NeedLoad | RF_WasLoaded | RF_LoadCompleted);
	this->SetFlags(ReLoadFlags);

	constexpr int32 PIEInstanceID = INDEX_NONE;

	LoadPackageAsync(Package->GetLoadedPath(), Package->GetFName(), FLoadPackageAsyncDelegate::CreateWeakLambda(this, [this, ExportIndexs, CallBack](const FName&, UPackage*, EAsyncLoadingResult::Type Result) {
		TArray<FStructView> OutExportData;

		this->ClearFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded);
		this->SetFlags(RF_Public | RF_WasLoaded | RF_LoadCompleted);

		if (Result != EAsyncLoadingResult::Succeeded)
		{
			// 加载失败
			CallBack.ExecuteIfBound(OutExportData);
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

		OutExportData.Empty(ExportIndexs.Num());

		for (int32 Index : ExportIndexs)
		{
			OutExportData.Add(ExportData[Index]);
		}
		CallBack.ExecuteIfBound(OutExportData);

	}), PKG_None, PIEInstanceID, Priority, nullptr, LOAD_NoVerify);
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
		LoadImports_Sync({ ExportIndex });

		TArray<FStructView> OutExportData;
		LoadExports_Sync({ ExportIndex }, OutExportData);
		if (OutExportData.Num() == 1)
		{
			return OutExportData[0];
		}
	}

	return FStructView();
}

void UConfigVarsLinker::LoadData_Async(int32 ExportIndex, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority)
{

	if (ExportIndex == INDEX_NONE || !ExportTable.IsValidIndex(ExportIndex))
	{
		TArray<FStructView> NullData;
		CallBack.ExecuteIfBound(NullData);
		return;
	}

	FConfigVarsExport& Export = ExportTable[ExportIndex];

	// 第二级，在本身的数组中寻找。
	if (ExportData[ExportIndex].IsValid())
	{
		CallBack.ExecuteIfBound({ FStructView(ExportData[ExportIndex])});
		return;
	}

	/************************************************************************/
	/* 第四级，反序列化															*/
	/************************************************************************/
	if (Export.ClassIndex != INDEX_NONE)
	{
		LoadExports_Async_Request({ ExportIndex }, CallBack, Priority);
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

FStructView UConfigVarsLinker::LoadOrAddData(FConfigVarsBag& ConfigVarsBag, const UScriptStruct* TemplateDataStruct)
{
	ConfigVarsBag.Outermost = GetPackage();
	ConfigVarsBag.Linker = this;
	int32& InOutExportIndex = ConfigVarsBag.ExportIndex;

	if (InOutExportIndex != INDEX_NONE)
	{
		// 第二级，在本身的数组中寻找。
		if (ExportData[InOutExportIndex].IsValid())
		{
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
				LoadImports_Sync({ InOutExportIndex });

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
				int32 AvailableIndex = *(FirstIt);
				FirstIt.RemoveCurrent();

				return AvailableIndex;
			}

			return INDEX_NONE;
		};

		InOutExportIndex = GetAvailableExportDataIndex();
		if (InOutExportIndex != INDEX_NONE)
		{
			// 有空余就用空余。
			ExportData[InOutExportIndex].InitializeAs(TemplateDataStruct);
			return ExportData[InOutExportIndex];
		}
		else
		{
			// 没有可用的空间，则尝试额外分配。
			ExportData.Emplace(TemplateDataStruct);

			// Mark the package dirty...
			GetPackage()->MarkPackageDirty();

			InOutExportIndex = ExportData.Num() - 1;

			return ExportData.Last();
		}
	}

	return FStructView();
}

void UConfigVarsLinker::MarkPendingRemoved(int32 ExportIndex, bool bIsPendingRemoved)
{
	if (GetLinkerEditorData())
	{
		if (bIsPendingRemoved)
		{
			LinkerEditorData->PendingRemovedSet.Add(ExportIndex);

			// 如果新增数据，则原有排序很可能不再紧凑，需要清空。
			LinkerEditorData->ExportDataOrderSet.Empty();
		}
		else
		{
			LinkerEditorData->PendingRemovedSet.Remove(ExportIndex);
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
	FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	if (StructProperty)
	{
		if (StructProperty->Struct->IsChildOf(FInstancedStruct::StaticStruct()))
		{
			return true;
		}
		if (StructProperty->Struct->GetCppStructOps()->HasSerializer())
		{
			return false;
		}

	}
	
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
				break;
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
					UE_LOG(LogConfigVarsLinker, Warning, TEXT("Type mismatch in %s of %s - Previous (%s) Current(%s) for package:  %s"), *PropertyName.ToString(), *DataStruct->GetName(), *PropertyType.ToString(), *ChildProperty->GetID().ToString());

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
		static const FName NAME_ForceLazyLoadExportObject = "ConfigVars";
		bool bIsExportObject = ChildProperty->HasAnyPropertyFlags(CPF_ExportObject);
		bool bForceLazyLoadExportObject = ChildProperty->HasMetaData(NAME_ForceLazyLoadExportObject) && bIsExportObject;

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
		if (StructProperty->Struct->IsChildOf(FInstancedStruct::StaticStruct()))
		{
//////////////////////////////////////////////////////////////////////////
// FInstancedStruct 特殊处理
			UScriptStruct* DataStruct = nullptr;
			FInstancedStruct* InstancedStruct = StructProperty->ContainerPtrToValuePtr<FInstancedStruct>((void*)SrcData);
			DataStruct = const_cast<UScriptStruct*>(InstancedStruct->GetScriptStruct());
			FConfigVarsUtils::SerializeObject(PropertyRecord, Linker, DataStruct);

			if (PropertyArchive.IsSaving())
			{
				SerializeProperties(PropertyRecord, Linker, DataStruct, InstancedStruct->GetMemory());
			}
			else if (PropertyArchive.IsLoading())
			{
				InstancedStruct->InitializeAs(DataStruct);
				SerializeProperties(PropertyRecord, Linker, DataStruct, InstancedStruct->GetMemory());
			}
//////////////////////////////////////////////////////////////////////////
		}
		else
		{
			SerializeProperties(PropertyRecord, Linker, StructProperty->Struct, (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ChildProperty))
	{
		if (FObjectProperty* ItemObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(SrcData));

			if (PropertyArchive.IsSaving())
			{
				int32 Num = ArrayHelper.Num();
				PropertyRecord << SA_VALUE(TEXT("ArrayNum"), Num);

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
				int32 Num = 0;
				PropertyRecord << SA_VALUE(TEXT("ArrayNum"), Num);
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
			ArrayProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
		}
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(ChildProperty))
	{
		FProperty* KeyProperty = MapProperty->KeyProp;
		FProperty* ValueProperty = MapProperty->ValueProp;
		FObjectProperty* KeyObjectProperty = CastField<FObjectProperty>(MapProperty->KeyProp);
		FObjectProperty* ValueObjectProperty = CastField<FObjectProperty>(MapProperty->ValueProp);
		if (KeyObjectProperty || ValueObjectProperty)
		{
			FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(SrcData));

			if (PropertyArchive.IsSaving())
			{
				int32 Num = MapHelper.Num();
				FStructuredArchive::FArray EntriesArray = PropertyRecord.EnterArray(TEXT("Entries"), Num);

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
				int32 Num = 0;
				FStructuredArchive::FArray EntriesArray = PropertyRecord.EnterArray(TEXT("Entries"), Num);

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
			MapProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
		}
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(ChildProperty))
	{
		if (FObjectProperty* ItemObjectProperty = CastField<FObjectProperty>(SetProperty->ElementProp))
		{
			FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(SrcData));

			if (PropertyArchive.IsSaving())
			{
				// 这里用Num而不是MaxIndex，可以减少遍历数量
				int32 Num = SetHelper.Num();
				FStructuredArchive::FArray ElementsArray = PropertyRecord.EnterArray(TEXT("Elements"), Num);

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
				int32 Num = 0;
				FStructuredArchive::FArray ElementsArray = PropertyRecord.EnterArray(TEXT("Elements"), Num);
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
			SetProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
		}
	}
	else
	{
		FSerializedPropertyScope SerializedProperty(PropertyArchive, ChildProperty);
		ChildProperty->SerializeItem(FStructuredArchiveFromArchive(PropertyArchive).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
	}
}