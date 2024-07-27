// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConfigVarsEditorModule.h"

#include "ConfigVarsDetails.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "FConfigVarsEditorModule"

TSharedPtr<FSlateStyleSet> StyleSet = nullptr;

void FConfigVarsEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("ConfigVarsBag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FConfigVarsDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	StyleSet = MakeShared<FSlateStyleSet>("ConfigVarsStyle");
	const FString Path = IPluginManager::Get().FindPlugin("ConfigVars")->GetBaseDir()/ TEXT("Resources");
	StyleSet->Set("ConfigVarsIcon", new FSlateImageBrush(Path/ "Icon128.png", FVector2D(16, 16)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FConfigVarsEditorModule::ShutdownModule()
{
	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("ConfigVarsBag");
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FConfigVarsEditorModule, ConfigVarsEditor)