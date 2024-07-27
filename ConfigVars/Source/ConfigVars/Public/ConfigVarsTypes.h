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
	virtual ~FConfigVarsBag();

	// 这些加载方式仅限Runtime使用，否则，请用EditorLoadData
	static void LoadData_Multi_Async(const UObject* Outer, FConfigVarsBag ConfigVarsBegin, FConfigVarsBag ConfigVarsEnd, int32 Priority = DefaultAsyncLoadPriority);
	// 加载嵌套的懒加载块，Depth为深度，Depth = -1时，加载当前数据块下的所有嵌套数据块
	static void LoadData_Nested_Async(const UObject* Outer, FConfigVarsBag ConfigVarsBag, int32 Priority = DefaultAsyncLoadPriority);
	FConstStructView LoadData(const UObject* Outer) const;
	void LoadData_Async(const UObject* Outer, int32 Priority = DefaultAsyncLoadPriority) const;

	bool IsDataValid() const { return ExportIndex != INDEX_NONE; }

	bool Serialize(FArchive& Ar);

	int32 GetExportIndex() { return ExportIndex; }

#if WITH_EDITOR
	UObject* GetOutermost() { return Outermost; }
	FStructView EditorLoadData(const UObject* Outer) const;
	FStructView EditorLoadOrAddData(UObject* Outer, const UScriptStruct* TemplateDataStruct);
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

USTRUCT(BlueprintType)
struct CONFIGVARS_API FConfigVarsData_Empty
{
	GENERATED_BODY()

};

template<>
struct TStructOpsTypeTraits<FConfigVarsBag> : public TStructOpsTypeTraitsBase2<FConfigVarsBag>
{
	enum
	{
		WithSerializer = true,
	};
};

USTRUCT(BlueprintType)
struct FVars_Base
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Vars_Base_ID;
};

USTRUCT(BlueprintType)
struct FVars_Nested
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Vars_Nested_ID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DataStruct = "/Script/ConfigVars.Vars_Base"))
	FConfigVarsBag NestedData;
};

USTRUCT(BlueprintType)
struct FVars_Nested_Child : public FVars_Nested
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Vars_Nested_Child_ID;
};

UCLASS(Blueprintable, BlueprintType)
class UConfigVarsTestData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DataStruct = "/Script/ConfigVars.Vars_Nested"))
	TArray<FConfigVarsBag> NestedDatas;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (InheritedDataStruct, DataStruct = "/Script/ConfigVars.Vars_Nested_Child"))
	TArray<FConfigVarsBag> NestedDatas2;
};