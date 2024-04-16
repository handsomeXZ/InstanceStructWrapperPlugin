#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "InstancedStruct.h"
#include "Engine/DataAsset.h"

#include "Containers/LruCache.h"

#include "ConfigVarsReader.generated.h"

class UConfigVarsData;

USTRUCT(BlueprintType)
struct CONFIGVARS_API FConfigVarsBag
{
	GENERATED_BODY()
public:
	virtual ~FConfigVarsBag();

	const UConfigVarsData* GetData(UObject* Outer);

	template<typename T>
	const T* GetData(UObject* Outer)
	{
		return Cast<T>(GetData(Outer));
	}

	bool Serialize(FArchive& Ar);

	UPROPERTY()
	int32 ExportIndex = INDEX_NONE;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UObject* Outermost = nullptr;
#endif

#if WITH_EDITOR
	UConfigVarsData* LoadData(UObject* Outer, const UClass* DataClass);
#endif
};

template<>
struct TStructOpsTypeTraits<FConfigVarsBag> : public TStructOpsTypeTraitsBase2<FConfigVarsBag>
{
	enum
	{
		WithSerializer = true,
	};
};

/************************************************************************/
/************************************************************************/

class FConfigVarsLRUCache : public FGCObject
{
public:
	FConfigVarsLRUCache() : Cache() {}
	FConfigVarsLRUCache(int32 InMaxNumElements) : Cache(InMaxNumElements) {}

	void Add(const FPackageId& PakUID, const int32 UniqueID, UConfigVarsData* Data);
	void Add(const uint32 Hash, UConfigVarsData* Data);

	UConfigVarsData* FindAndTouchRef(const FPackageId& PakUID, const int32 UniqueID);
	UConfigVarsData* FindAndTouchRef(const uint32 Hash);

	UConfigVarsData* FindRef(const FPackageId& PakUID, const int32 UniqueID);
	UConfigVarsData* FindRef(const uint32 Hash);

	void Remove(const FPackageId& PakUID, const int32 UniqueID);
	void Remove(const uint32 Hash);

	uint32 GetHashKey(const FPackageId& PakUID, const int32 UniqueID) {
		return HashCombine(GetTypeHash(PakUID), GetTypeHash(UniqueID));
	}
public:
	// ~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// ~ FGCObject
private:
	//https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Containers/TLruCache?application_version=5.0
	TLruCache<uint32, UConfigVarsData*> Cache;
};

