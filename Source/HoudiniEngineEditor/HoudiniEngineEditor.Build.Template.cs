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
{{
	public class HoudiniEngineEditor : ModuleRules
	{{
		public HoudiniEngineEditor(TargetInfo Target)
		{{
			PublicLibraryPaths.Add("{cpp_lib_path}");
			PublicAdditionalLibraries.Add("{cpp_lib_path}/{cpp_hapi_lib}");

			PublicIncludePaths.AddRange(
				new string[] {{
					// ... add public include paths required here ...
					"{cpp_include_path}"
				}}
				);

			PrivateIncludePaths.AddRange(
				new string[] {{
					"HoudiniEngineEditor/Private",
					// ... add other private include paths required here ...
				}}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{{
					"Core",
					// ... add other public dependencies that you statically link with here ...
					"CoreUObject",
					"Engine",
					"RenderCore",
					"ShaderCore",
					"InputCore",
					"RHI",
                    "AssetTools",
                    "UnrealEd",
                    "HoudiniEngine"
				}}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{{
					// ... add private dependencies that you statically link with here ...
				}}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{{
					// ... add any modules that your module loads dynamically here ...
				}}
				);
		}}
	}}
}}
