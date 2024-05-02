#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Containers/LruCache.h"

#include "ConfigVarsReader.generated.h"

class FOnConfigVarsAsyncCallBack;

UCLASS()
class CONFIGVARS_API UConfigVarsBagReader : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "ConfigVarsData", meta = (CustomStructureParam = "Value", ExpandEnumAsExecs = "ExecResult"))
	static void GetValue(EStructUtilsResult& ExecResult, UObject* Outer, UPARAM(Ref) const FConfigVarsBag& ConfigVarsBag, int32& Value);

	UFUNCTION(BlueprintCallable, Category = "ConfigVarsData")
	static void LoadData_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, FOnConfigVarsAsyncCallBack CallBack, int32 Priority);
private:
	DECLARE_FUNCTION(execGetValue);
};