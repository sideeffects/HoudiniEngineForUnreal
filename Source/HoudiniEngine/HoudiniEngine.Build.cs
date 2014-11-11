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

/*

	Houdini Version: 13.0.593
	Houdini Engine Version: 1.8.20
	Unreal Version: 4.5.0

 */

namespace UnrealBuildTool.Rules
{
	public class HoudiniEngine : ModuleRules
	{
		public HoudiniEngine(TargetInfo Target)
		{
			string HFSPath = "";
			string HoudiniVersion = "13.0.593";
			
			string HAPILib = "";
			string HAPILibPath = "";
			
			if ( HFSPath == "" )
			{
				if( Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 )
				{
					HFSPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
				}
				else if( Target.Platform == UnrealTargetPlatform.Mac )
				{
					HFSPath = "/Library/Frameworks/Houdini.framework/Versions/" + HoudiniVersion;
				}
				else if( Target.Platform == UnrealTargetPlatform.Linux )
				{
					HFSPath = "/opt/hfs" + HoudiniVersion;
				}
			}

			string HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";

			if( Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 )
			{
				HAPILib = "libHAPI.a";
				HAPILibPath = HFSPath + "/custom/houdini/dsolib";
			}
			else if( Target.Platform == UnrealTargetPlatform.Mac )
			{
				HAPILib = "libHAPI.dylib";
				HAPILibPath = HFSPath + "/dsolib";
			}
			else if( Target.Platform == UnrealTargetPlatform.Linux )
			{
				HAPILib = "libHAPI.so";
				HAPILibPath = HFSPath + "/dsolib";
			}
			
			PublicLibraryPaths.Add(HAPILibPath);
			PublicAdditionalLibraries.Add(HAPILibPath + "/" + HAPILib);

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
					HAPIIncludePath
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
					"Projects",
					"PropertyEditor",
					"ContentBrowser",
					"RawMesh",
					"TargetPlatform",
					"LevelEditor",
					"MainFrame",
					"EditorStyle",
					"EditorWidgets"
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
