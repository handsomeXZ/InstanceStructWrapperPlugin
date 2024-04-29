// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ConfigVars : ModuleRules
{
	public ConfigVars(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

        PublicIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("CoreUObject"), "Private")
           }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("CoreUObject"), "Private")
           }
        );
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"StructUtils",
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
