// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ConfigVarsEditor : ModuleRules
{
	public ConfigVarsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

    var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

        PublicIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("CoreUObject"), "Private"),
                System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
           }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("CoreUObject"), "Private"),
                System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
           }
        );

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"ConfigVars",
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
                "PropertyEditor",
                "StructUtils",
                "UnrealEd",
                "Projects",
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
