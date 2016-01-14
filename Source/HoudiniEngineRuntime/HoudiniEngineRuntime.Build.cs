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

    Houdini Version: 15.0.355
    Houdini Engine Version: 2.0.18
    Unreal Version: 4.10.0

*/

using UnrealBuildTool;
using System.IO;

public class HoudiniEngineRuntime : ModuleRules
{
	public HoudiniEngineRuntime( TargetInfo Target )
	{
		bool bIsRelease = true;
		string HFSPath = "";
		string[] HoudiniVersions = { "15.0.355", "15.0.347", "15.0.244.16", };

		// Check if we are compiling on unsupported platforms.
		if( Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Mac )
		{
			string Err = string.Format( "Houdini Engine : Compiling on unsupported platform." );
			System.Console.WriteLine( Err );
			throw new BuildException( Err );
		}

		if( bIsRelease )
		{
            foreach (var HoudiniVersion in HoudiniVersions)
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
						    // [PHX][jsuter] - BEGIN modification        

                                // We've changed this plugin so that not having Houdini installed is no longer fatal (at least on Windows).
                        
                                // string Err = string.Format( "Houdini Engine : Please install Houdini or Houdini Engine {0}", HoudiniVersion );
						        // System.Console.WriteLine( Err );
                                // throw new BuildException( Err );

                            // [PHX][jsuter] - END modification
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
						    throw new BuildException( Err );
					    }
				    }
				    else
				    {
					    HFSPath = HPath;
				    }
			    }

                if (HFSPath != "")
                {
                    break;
                }
		    }
        }
        
		// [PHX][jsuter] - BEGIN modification        
        string HAPIIncludePath = "";
        if ( HFSPath != "" )
        {
            System.Console.WriteLine( "\n [HoudiniEngineRuntime] Note, Houdini plugin is enabled, and relying on \"{0}\").\n", HFSPath);
            HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";            
        }
        else
        {
            // If Houdini or the Houdini Engine is not installed, fall back on the HAPI directory that we included alongside the plugin.
            HAPIIncludePath = Path.GetFullPath(Path.Combine( ModuleDirectory, "../toolkit/include/HAPI" ));
            System.Console.WriteLine( "\n [HoudiniEngineRuntime] Note, Houdini plugin is enabled but you don't have Houdini (or Houdini Engine) installed, you should avoid using Houdini assets in your project. (also, you're relying on HAPI includes from \"{0}\").\n", HAPIIncludePath );
        }
        // [PHX][jsuter] - END modification
    
        if( HFSPath != "" )
		{
			if( Target.Platform == UnrealTargetPlatform.Win64 )
			{
				Definitions.Add( "HOUDINI_ENGINE_HFS_PATH_DEFINE=" + HFSPath );
			}
		}

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				HAPIIncludePath,
				"HoudiniEngineRuntime/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"HoudiniEngineRuntime/Private"
				// ... add other private include paths required here ...
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
				"Settings",
				"Foliage",
				"Landscape"
			}
		);

		if (UEBuildConfiguration.bBuildEditor == true)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"UnrealEd",
					"Slate",
					"SlateCore",
					"Projects",
					"PropertyEditor",
					"ContentBrowser",
					"LevelEditor",
					"MainFrame",
					"EditorStyle",
					"EditorWidgets",
					"AppFramework",
					"TargetPlatform",
					"RawMesh"
				}
			);
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
