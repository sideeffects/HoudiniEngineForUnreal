/*
 * Copyright (c) <2017> Side Effects Software Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

    Houdini Version: 16.5.197
    Houdini Engine Version: 3.1.5
    Unreal Version: 4.16.0

*/

using UnrealBuildTool;
using System;
using System.IO;

public class HoudiniEngineRuntime : ModuleRules
{
	public HoudiniEngineRuntime( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = PCHUsageMode.UseSharedPCHs;
		bool bIsRelease = true;
		string HFSPath = "";
		string HoudiniVersion = "16.5.197";
		PlatformID platformId = Environment.OSVersion.Platform;

		// Check if we are compiling on unsupported platforms.
		if( Target.Platform != UnrealTargetPlatform.Win64 &&
		    Target.Platform != UnrealTargetPlatform.Mac &&
		    Target.Platform != UnrealTargetPlatform.Linux &&
		    Target.Platform != UnrealTargetPlatform.Switch )
		{
			System.Console.WriteLine( string.Format( "Houdini Engine : Compiling on untested platform.  Please let us know how it goes!" ) );
		}

		if( bIsRelease )
		{
			if( platformId == PlatformID.Win32NT )
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
			else if( platformId == PlatformID.MacOSX )
			{
				string HPath = "/Applications/Houdini/Houdini" + HoudiniVersion + "/Frameworks/Houdini.framework/Versions/Current/Resources";
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
			else if( platformId == PlatformID.Unix )
			{
				HFSPath = System.Environment.GetEnvironmentVariable( "HFS" );
				System.Console.WriteLine( "Linux - found HFS:" + HFSPath );
			}
			else
			{
				System.Console.WriteLine( string.Format("Unknown environment!") );
			}
		}

		string HAPIIncludePath = "";

		if( HFSPath != "" )
		{
			HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";

			if( platformId == PlatformID.Win32NT )
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
				"HoudiniEngineRuntime/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
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
					"RawMesh",
					"DesktopWidgets",
					"Settings"
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
