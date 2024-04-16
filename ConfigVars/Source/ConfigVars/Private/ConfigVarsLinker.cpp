#include "ConfigVarsLinker.h"

#include "PrivateAccessor.h"
#include "HAL/FileManagerGeneric.h"

struct FSerialSizeScope
{
	FSerialSizeScope(FArchive& Ar)
		: HeadOffset(Ar.Tell())
		, Archive(Ar)
	{
		int32 SerialSize = 0;
		Archive << SerialSize;	// 先占位

		InitialOffset = Archive.Tell();
	}
	~FSerialSizeScope()
	{
		const int64 FinalOffset = Archive.Tell();

		Archive.Seek(HeadOffset);	// 覆写占位数据
		int32 SerialSize = (int32)(FinalOffset - InitialOffset);
		Archive << SerialSize;
		Archive.Seek(FinalOffset);	// 还原偏移
	}

private:
	int64 HeadOffset;
	int64 InitialOffset;
	FArchive& Archive;
};

FArchiveConfigVars::FArchiveConfigVars(FArchive& Ar, UConfigVarsLinker* Linker, bool bIsLoading, bool bIsSaving)
	: RealArchive(Ar)
	, ConfigVarsLinker(Linker)
{
	SetIsLoading(bIsLoading);
	SetIsSaving(bIsSaving);
}

FArchive& FArchiveConfigVars::operator <<(UObject*& Obj)
{
	if (RealArchive.IsSaving())
	{
		if (IsValid(Obj))
		{
			int32 ImportIndex = ConfigVarsLinker->ImportObject(Obj);
			RealArchive << ImportIndex;
		}
		else
		{
			int32 NullImportIndex = -1;
			RealArchive << NullImportIndex;
		}
	}
	else if (RealArchive.IsLoading())
	{
		int32 ImportIndex = -1;
		RealArchive << ImportIndex;

		Obj = nullptr;
		FConfigVarsImport& Import = ConfigVarsLinker->ImportTable[ImportIndex];

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

	return *this;
}

FArchive& operator<<(FArchive& Ar, FConfigVarsImport& Import)
{
	Ar << Import.ObjectPath;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FConfigVarsExport& Export)
{
	Ar << Export.ObjectName;
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
		if (Ar.GetLinker() || Ar.IsCooking())
		{
			VerifyAllExportLoaded();
		}

		ImportTable.Empty();
		ExportTable.Empty();
	
		int32 ExportObjectsNum = ExportObjects.Num();
		Ar << ExportObjectsNum;

		int32 InitialLocation = Ar.Tell();
		Ar << InitialLocation;

		{
			FSerialSizeScope Scope(Ar);	// SerialSize

			for (UConfigVarsData* ExportObj : ExportObjects)
			{
				//if (IsValid(ExportObj))
				{
					ExportObject(Record, ExportObj);
				}
			}
		}

		{
			FSerialSizeScope Scope(Ar);	// ImportExportSize
			Ar << ImportTable;
			Ar << ExportTable;
		}

	}
	else if (Ar.IsLoading())
	{
		int32 ExportObjectsNum = 0;
		Ar << ExportObjectsNum;

		if (PendingLoadExports.IsEmpty())
		{
			// 常规反序列化流程
			ExportObjects.SetNum(ExportObjectsNum);

			int32 InitialLocation = 0;
			Ar << InitialLocation;

			int32 SerialSize = 0;
			Ar << SerialSize;

			// 跳过这部分数据的反序列化
			//FArchiveFileReaderGeneric& FileReader = static_cast<FArchiveFileReaderGeneric&>(Ar.GetLoader());
			Ar.Seek(Ar.Tell() + SerialSize);

			Ar << SerialSize;

			Ar << ImportTable;
			Ar << ExportTable;
		}
		else
		{
			// Export 序列化流程
			int32 RealLocation = Ar.Tell();

			int32 InitialLocation = 0;
			Ar << InitialLocation;

			int32 SerializeHeadOffset = InitialLocation - RealLocation;

			int32 SerialSize = 0;
			int32 ImportExportSize = 0;

			Ar << SerialSize;

			int32 InitialOffset = Ar.Tell();

			for (int32 Index : PendingLoadExports)
			{
				if (!ExportObjects[Index])
				{
					SerializeExport(Record, Index, SerializeHeadOffset);
				}
			}
			PendingLoadExports.Empty();

			Ar.Seek(InitialOffset + SerialSize);

			Ar << ImportExportSize;
			Ar.Seek(Ar.Tell() + ImportExportSize);
		}
	}
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

void UConfigVarsLinker::ExportObject(FStructuredArchive::FRecord Record, class UConfigVarsData* ExportObj)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	FArchiveConfigVars ConfigVarsAr(Ar, this, false, true);

	int32 InitialImportNum = ImportTable.Num();

	int32 InitialOffset = Ar.Tell();
	{
		ExportObj->SerializeConfigVars(ConfigVarsAr, Ar);
	}

	int32 FinalImportNum = ImportTable.Num();

	FConfigVarsExport& Export = ExportTable.AddDefaulted_GetRef();
	Export.ObjectName = ExportObj->GetFName();
	Export.SerialLocation = InitialOffset;
	Export.ClassIndex = ImportTable.Num();

	if (FinalImportNum > InitialImportNum)
	{
		Export.ImportSet.Reset(FinalImportNum);
		Export.ImportSet.AddRange(InitialImportNum, FinalImportNum - 1);
	}

	ImportTable.Emplace(ExportObj->GetClass());

}

void UConfigVarsLinker::VerifyAllExportLoaded()
{
	// 在Editor模式下，需要在序列化前加载完成所有未加载的Export对象。
	for (int32 Index = 0; Index < ExportTable.Num(); ++Index)
	{
		// 注意：已经加载过的对象可能已经过时了，需要结合ClassIndex来判断。
		if (!ExportObjects[Index] && ExportTable[Index].ClassIndex != INDEX_NONE)
		{
#if WITH_EDITOR
			LoadData(Index, nullptr);
#else
			// FindData 会走异步加载，Cook时不能使用
			FindData(Index);
#endif
		}
	}
}

void UConfigVarsLinker::LoadImports_Sync(TArray<int32> ExportIDs)
{
	// 暂时不支持UObjectRedirector

	TArray<int32> AsyncLoadRequestIDs;
	TArray<int32> PendingLoadImport;

	auto AsyncLoadImport = [&AsyncLoadRequestIDs, &PendingLoadImport, this](FConfigVarsImport& Import) {
		if (UPackage* ExistingPackage = FindObjectFast<UPackage>(/*Outer =*/nullptr, Import.ObjectPath.GetLongPackageFName()))
		{
			if (Import.ObjectPath.IsAsset())
			{
				if (UObject* ExistingObject = FindObjectFast<UObject>(ExistingPackage, Import.ObjectPath.GetAssetFName()))
				{
					return;
				}
			}
			else if (Import.ObjectPath.IsSubobject())
			{
				if (UObject* ExistingObject = FindObject<UObject>(ExistingPackage, *Import.ObjectPath.GetSubPathString()))
				{
					return;
				}
			}
		}

		constexpr int32 PIEInstanceID = INDEX_NONE;
		constexpr int32 Priority = INT32_MAX;
		AsyncLoadRequestIDs.Add(LoadPackageAsync(Import.ObjectPath.GetAssetPath().GetPackageName().ToString(), FLoadPackageAsyncDelegate(), Priority, PKG_None, PIEInstanceID));
	};


	for (int32 ExportIndex : ExportIDs)
	{
		FConfigVarsExport& Export = ExportTable[ExportIndex];
		// Class
		FConfigVarsImport& ClassImport = ImportTable[Export.ClassIndex];
		AsyncLoadImport(ClassImport);

		// Dependency
		for (FBitArray::FIterator It(Export.ImportSet); It; ++It)
		{
			int32 Index = *It;
			FConfigVarsImport& Import = ImportTable[Index];

			AsyncLoadImport(Import);
		}
	}

	if (!AsyncLoadRequestIDs.IsEmpty())
	{
		FlushAsyncLoading(AsyncLoadRequestIDs);
	}
}

void UConfigVarsLinker::LoadExports_Sync(TArray<int32> ExportIndexs, TArray<UConfigVarsData*>& ExportObjs)
{
	if (ExportIndexs.IsEmpty())
	{
		return;
	}

	PendingLoadExports = ExportIndexs;
	ExportObjs.Empty(ExportIndexs.Num());

	UPackage* Package = GetPackage();
	this->ClearFlags(RF_NeedLoad | RF_WasLoaded | RF_LoadCompleted);
	this->SetFlags(RF_Public | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded);
	
	int32 AsyncLoadRequestID = -1;

	constexpr int32 PIEInstanceID = INDEX_NONE;
	constexpr int32 Priority = INT32_MAX;
	AsyncLoadRequestID = LoadPackageAsync(Package->GetLoadedPath(), Package->GetFName(), FLoadPackageAsyncDelegate(), PKG_None, PIEInstanceID, Priority, nullptr, LOAD_NoVerify);
	
	FlushAsyncLoading(AsyncLoadRequestID);

	for (int32 Index : ExportIndexs)
	{
		ExportObjs.Add(ExportObjects[Index]);
	}

	this->ClearFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded);
	this->SetFlags(RF_Public | RF_WasLoaded | RF_LoadCompleted);
}

void UConfigVarsLinker::SerializeExport(FStructuredArchive::FRecord Record, int32 ExportIndex, float SerializeHeadOffset)
{
	FArchive& Ar = Record.GetUnderlyingArchive();

	if (!Ar.IsLoading())
	{
		return;
	}

	FConfigVarsExport& Export = ExportTable[ExportIndex];
	FConfigVarsImport& Import = ImportTable[Export.ClassIndex];

	UClass* ExportClass = ((FSoftClassPath&)(Import.ObjectPath)).ResolveClass();
	if (ExportClass)
	{
		ExportObjects[ExportIndex] = NewObject<UConfigVarsData>(this, ExportClass, Export.ObjectName);

		{
			// 反序列化Object的数据
			Ar.Seek(Export.SerialLocation - SerializeHeadOffset);

			FArchiveConfigVars ConfigVarsAr(Ar, this, true, false);

			ExportObjects[ExportIndex]->SerializeConfigVars(ConfigVarsAr, Ar);
		}
	}
}

UConfigVarsData* UConfigVarsLinker::FindData(int32 ExportIndex)
{
	UConfigVarsData* ConfigVarsData = nullptr;

	if (ExportIndex == INDEX_NONE || !ExportTable.IsValidIndex(ExportIndex))
	{
		return ConfigVarsData;
	}

	FConfigVarsExport& Export = ExportTable[ExportIndex];

	// 第二级，在本身的数组中寻找。
	if (IsValid(ExportObjects[ExportIndex]))
	{
		return ExportObjects[ExportIndex];
	}


	// 第三级，在内存中寻找。（可能已经被踢出了缓存，但是仍被别的可达对象所引用）
	ConfigVarsData = FindObject<UConfigVarsData>(this, *(Export.ObjectName.ToString()));


	/************************************************************************/
	/* 第四级，反序列化															*/
	/************************************************************************/
	if (!ConfigVarsData && Export.ClassIndex != INDEX_NONE)
	{
		// 确保所有依赖已经加载
		LoadImports_Sync({ ExportIndex });

		TArray<UConfigVarsData*> OutObjects;
		LoadExports_Sync({ ExportIndex }, OutObjects);
		if (OutObjects.Num() == 1)
		{
			ConfigVarsData = OutObjects[0];
		}
	}
	check(ConfigVarsData);
	ExportObjects[ExportIndex] = ConfigVarsData;

	return ConfigVarsData;
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

UConfigVarsData* UConfigVarsLinker::LoadData(int32& InOutExportIndex, const UClass* TemplateDataClass)
{
	UConfigVarsData* ConfigVarsData = nullptr;

	if (InOutExportIndex != INDEX_NONE && ExportTable.IsValidIndex(InOutExportIndex))
	{
		FConfigVarsExport& Export = ExportTable[InOutExportIndex];

		// 第二级，在本身的数组中寻找。
		if (IsValid(ExportObjects[InOutExportIndex]))
		{
			return ExportObjects[InOutExportIndex];
		}

		// 第三级，在内存中寻找。（可能已经被踢出了缓存，但是仍被别的可达对象所引用）
		ConfigVarsData = FindObject<UConfigVarsData>(this, *(Export.ObjectName.ToString()));


		/************************************************************************/
		/* 第四级，反序列化															*/
		/************************************************************************/
		if (!ConfigVarsData && Export.ClassIndex != INDEX_NONE)
		{
			// 确保所有依赖已经加载
			LoadImports_Sync({ InOutExportIndex });

			FConfigVarsImport& Import = ImportTable[Export.ClassIndex];
			UClass* ExportClass = ((FSoftClassPath&)(Import.ObjectPath)).TryLoadClass<UObject>();
			if (ExportClass)
			{
				ConfigVarsData = NewObject<UConfigVarsData>(this, ExportClass, Export.ObjectName);

				FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
				// Set up a load context
				TRefCountPtr<FUObjectSerializeContext> LoadContext = ThreadContext.GetSerializeContext();
				// Try to load.
				BeginLoad(LoadContext, *(GetPackage()->GetName()));
				{
					// 反序列化Object的数据
					FLinkerLoad* LinkerLoad = CreateLinker_Sync();
					check(LinkerLoad);
					// FStructuredArchive::FRecord Record = FStructuredArchiveFromArchive(*LinkerLoad).GetSlot().EnterRecord();

					((FArchive*)LinkerLoad)->Seek(Export.SerialLocation);
					FArchiveConfigVars ConfigVarsAr(*LinkerLoad, this, true, false);

					ConfigVarsData->SerializeConfigVars(ConfigVarsAr, *LinkerLoad);
				}
				EndLoad(LoadContext);
			}
		}
	}

	// 第五级，重新创建
	if (!ConfigVarsData && TemplateDataClass)
	{
		FGuid UID = FGuid::NewGuid();
		FString UniqueObjectGuid;
		UID.AppendString(UniqueObjectGuid, EGuidFormats::UniqueObjectGuid);
		ConfigVarsData = NewObject<UConfigVarsData>(this, TemplateDataClass, FName(TEXT("ConfigVarsData") + UniqueObjectGuid));
		//ConfigVarsData->ClearFlags(RF_Transient);
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(ConfigVarsData);

		// Mark the package dirty...
		GetPackage()->MarkPackageDirty();
	}

	if (InOutExportIndex != INDEX_NONE)
	{
		ExportObjects[InOutExportIndex] = ConfigVarsData;
	}
	else
	{
		InOutExportIndex = ExportObjects.Num();
		ExportObjects.Add(ConfigVarsData);
	}

	check(ConfigVarsData);

	return ConfigVarsData;
}

void UConfigVarsLinker::RemoveData(int32 ExportIndex)
{
	if (ExportObjects.IsValidIndex(ExportIndex))
	{
		ExportObjects[ExportIndex] = nullptr;
	}

	if (ExportTable.IsValidIndex(ExportIndex))
	{
		ExportTable[ExportIndex].ClassIndex = INDEX_NONE;
	}
}
#endif

//////////////////////////////////////////////////////////////////////////

const UConfigVarsData* UConfigVarsData::K2_GetData(UObject* DataOuter, FConfigVarsBag ConfigVarsBag)
{
	return ConfigVarsBag.GetData(DataOuter);
}

void UConfigVarsData::SerializeConfigVars(FArchiveConfigVars& ConfigVarsAr, FArchive& RealAr)
{
	for (UClass* DataClass = GetClass(); DataClass->IsChildOf(UConfigVarsData::StaticClass()); DataClass = DataClass->GetSuperClass())
	{
		Serialize_Internal(ConfigVarsAr, RealAr, DataClass, this);
	}

	// 仅作为静态数据存储Object而存在，不希望再走UObject的Serialize了。否则会被加入Export中。
	//Super::Serialize(Record);
}

template<typename SrcType>
void UConfigVarsData::Serialize_Internal(FArchiveConfigVars& ConfigVarsAr, FArchive& RealAr, const UStruct* DataStruct, SrcType* SrcData)
{
	FBitArray BitArray;

#if WITH_EDITOR
	if (ConfigVarsAr.IsSaving())
	{
		int32 PropCount = 0;
		for (TFieldIterator<FProperty> PropertyIter(DataStruct); PropertyIter; ++PropertyIter)
		{
			++PropCount;
		}

		BitArray = FBitArray(PropCount);

		int32 PropIndex = 0;
		for (TFieldIterator<FProperty> PropertyIter(DataStruct); PropertyIter; ++PropertyIter, ++PropIndex)
		{
			static const FName NAME_NoConfigVars = "NoConfigVars";
			if ((*PropertyIter)->HasMetaData(NAME_NoConfigVars))
			{
				BitArray.Add(PropIndex);
				continue;
			}
		}
	}
#endif

	ConfigVarsAr << BitArray;

	int32 PropIndex = 0;
	for (TFieldIterator<FProperty> PropertyIter(DataStruct); PropertyIter; ++PropertyIter, ++PropIndex)
	{
		FProperty* ChildProperty = *PropertyIter;

		if (BitArray[PropIndex])
		{
			continue;
		}
		else
		{
			ChildProperty->SetPropertyFlags(EPropertyFlags::CPF_SkipSerialization);
		}

		if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ChildProperty))
		{
			if (ConfigVarsAr.IsSaving())
			{
				UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue((uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
				if (!IsValid(ObjectValue))
				{
					ObjectValue = nullptr;
				}

				ConfigVarsAr << ObjectValue;
			}
			else if (ConfigVarsAr.IsLoading())
			{
				UObject* ObjectValue = nullptr;
				ConfigVarsAr << ObjectValue;

				ObjectProperty->SetObjectPropertyValue((uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), ObjectValue);
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(ChildProperty))
		{
			Serialize_Internal(ConfigVarsAr, RealAr, StructProperty->Struct, (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
		}
		else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ChildProperty))
		{
			if (FObjectProperty* ItemObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(SrcData));

				if (ConfigVarsAr.IsSaving())
				{
					int32 Num = ArrayHelper.Num();
					ConfigVarsAr << Num;
					for (int32 Index = 0; Index < Num; ++Index)
					{
						UObject* ObjectValue = ItemObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
						if (!IsValid(ObjectValue))
						{
							ObjectValue = nullptr;
						}

						ConfigVarsAr << ObjectValue;
					}
				}
				else if (ConfigVarsAr.IsLoading())
				{
					int32 Num = 0;
					ConfigVarsAr << Num;
					ArrayHelper.EmptyValues(Num);

					for (; Num; --Num)
					{
						UObject* ObjectValue = nullptr;
						ConfigVarsAr << ObjectValue;

						int32 Index = ArrayHelper.AddUninitializedValue();
						ItemObjectProperty->SetObjectPropertyValue(ArrayHelper.GetRawPtr(Index), ObjectValue);
					}
				}
			}
			else
			{
				ArrayProperty->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
			}
		}
		else if (FMapProperty* MapProperty = CastField<FMapProperty>(ChildProperty))
		{
			FObjectProperty* KeyObjectProperty = CastField<FObjectProperty>(MapProperty->KeyProp);
			FObjectProperty* ValueObjectProperty = CastField<FObjectProperty>(MapProperty->ValueProp);
			if (KeyObjectProperty || ValueObjectProperty)
			{
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(SrcData));

				if (ConfigVarsAr.IsSaving())
				{
					int32 Num = MapHelper.Num();
					ConfigVarsAr << Num;

					for (int32 MapSparseIndex = 0; MapSparseIndex < MapHelper.GetMaxIndex(); ++MapSparseIndex)
					{
						// Map 是稀疏数组，必须判断Index有效性
						if (MapHelper.IsValidIndex(MapSparseIndex))
						{
							uint8* MapKeyData = MapHelper.GetKeyPtr(MapSparseIndex);
							uint8* MapValueData = MapHelper.GetValuePtr(MapSparseIndex);

							if (KeyObjectProperty)
							{
								UObject* ObjectValue = KeyObjectProperty->GetObjectPropertyValue(MapKeyData);
								if (!IsValid(ObjectValue))
								{
									ObjectValue = nullptr;
								}

								ConfigVarsAr << ObjectValue;
							}
							else
							{
								/*MapProperty->KeyProp->SerializeItem(Record.EnterField(TEXT("Key")), MapKeyData);*/
								MapProperty->KeyProp->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), MapKeyData);
							}

							if (ValueObjectProperty)
							{
								UObject* ObjectValue = ValueObjectProperty->GetObjectPropertyValue(MapValueData);
								if (!IsValid(ObjectValue))
								{
									ObjectValue = nullptr;
								}

								ConfigVarsAr << ObjectValue;
							}
							else
							{
								/*MapProperty->ValueProp->SerializeItem(Record.EnterField(TEXT("Value")), MapValueData);*/
								MapProperty->ValueProp->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), MapValueData);
							}
						}
					}
				}
				else if (ConfigVarsAr.IsLoading())
				{
					int32 Num = 0;
					ConfigVarsAr << Num;
					MapHelper.EmptyValues(Num);

					for (; Num; --Num)
					{
						int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
						uint8* MapKeyData = MapHelper.GetKeyPtr(Index);
						uint8* MapValueData = MapHelper.GetValuePtr(Index);

						if (KeyObjectProperty)
						{
							UObject* ObjectValue = nullptr;
							ConfigVarsAr << ObjectValue;

							KeyObjectProperty->SetObjectPropertyValue(MapKeyData, ObjectValue);
						}
						else
						{
							/*MapProperty->KeyProp->SerializeItem(Record.EnterField(TEXT("Key")), MapKeyData);*/
							MapProperty->KeyProp->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), MapKeyData);
						}

						if (ValueObjectProperty)
						{
							UObject* ObjectValue = nullptr;
							ConfigVarsAr << ObjectValue;

							ValueObjectProperty->SetObjectPropertyValue(MapValueData, ObjectValue);
						}
						else
						{
							/*MapProperty->ValueProp->SerializeItem(Record.EnterField(TEXT("Value")), MapValueData);*/
							MapProperty->ValueProp->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), MapValueData);
						}
					}
				}
			}
			else
			{
				MapProperty->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
			}
		}
		else if (FSetProperty* SetProperty = CastField<FSetProperty>(ChildProperty))
		{
			if (FObjectProperty* ItemObjectProperty = CastField<FObjectProperty>(SetProperty->ElementProp))
			{
				FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(SrcData));

				if (ConfigVarsAr.IsSaving())
				{
					int32 Num = SetHelper.Num();
					ConfigVarsAr << Num;
					for (int32 SetSparseIndex = 0; SetSparseIndex < SetHelper.GetMaxIndex(); ++SetSparseIndex)
					{
						// Set 是稀疏数组，必须判断Index有效性
						if (SetHelper.IsValidIndex(SetSparseIndex))
						{
							UObject* ObjectValue = ItemObjectProperty->GetObjectPropertyValue(SetHelper.GetElementPtr(SetSparseIndex));
							if (!IsValid(ObjectValue))
							{
								ObjectValue = nullptr;
							}

							ConfigVarsAr << ObjectValue;
						}
					}
				}
				else if (ConfigVarsAr.IsLoading())
				{
					int32 Num = 0;
					ConfigVarsAr << Num;
					SetHelper.EmptyElements(Num);

					for (; Num; --Num)
					{
						UObject* ObjectValue = nullptr;
						ConfigVarsAr << ObjectValue;

						int32 Index = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
						ItemObjectProperty->SetObjectPropertyValue(SetHelper.GetElementPtr(Index), ObjectValue);
					}
				}
			}
			else
			{
				SetProperty->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal(), nullptr);
			}
		}
		else
		{
			//TStringBuilder<256> TagName;
			//TagName = ChildProperty->GetName();
			ChildProperty->SerializeItem(FStructuredArchiveFromArchive(RealAr).GetSlot(), (uint8*)SrcData + ChildProperty->GetOffset_ForInternal());
		}
	}
}