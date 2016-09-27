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

    Houdini Version: 15.5.611
    Houdini Engine Version: 2.0.24
    Unreal Version: 4.12.0

*/

using UnrealBuildTool;
using System.IO;

public class HoudiniEngineEditor : ModuleRules
{
	public HoudiniEngineEditor( TargetInfo Target )
	{
		bool bIsRelease = true;
		string HFSPath = "";
		string HoudiniVersion = "15.5.611";

		// Check if we are compiling on unsupported platforms.
		if( Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Mac &&
			Target.Platform != UnrealTargetPlatform.Linux )
		{
			string Err = string.Format( "Houdini Engine Runtime: Compiling on unsupported platform." );
			System.Console.WriteLine( Err );
			throw new BuildException( Err );
		}

		if( bIsRelease )
		{
			if( Target.Platform == UnrealTargetPlatform.Win64 )
			{
				// We first check if Houdini Engine is installed.
				string HPath = "C:/Program Files/Side Effects Software/Houdini Engine " + HoudiniVersion;
				if( !Directory.Exists( HPath ) )
				{
					// If Houdini Engine is not installed, we check for Houdini installation.
					HPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
					if( !Directory.Exists( HPath ) )
					{
						if ( !Directory.Exists( HFSPath ) )
						{
							string Err = string.Format( "Houdini Engine : Please install Houdini or Houdini Engine {0}", HoudiniVersion );
							System.Console.WriteLine( Err );
						}
					}
					else
					{
						HFSPath = HPath;
					}
				}
				else
				{
					HFSPath = HPath;
				}
			}
			else if( Target.Platform == UnrealTargetPlatform.Mac )
			{
				string HPath = "/Library/Frameworks/Houdini.framework/Versions/" + HoudiniVersion + "/Resources";
				if( !Directory.Exists( HPath ) )
				{
					if ( !Directory.Exists( HFSPath ) )
					{
						string Err = string.Format( "Houdini Engine : Please install Houdini {0}", HoudiniVersion );
						System.Console.WriteLine( Err );
					}
				}
				else
				{
					HFSPath = HPath;
				}
			}
			else
			{
				HFSPath = System.Environment.GetEnvironmentVariable( "HFS" );
				System.Console.WriteLine( "Linux - found HFS:" + HFSPath );
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

			// Add the custom include path as well in case the toolkit path doesn't exist yet.
			PublicIncludePaths.Add( HFSPath + "/custom/houdini/include/HAPI" );
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
				"DesktopWidgets",
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
