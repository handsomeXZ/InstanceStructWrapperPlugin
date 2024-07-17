#include "ConfigVarsReader.h"

#include "UObject/Package.h"
#include "UObject/ObjectResource.h"
#include "UObject/UObjectGlobals.h"
#include "StructUtilsFunctionLibrary.h"
#include "Blueprint/BlueprintExceptionInfo.h"

#include "ConfigVarsLinker.h"

#define LOCTEXT_NAMESPACE "ConfigVarsBagReader"

void UConfigVarsBagReader::GetValue(EStructUtilsResult& ExecResult, UObject* Outer, const FConfigVarsBag& ConfigVarsBag, int32& Value)
{
	// We should never hit this! stubs to avoid NoExport on the class.
	checkNoEntry();
}

DEFINE_FUNCTION(UConfigVarsBagReader::execGetValue)
{
	P_GET_ENUM_REF(EStructUtilsResult, ExecResult);
	P_GET_OBJECT(UObject, Outer);
	P_GET_STRUCT_REF(FConfigVarsBag, ConfigVarsBag);


	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* ValueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ExecResult = EStructUtilsResult::NotValid;

	if (!ValueProp || !ValuePtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("ConfigVars_GetInvalidValueWarning", "Failed to resolve the Value for Get ConfigVars Value")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;
		ExecResult = EStructUtilsResult::NotValid;
		if (IsValid(Outer) && ConfigVarsBag.IsDataValid())
		{
			FConstStructView StructView = ConfigVarsBag.LoadData(Outer);
			if (StructView.IsValid() && StructView.GetScriptStruct()->IsChildOf(ValueProp->Struct))
			{
				ValueProp->Struct->CopyScriptStruct(ValuePtr, StructView.GetMemory());
				ExecResult = EStructUtilsResult::Valid;
			}
		}
		P_NATIVE_END;
	}
}

void UConfigVarsBagReader::LoadData_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, int32 Priority)
{
	ConfigVarsBag.LoadData_Async(Outer, Priority);
}

void UConfigVarsBagReader::LoadData_Multi_Async(UObject* Outer, FConfigVarsBag ConfigVarsBegin, FConfigVarsBag ConfigVarsEnd, int32 Priority)
{
	FConfigVarsBag::LoadData_Multi_Async(Outer, ConfigVarsBegin, ConfigVarsEnd, Priority);
}

void UConfigVarsBagReader::LoadData_Nested_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, int32 Priority)
{
	FConfigVarsBag::LoadData_Nested_Async(Outer, ConfigVarsBag, Priority);
}

#undef LOCTEXT_NAMESPACE