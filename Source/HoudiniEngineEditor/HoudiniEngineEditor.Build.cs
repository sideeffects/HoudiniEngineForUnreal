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

    Houdini Version: 15.0.377
    Houdini Engine Version: 2.0.19
    Unreal Version: 4.9.0

*/

using UnrealBuildTool;
using System.IO;

public class HoudiniEngineEditor : ModuleRules
{
	public HoudiniEngineEditor( TargetInfo Target )
	{
		string HFSPath = "";
		string HoudiniVersion = "15.0.377";

		// Check if we are compiling on unsupported platforms.
		if( Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Mac )
		{
			string Err = string.Format( "Houdini Engine Runtime: Compiling on unsupported platform." );
			System.Console.WriteLine( Err );
			throw new BuildException( Err );
		}

		if( HFSPath == "" )
		{
			if( Target.Platform == UnrealTargetPlatform.Win64 )
			{
				// We first check if Houdini Engine is installed.
				HFSPath = "C:/Program Files/Side Effects Software/Houdini Engine " + HoudiniVersion;
				if( !Directory.Exists( HFSPath ) )
				{
					// If Houdini Engine is not installed, we check for Houdini installation.
					HFSPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
					if( !Directory.Exists( HFSPath ) )
					{
						string Err = string.Format( "Houdini Engine Runtime: Please install Houdini or Houdini Engine {0}", HoudiniVersion );
						System.Console.WriteLine( Err );
					}
				}
			}
			else if( Target.Platform == UnrealTargetPlatform.Mac )
			{
				HFSPath = "/Library/Frameworks/Houdini.framework/Versions/" + HoudiniVersion + "/Resources";
				if( !Directory.Exists( HFSPath ) )
				{
					string Err = string.Format( "Houdini Engine Runtime: Please install Houdini {0}", HoudiniVersion );
					System.Console.WriteLine( Err );
				}
			}
		}

		string HAPIIncludePath = "";

		if( HFSPath != "" )
		{
			HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";

			if( Target.Platform == UnrealTargetPlatform.Win64 )
			{
				Definitions.Add( "HOUDINI_ENGINE_HFS_PATH_DEFINE=" + HFSPath );
			}
		}

		if( HAPIIncludePath != "" )
		{
			PublicIncludePaths.Add( HAPIIncludePath );
		}

		PublicIncludePaths.AddRange(
			new string[] {
				"HoudiniEngineRuntime/Public/HAPI",
				"HoudiniEngineRuntime/Public",
				"HoudiniEngineEditor/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"HoudiniEngineEditor/Private",
				"HoudiniEngineRuntime/Private"
			}
		);

		// Add common dependencies.
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
				"AppFramework",
				"HoudiniEngineRuntime"
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
