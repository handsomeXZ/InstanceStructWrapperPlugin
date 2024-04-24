#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "InstancedStruct.h"
#include "Engine/DataAsset.h"
#include "StructView.h"
#include "Containers/LruCache.h"

#include "ConfigVarsReader.generated.h"

// Default priority for all async loads
static const int32 DefaultAsyncLoadPriority = 0;
// Priority to try and load immediately
static const int32 AsyncLoadHighPriority = INT32_MAX;

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnConfigVarsAsyncCallBack, const TArray<FInstancedStruct>&, OutStructData);

USTRUCT(BlueprintType)
struct CONFIGVARS_API FConfigVarsBag
{
	GENERATED_BODY()
public:
	virtual ~FConfigVarsBag();

	FConstStructView LoadData(UObject* Outer);
	void LoadData_Async(UObject* Outer, FOnConfigVarsAsyncCallBack CallBack, int32 Priority = DefaultAsyncLoadPriority);

	bool Serialize(FArchive& Ar);

	UPROPERTY()
	int32 ExportIndex = INDEX_NONE;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UObject* Outermost = nullptr;
#endif

#if WITH_EDITOR
	FStructView LoadOrAddData(UObject* Outer, const UScriptStruct* DataStruct);
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

UCLASS()
class UConfigVarsBagReader : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = ConfigVarsData)
	static FInstancedStruct LoadData(UObject* Outer, FConfigVarsBag ConfigVarsBag);

	UFUNCTION(BlueprintCallable, Category = ConfigVarsData)
	static void LoadData_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, FOnConfigVarsAsyncCallBack CallBack, int32 Priority);
};