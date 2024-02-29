// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class InstancedStructWrapperEditor : ModuleRules
{
    public InstancedStructWrapperEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

        PublicIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private")
           }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private")
           }
        );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
				// ... add other public dependencies that you statically link with here ...
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
                "InstancedStructWrapper",
                "StructUtils",
                "StructUtilsEditor",
                "PropertyEditor",
                "UnrealEd",
                "StructUtilsEngine",
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