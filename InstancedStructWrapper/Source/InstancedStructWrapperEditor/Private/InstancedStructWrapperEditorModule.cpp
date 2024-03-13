// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStructWrapperEditorModule.h"

#include "InstancedStructWrapperDetails.h"

#define LOCTEXT_NAMESPACE "FInstancedStructWrapperEditorModule"

void FInstancedStructWrapperEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("InstancedStructWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInstancedStructWrapperDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("InstancedStructContainerWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInstancedStructWrapperContainerDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

}

void FInstancedStructWrapperEditorModule::ShutdownModule()
{
	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("InstancedStructWrapper");
		PropertyModule.UnregisterCustomPropertyTypeLayout("InstancedStructContainerWrapper");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
} 


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInstancedStructWrapperEditorModule, InstancedStructWrapperEditor)