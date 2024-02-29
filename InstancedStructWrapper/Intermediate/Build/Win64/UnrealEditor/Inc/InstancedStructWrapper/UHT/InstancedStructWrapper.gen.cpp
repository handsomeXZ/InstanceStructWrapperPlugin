// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "InstancedStructWrapper/Public/InstancedStructWrapper.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeInstancedStructWrapper() {}
// Cross Module References
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	INSTANCEDSTRUCTWRAPPER_API UClass* Z_Construct_UClass_UInstancedStructTest();
	INSTANCEDSTRUCTWRAPPER_API UClass* Z_Construct_UClass_UInstancedStructTest_NoRegister();
	INSTANCEDSTRUCTWRAPPER_API UScriptStruct* Z_Construct_UScriptStruct_FInstancedStructWrapper();
	STRUCTUTILS_API UScriptStruct* Z_Construct_UScriptStruct_FInstancedStruct();
	UPackage* Z_Construct_UPackage__Script_InstancedStructWrapper();
// End Cross Module References

static_assert(std::is_polymorphic<FInstancedStructWrapper>() == std::is_polymorphic<FInstancedStruct>(), "USTRUCT FInstancedStructWrapper cannot be polymorphic unless super FInstancedStruct is polymorphic");

	static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_InstancedStructWrapper;
class UScriptStruct* FInstancedStructWrapper::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_InstancedStructWrapper.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_InstancedStructWrapper.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FInstancedStructWrapper, (UObject*)Z_Construct_UPackage__Script_InstancedStructWrapper(), TEXT("InstancedStructWrapper"));
	}
	return Z_Registration_Info_UScriptStruct_InstancedStructWrapper.OuterSingleton;
}
template<> INSTANCEDSTRUCTWRAPPER_API UScriptStruct* StaticStruct<FInstancedStructWrapper>()
{
	return FInstancedStructWrapper::StaticStruct();
}
	struct Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics
	{
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[];
#endif
		static void* NewStructOps();
#if WITH_EDITORONLY_DATA
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_DisplayNameOverride_MetaData[];
#endif
		static const UECodeGen_Private::FTextPropertyParams NewProp_DisplayNameOverride;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
#endif // WITH_EDITORONLY_DATA
		static const UECodeGen_Private::FStructParams ReturnStructParams;
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/InstancedStructWrapper.h" },
	};
#endif
	void* Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FInstancedStructWrapper>();
	}
#if WITH_EDITORONLY_DATA
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewProp_DisplayNameOverride_MetaData[] = {
		{ "ModuleRelativePath", "Public/InstancedStructWrapper.h" },
	};
#endif
	const UECodeGen_Private::FTextPropertyParams Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewProp_DisplayNameOverride = { "DisplayNameOverride", nullptr, (EPropertyFlags)0x0010000800000000, UECodeGen_Private::EPropertyGenFlags::Text, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FInstancedStructWrapper, DisplayNameOverride), METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewProp_DisplayNameOverride_MetaData), Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewProp_DisplayNameOverride_MetaData) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewProp_DisplayNameOverride,
	};
#endif // WITH_EDITORONLY_DATA
	const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_InstancedStructWrapper,
		Z_Construct_UScriptStruct_FInstancedStruct,
		&NewStructOps,
		"InstancedStructWrapper",
		IF_WITH_EDITORONLY_DATA(Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::PropPointers, nullptr),
		IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::PropPointers), 0),
		sizeof(FInstancedStructWrapper),
		alignof(FInstancedStructWrapper),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000201),
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::Struct_MetaDataParams)
	};
#if WITH_EDITORONLY_DATA
	static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::PropPointers) < 2048);
#endif
	UScriptStruct* Z_Construct_UScriptStruct_FInstancedStructWrapper()
	{
		if (!Z_Registration_Info_UScriptStruct_InstancedStructWrapper.InnerSingleton)
		{
			UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_InstancedStructWrapper.InnerSingleton, Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::ReturnStructParams);
		}
		return Z_Registration_Info_UScriptStruct_InstancedStructWrapper.InnerSingleton;
	}
	void UInstancedStructTest::StaticRegisterNativesUInstancedStructTest()
	{
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UInstancedStructTest);
	UClass* Z_Construct_UClass_UInstancedStructTest_NoRegister()
	{
		return UInstancedStructTest::StaticClass();
	}
	struct Z_Construct_UClass_UInstancedStructTest_Statics
	{
		static UObject* (*const DependentSingletons[])();
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_Test_MetaData[];
#endif
		static const UECodeGen_Private::FStructPropertyParams NewProp_Test;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UInstancedStructTest_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_InstancedStructWrapper,
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UInstancedStructTest_Statics::DependentSingletons) < 16);
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UInstancedStructTest_Statics::Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "InstancedStructWrapper.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/InstancedStructWrapper.h" },
	};
#endif
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UInstancedStructTest_Statics::NewProp_Test_MetaData[] = {
		{ "BaseStruct", "/Script/Chooser.ChooserParameterObjectBase" },
		{ "Category", "InstancedStructTest" },
		{ "ExcludeBaseStruct", "" },
		{ "ModuleRelativePath", "Public/InstancedStructWrapper.h" },
	};
#endif
	const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UInstancedStructTest_Statics::NewProp_Test = { "Test", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UInstancedStructTest, Test), Z_Construct_UScriptStruct_FInstancedStructWrapper, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UInstancedStructTest_Statics::NewProp_Test_MetaData), Z_Construct_UClass_UInstancedStructTest_Statics::NewProp_Test_MetaData) }; // 3510057276
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UInstancedStructTest_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UInstancedStructTest_Statics::NewProp_Test,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UInstancedStructTest_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UInstancedStructTest>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_UInstancedStructTest_Statics::ClassParams = {
		&UInstancedStructTest::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UInstancedStructTest_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UInstancedStructTest_Statics::PropPointers),
		0,
		0x000000A0u,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UInstancedStructTest_Statics::Class_MetaDataParams), Z_Construct_UClass_UInstancedStructTest_Statics::Class_MetaDataParams)
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UInstancedStructTest_Statics::PropPointers) < 2048);
	UClass* Z_Construct_UClass_UInstancedStructTest()
	{
		if (!Z_Registration_Info_UClass_UInstancedStructTest.OuterSingleton)
		{
			UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UInstancedStructTest.OuterSingleton, Z_Construct_UClass_UInstancedStructTest_Statics::ClassParams);
		}
		return Z_Registration_Info_UClass_UInstancedStructTest.OuterSingleton;
	}
	template<> INSTANCEDSTRUCTWRAPPER_API UClass* StaticClass<UInstancedStructTest>()
	{
		return UInstancedStructTest::StaticClass();
	}
	UInstancedStructTest::UInstancedStructTest(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UInstancedStructTest);
	UInstancedStructTest::~UInstancedStructTest() {}
	struct Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics
	{
		static const FStructRegisterCompiledInInfo ScriptStructInfo[];
		static const FClassRegisterCompiledInInfo ClassInfo[];
	};
	const FStructRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics::ScriptStructInfo[] = {
		{ FInstancedStructWrapper::StaticStruct, Z_Construct_UScriptStruct_FInstancedStructWrapper_Statics::NewStructOps, TEXT("InstancedStructWrapper"), &Z_Registration_Info_UScriptStruct_InstancedStructWrapper, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FInstancedStructWrapper), 3510057276U) },
	};
	const FClassRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics::ClassInfo[] = {
		{ Z_Construct_UClass_UInstancedStructTest, UInstancedStructTest::StaticClass, TEXT("UInstancedStructTest"), &Z_Registration_Info_UClass_UInstancedStructTest, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UInstancedStructTest), 3917473712U) },
	};
	static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_3369401282(TEXT("/Script/InstancedStructWrapper"),
		Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics::ClassInfo),
		Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_project_5_3_Project_Plugins_InstancedStructWrapper_Source_InstancedStructWrapper_Public_InstancedStructWrapper_h_Statics::ScriptStructInfo),
		nullptr, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
