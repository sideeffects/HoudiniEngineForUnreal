/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 * COMMENTS:
 *      This file is generated. Do not modify directly.
 */

namespace UnrealBuildTool.Rules
{
	public class HoudiniEngine : ModuleRules
	{
		public HoudiniEngine(TargetInfo Target)
		{
			Definitions.Add("HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE=65536");

			PublicLibraryPaths.Add("C:/Program Files/Side Effects Software/Houdini 14.0.6/custom/houdini/dsolib");
			PublicAdditionalLibraries.Add("C:/Program Files/Side Effects Software/Houdini 14.0.6/custom/houdini/dsolib/libHAPI.a");

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
					"C:/Program Files/Side Effects Software/Houdini 14.0.6/toolkit/include/HAPI"
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"HoudiniEngine/Private"
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"ShaderCore",
					"InputCore",
					"RHI",
					"AssetTools",
					"UnrealEd",
					"Slate",
					"SlateCore",
					"Projects"
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
