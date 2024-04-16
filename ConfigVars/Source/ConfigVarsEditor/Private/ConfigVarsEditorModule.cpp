// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConfigVarsEditorModule.h"

#include "ConfigVarsDetails.h"

#define LOCTEXT_NAMESPACE "FConfigVarsEditorModule"

void FConfigVarsEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("ConfigVarsBag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FConfigVarsDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

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
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FConfigVarsEditorModule, ConfigVarsEditor)