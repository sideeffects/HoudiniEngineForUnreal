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

	Houdini Version: 14.0.188
	Houdini Engine Version: 1.9.7
	Unreal Version: 4.6.0

 */

namespace UnrealBuildTool.Rules
{
	public class HoudiniEngine : ModuleRules
	{
		public HoudiniEngine(TargetInfo Target)
		{
			string HFSPath = "";
			string HoudiniVersion = "14.0.188";
			string HoudiniEngineVersion = "1.9.7";

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

			Definitions.Add("HOUDINI_ENGINE_HFS_PATH=\"" + HFSPath + "\"");
			Definitions.Add("HOUDINI_ENGINE_HOUDINI_VERSION=\"" + HoudiniVersion + "\"");

			{
				string[] VersionTokens = HoudiniVersion.Split(new char[] { '.' });

				int HoudiniMajor = 0;
				int HoudiniMinor = 0;
				int HoudiniBuild = 0;

				if(3 == VersionTokens.Length)
				{
					System.Int32.TryParse(VersionTokens[0], out HoudiniMajor);
					System.Int32.TryParse(VersionTokens[1], out HoudiniMinor);
					System.Int32.TryParse(VersionTokens[2], out HoudiniBuild);
				}

				Definitions.Add(string.Format("HOUDINI_ENGINE_HOUDINI_MAJOR={0}", HoudiniMajor));
				Definitions.Add(string.Format("HOUDINI_ENGINE_HOUDINI_MINOR={0}", HoudiniMinor));
				Definitions.Add(string.Format("HOUDINI_ENGINE_HOUDINI_BUILD={0}", HoudiniBuild));
			}

			{
				string[] VersionTokens = HoudiniEngineVersion.Split(new char[] { '.' });

				int HoudiniEngineMajor = 0;
				int HoudiniEngineMinor = 0;
				int HoudiniEngineApi = 0;

				if(3 == VersionTokens.Length)
				{
					System.Int32.TryParse(VersionTokens[0], out HoudiniEngineMajor);
					System.Int32.TryParse(VersionTokens[1], out HoudiniEngineMinor);
					System.Int32.TryParse(VersionTokens[2], out HoudiniEngineApi);
				}

				Definitions.Add(string.Format("HOUDINI_ENGINE_HOUDINI_ENGINE_MAJOR={0}", HoudiniEngineMajor));
				Definitions.Add(string.Format("HOUDINI_ENGINE_HOUDINI_ENGINE_MINOR={0}", HoudiniEngineMinor));
				Definitions.Add(string.Format("HOUDINI_ENGINE_HOUDINI_ENGINE_API={0}", HoudiniEngineApi));
			}

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
					"EditorWidgets",
					"AppFramework"
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
