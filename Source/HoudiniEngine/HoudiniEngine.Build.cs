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

    Houdini Version: 14.0.303
    Houdini Engine Version: 1.9.17
    Unreal Version: 4.6.1

*/

using UnrealBuildTool;
using System.IO;

public class HoudiniEngine : ModuleRules
{
	public HoudiniEngine( TargetInfo Target )
	{
		string HFSPath = "";
		string HoudiniVersion = "14.0.303";

		// Check if we are compiling on unsupported platforms.
		if( Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Mac )
		{
			string Err = string.Format( "Houdini Engine : Compiling on unsupported platform." );
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
						string Err = string.Format( "Houdini Engine : Please install Houdini or Houdini Engine {0}", HoudiniVersion );
						System.Console.WriteLine( Err );
						throw new BuildException( Err );
					}
				}
			}
			else if( Target.Platform == UnrealTargetPlatform.Mac )
			{
				HFSPath = "/Library/Frameworks/Houdini.framework/Versions/" + HoudiniVersion + "/Resources";
				if( !Directory.Exists( HFSPath ) )
				{
					string Err = string.Format( "Houdini Engine : Please install Houdini {0}", HoudiniVersion );
					System.Console.WriteLine( Err );
					throw new BuildException( Err );
				}
			}
		}

		string HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";

		if( HFSPath != "" )
		{
			if( Target.Platform == UnrealTargetPlatform.Win64 )
			{
				Definitions.Add( "HOUDINI_ENGINE_HFS_PATH=\"" + HFSPath + "\"" );
			}
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

		// Add common dependencies.
		PublicDependencyModuleNames.Add( "Core" );
		PublicDependencyModuleNames.Add( "CoreUObject" );
		PublicDependencyModuleNames.Add( "Engine" );
		PublicDependencyModuleNames.Add( "RenderCore" );
		PublicDependencyModuleNames.Add( "ShaderCore" );
		PublicDependencyModuleNames.Add( "InputCore" );
		PublicDependencyModuleNames.Add( "RHI" );
		PublicDependencyModuleNames.Add( "Settings" );
		PublicDependencyModuleNames.Add( "RawMesh" );
		PublicDependencyModuleNames.Add( "TargetPlatform" );
		PublicDependencyModuleNames.Add( "AppFramework" );
		PublicDependencyModuleNames.Add( "Projects" );

		// Add editor specific dependencies.
		if( UEBuildConfiguration.bBuildEditor == true )
		{
			Definitions.Add( "HOUDINI_ENGINE_EDITOR_BUILD" );
			
			PublicDependencyModuleNames.Add( "Slate" );
			PublicDependencyModuleNames.Add( "SlateCore" );
			PublicDependencyModuleNames.Add( "AssetTools");

			PublicDependencyModuleNames.Add( "UnrealEd" );
			PublicDependencyModuleNames.Add( "LevelEditor" );
			PublicDependencyModuleNames.Add( "MainFrame" );
			PublicDependencyModuleNames.Add( "EditorStyle" );
			PublicDependencyModuleNames.Add( "EditorWidgets" );
			PublicDependencyModuleNames.Add( "ContentBrowser" );
			PublicDependencyModuleNames.Add( "PropertyEditor" );
		}

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
