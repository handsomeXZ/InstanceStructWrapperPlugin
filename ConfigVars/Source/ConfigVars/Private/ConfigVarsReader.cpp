#include "ConfigVarsReader.h"

#include "UObject/Package.h"
#include "UObject/ObjectResource.h"
#include "UObject/UObjectGlobals.h"

#include "BitArray.h"
#include "ConfigVarsLinker.h"

static int32 LRUCacheMaxNum = 128;

const UConfigVarsData* UConfigVarsBagReader::LoadData(UObject* Outer, FConfigVarsBag ConfigVarsBag)
{
	return ConfigVarsBag.LoadData(Outer);
}

void UConfigVarsBagReader::LoadData_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, FOnConfigVarsAsyncCallBack CallBack)
{
	ConfigVarsBag.LoadData_Async(Outer, CallBack);
}

FConfigVarsBag::~FConfigVarsBag()
{
#if WITH_EDITOR
	if (Outermost && ExportIndex != INDEX_NONE)
	{
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Outermost->GetPackage(), TEXT("ConfigVarsLinker"));
		if (ConfigVarsLinker)
		{
			ConfigVarsLinker->RemoveData(ExportIndex);
		}
	}
#endif
}

bool FConfigVarsBag::Serialize(FArchive& Ar)
{
	Ar << ExportIndex;

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << Outermost;
	}
#endif

	return true;
}

const UConfigVarsData* FConfigVarsBag::LoadData(UObject* Outer)
{
	UConfigVarsData* ConfigVarsData = nullptr;

	if (!Outer)
	{
		ConfigVarsData = nullptr;
		return nullptr;
	}
	UPackage* Package = Outer->GetPackage();

#if !WITH_EDITOR
	static FConfigVarsLRUCache GlobalConfigVarsDataCache(LRUCacheMaxNum);

	// 第一级，在缓存优化中寻找，还需要提供资源移除。(Editor 不会走这里，否则会扰乱原有的新增、修改和删除流程)
	ConfigVarsData = GlobalConfigVarsDataCache.FindAndTouchRef(Package->GetPackageIdToLoad(), ExportIndex);
	if (ConfigVarsData)
	{
		return ConfigVarsData;
	}
#endif

	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		ConfigVarsData = nullptr;
		return nullptr;
	}


	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (ConfigVarsLinker)
	{
		ConfigVarsData = ConfigVarsLinker->LoadData(ExportIndex);
	}


#if !WITH_EDITOR
	if (ConfigVarsData)
	{
		GlobalConfigVarsDataCache.Add(Package->GetPackageIdToLoad(), ExportIndex, ConfigVarsData);
	}
#endif

	return ConfigVarsData;

}

void FConfigVarsBag::LoadData_Async(UObject* Outer, FOnConfigVarsAsyncCallBack CallBack)
{
	UConfigVarsData* ConfigVarsData = nullptr;

	if (!Outer)
	{
		CallBack.ExecuteIfBound({ConfigVarsData});
		return;
	}

	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		CallBack.ExecuteIfBound({ ConfigVarsData });
		return;
	}

	UPackage* Package = Outer->GetPackage();

#if !WITH_EDITOR
	static FConfigVarsLRUCache GlobalConfigVarsDataCache(LRUCacheMaxNum);

	// 第一级，在缓存优化中寻找，还需要提供资源移除。(Editor 不会走这里，否则会扰乱原有的新增、修改和删除流程)
	ConfigVarsData = GlobalConfigVarsDataCache.FindAndTouchRef(Package->GetPackageIdToLoad(), ExportIndex);
	if (ConfigVarsData)
	{
		CallBack.ExecuteIfBound({ ConfigVarsData });
		return;
	}
#endif


	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (ConfigVarsLinker)
	{
		ConfigVarsLinker->LoadData_Async(ExportIndex, FLoadConfigVarsAsyncDelegate::CreateWeakLambda(Outer, [Package, ExportIndex = ExportIndex, CallBack](TArray<UConfigVarsData*> OutObjects) {
#if !WITH_EDITOR
			if (!OutObjects.IsEmpty() && OutObjects[0])
			{
				GlobalConfigVarsDataCache.Add(Package->GetPackageIdToLoad(), ExportIndex, OutObjects[0]);
			}
#endif
			CallBack.ExecuteIfBound(OutObjects);
		}));
	}
}

#if WITH_EDITOR
UConfigVarsData* FConfigVarsBag::LoadOrAddData(UObject* Outer, const UClass* DataClass)
{
	UConfigVarsData* ConfigVarsData = nullptr;

	if (!Outer)
	{
		ConfigVarsData = nullptr;
		return nullptr;
	}
	UPackage* Package = Outer->GetPackage();


	// 第一级，在缓存优化中寻找，还需要提供资源移除。(Editor 不会走这里，否则会扰乱原有的新增、修改和删除流程)
	// ...


	// 由ConfigVarsLinker继续寻找或创建
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (!ConfigVarsLinker)
	{
		ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("ConfigVarsLinker"), RF_Public | RF_Standalone);
	}
	if (ConfigVarsLinker)
	{
		ConfigVarsData = ConfigVarsLinker->LoadOrAddData(ExportIndex, DataClass);
	}

	return ConfigVarsData;
}
#endif

//////////////////////////////////////////////////////////////////////////
// FConfigVarsLRUCache
void FConfigVarsLRUCache::Add(const FPackageId& PakUID, const int32 UniqueID, UConfigVarsData* Data)
{
	return Add(GetHashKey(PakUID, UniqueID), Data);
}

UConfigVarsData* FConfigVarsLRUCache::FindAndTouchRef(const FPackageId& PakUID, const int32 UniqueID)
{
	return FindAndTouchRef(GetHashKey(PakUID, UniqueID));
}

UConfigVarsData* FConfigVarsLRUCache::FindRef(const FPackageId& PakUID, const int32 UniqueID)
{
	return FindRef(GetHashKey(PakUID, UniqueID));
}

void FConfigVarsLRUCache::Remove(const FPackageId& PakUID, const int32 UniqueID)
{
	Remove(GetHashKey(PakUID, UniqueID));
}

void FConfigVarsLRUCache::Add(const uint32 Hash, UConfigVarsData* Data)
{
	Cache.Add(Hash, Data);
}

UConfigVarsData* FConfigVarsLRUCache::FindAndTouchRef(const uint32 Hash)
{
	return Cache.FindAndTouchRef(Hash);
}

UConfigVarsData* FConfigVarsLRUCache::FindRef(const uint32 Hash)
{
	return Cache.FindRef(Hash);
}

void FConfigVarsLRUCache::Remove(const uint32 Hash)
{
	Cache.Remove(Hash);
}

void FConfigVarsLRUCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto it = Cache.begin(); it != Cache.end(); ++it)
	{
		Collector.AddReferencedObject(*it);
	}
}

FString FConfigVarsLRUCache::GetReferencerName() const
{
	return TEXT("FConfigVarsLRUCache");
}