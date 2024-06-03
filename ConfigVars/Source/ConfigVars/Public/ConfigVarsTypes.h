#pragma once

#include "CoreMinimal.h"

#include "StructView.h"
#include "InstancedStruct.h"

#include "ConfigVarsTypes.generated.h"

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
	virtual ~FConfigVarsBag() {}

	FConstStructView LoadData(UObject* Outer) const;
	void LoadData_Async(UObject* Outer, int32 Priority = DefaultAsyncLoadPriority) const;
	bool IsValid() const { return ExportIndex != INDEX_NONE; }

	bool Serialize(FArchive& Ar);

	int32 GetExportIndex() { return ExportIndex; }

#if WITH_EDITOR
	UObject* GetOutermost() { return Outermost; }
#endif
private:
	friend class UConfigVarsLinker;

	UPROPERTY()
	int32 ExportIndex = INDEX_NONE;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UObject* Outermost = nullptr;
	UPROPERTY()
	class UConfigVarsLinker* Linker = nullptr;
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