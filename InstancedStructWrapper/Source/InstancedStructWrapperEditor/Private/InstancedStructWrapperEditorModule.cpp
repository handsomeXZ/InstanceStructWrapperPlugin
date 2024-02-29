// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStructWrapperEditorModule.h"

#include "InstancedStructWrapperDetails.h"

#define LOCTEXT_NAMESPACE "FInstancedStructWrapperEditorModule"

void FInstancedStructWrapperEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("InstancedStructWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInstancedStructWrapperDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

}

void FInstancedStructWrapperEditorModule::ShutdownModule()
{

} 


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInstancedStructWrapperEditorModule, InstancedStructWrapperEditor)