// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HoudiniEngine : ModuleRules
	{
		public HoudiniEngine(TargetInfo Target)
		{
			PublicLibraryPaths.Add("E:/dev_r/hfs/custom/houdini/dsolib");
			PublicAdditionalLibraries.Add("E:/dev_r/hfs/custom/houdini/dsolib/libHAPI.a");

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
					"E:/dev_r/hfs/custom/houdini/include/HAPI"
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/HoudiniEngine/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
					"CoreUObject",
					"Engine",
					"RenderCore",
					"ShaderCore",
					"RHI"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
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
}