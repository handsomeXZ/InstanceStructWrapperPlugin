#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConfigVarsTypes.h"
#include "Containers/LruCache.h"

#include "ConfigVarsReader.generated.h"


UCLASS()
class CONFIGVARS_API UConfigVarsBagReader : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "ConfigVarsData", meta = (CustomStructureParam = "Value", ExpandEnumAsExecs = "ExecResult"))
	static void GetValue(EStructUtilsResult& ExecResult, UObject* Outer, UPARAM(Ref) const FConfigVarsBag& ConfigVarsBag, int32& Value);

	UFUNCTION(BlueprintCallable, Category = "ConfigVarsData")
	static void LoadData_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, int32 Priority);
	UFUNCTION(BlueprintCallable, Category = "ConfigVarsData")
	static void LoadData_Multi_Async(UObject* Outer, FConfigVarsBag ConfigVarsBegin, FConfigVarsBag ConfigVarsEnd, int32 Priority);
	UFUNCTION(BlueprintCallable, Category = "ConfigVarsData")
	static void LoadData_Nested_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, int32 Priority);

private:
	DECLARE_FUNCTION(execGetValue);
};