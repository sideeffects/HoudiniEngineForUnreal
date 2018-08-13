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

    Houdini Version: 16.5.561
    Houdini Engine Version: 3.1.14
    Unreal Version: 4.20.1

*/

using UnrealBuildTool;
using System;
using System.IO;

public class HoudiniEngineEditor : ModuleRules
{
    private string GetHFSPath()
    {
        string HoudiniVersion = "16.5.561";
        bool bIsRelease = true;
        string HFSPath = "";

        if ( !bIsRelease )
        {
            // Only use the preset build folder
            return HFSPath;
        }

        // Look for the Houdini install folder for this platform
        PlatformID buildPlatformId = Environment.OSVersion.Platform;
        if (buildPlatformId == PlatformID.Win32NT)
        {
            // Look for the HEngine install path in the registry
            string HEngineRegistry = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Side Effects Software\Houdini Engine {0}", HoudiniVersion);
            string HPath = Microsoft.Win32.Registry.GetValue(HEngineRegistry, "InstallPath", null) as string;
            if ( HPath != null )
            {
                if ( Directory.Exists( HPath ) )
                    return HPath;
            }

            // If we couldn't find the Houdini Engine registry path, try the default one
            string DefaultHPath = "C:/Program Files/Side Effects Software/Houdini Engine " + HoudiniVersion;
            if ( DefaultHPath != HPath )
            {
                if ( Directory.Exists( DefaultHPath ) )
                    return DefaultHPath;
            }

            // Look for the Houdini registry install path for the version the plug-in was compiled for
            string HoudiniRegistry = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Side Effects Software\Houdini {0}", HoudiniVersion);
            HPath = Microsoft.Win32.Registry.GetValue(HoudiniRegistry, "InstallPath", null) as string;
            if ( HPath != null )
            {
                if ( Directory.Exists( HPath ) )
                    return HPath;
            }

            // If we couldn't find the Houdini registry path, try the default one
            DefaultHPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
            if ( DefaultHPath != HPath )
            {
                if ( Directory.Exists( DefaultHPath ) )
                    return DefaultHPath;
            }

            // See if the preset build HFS exists
            if ( Directory.Exists( HFSPath ) )
                return HFSPath;

            // We couldn't find the exact version the plug-in was built for, we can still try with the active version in the registry
            HEngineRegistry = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Side Effects Software";
            string ActiveHEngine = Microsoft.Win32.Registry.GetValue( HEngineRegistry, "ActiveEngineVersion", null ) as string;
            if ( ActiveHEngine != null )
            {
                // See if the latest active HEngine version has the proper major/minor version
                if ( ActiveHEngine.Substring( 0, 4 ) == HoudiniVersion.Substring( 0, 4 ) )
                {
                    HEngineRegistry = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Side Effects Software\Houdini Engine {0}", ActiveHEngine);
                    HPath = Microsoft.Win32.Registry.GetValue( HEngineRegistry, "InstallPath", null ) as string;
                    if ( HPath != null )
                    {
                        if ( Directory.Exists( HPath ) )
                            return HPath;
                    }
                }
            }

            // Active HEngine version didn't match, so try with the active Houdini version
            HoudiniRegistry = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Side Effects Software";
            string ActiveHoudini = Microsoft.Win32.Registry.GetValue( HoudiniRegistry, "ActiveVersion", null ) as string;
            if ( ActiveHoudini != null )
            {
                // See if the latest active Houdini version has the proper major/minor version
                if ( ActiveHoudini.Substring( 0, 4 ) == HoudiniVersion.Substring( 0, 4 ) )
                {
                    HoudiniRegistry = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Side Effects Software\Houdini {0}", ActiveHoudini);
                    HPath = Microsoft.Win32.Registry.GetValue( HoudiniRegistry, "InstallPath", null ) as string;
                    if ( HPath != null )
                    {
                        if ( Directory.Exists( HPath ) )
                            return HPath;
                    }
                }
            }
        }
        else if ( buildPlatformId == PlatformID.MacOSX )
        {
            // Check for Houdini installation.
            string HPath = "/Applications/Houdini/Houdini" + HoudiniVersion + "/Frameworks/Houdini.framework/Versions/Current/Resources";
            if ( Directory.Exists( HPath ) )
                return HPath;

            if ( Directory.Exists( HFSPath ) )
                return HFSPath;
        }
        else if ( buildPlatformId == PlatformID.Unix )
        {
            HFSPath = System.Environment.GetEnvironmentVariable( "HFS" );
            if ( Directory.Exists( HFSPath ) )
            {
                System.Console.WriteLine( "Linux - found HFS:" + HFSPath );
                return HFSPath;
            }
        }
        else
        {
            System.Console.WriteLine( string.Format( "Building on an unknown environment!" ) );
        }

        string Err = string.Format("Houdini Engine : Please install Houdini or Houdini Engine {0}", HoudiniVersion);
        System.Console.WriteLine(Err);

        return "";
    }
    
    public HoudiniEngineEditor( ReadOnlyTargetRules Target ) : base( Target )
    {
        bPrecompile = true;
        PCHUsage = PCHUsageMode.UseSharedPCHs;

        // Check if we are compiling on unsupported platforms.
        if( Target.Platform != UnrealTargetPlatform.Win64 &&
            Target.Platform != UnrealTargetPlatform.Mac &&
            Target.Platform != UnrealTargetPlatform.Linux )
        {
            string Err = string.Format( "Houdini Engine Runtime: Compiling on unsupported platform." );
            System.Console.WriteLine( Err );
            throw new BuildException( Err );
        }

        // Find HFS
        string HFSPath = GetHFSPath();
        HFSPath = HFSPath.Replace("\\", "/");
        if( HFSPath != "" )
        {
            PlatformID buildPlatformId = Environment.OSVersion.Platform;
            if ( buildPlatformId == PlatformID.Win32NT )
            {
                PublicDefinitions.Add("HOUDINI_ENGINE_HFS_PATH_DEFINE=" + HFSPath);
            }
        }

        // Find the HAPI include directory
        string HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";
        if (!Directory.Exists(HAPIIncludePath))
        {
            // Try the custom include path as well in case the toolkit path doesn't exist yet.
            HAPIIncludePath = HFSPath + "/custom/houdini/include/HAPI";

            if (!Directory.Exists(HAPIIncludePath))
            {
                System.Console.WriteLine(string.Format("Couldnt find the HAPI include folder!"));
                HAPIIncludePath = "";
            }
        }

        if (HAPIIncludePath != "")
            PublicIncludePaths.Add(HAPIIncludePath);
    
        // Get the plugin path
        string PluginPath = Path.Combine( ModuleDirectory, "../../" );
        PluginPath = Utils.MakePathRelativeTo(PluginPath, Target.RelativeEnginePath);

        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(PluginPath, "Source/HoudiniEngineRuntime/Public/HAPI"),
                Path.Combine(PluginPath, "Source/HoudiniEngineRuntime/Public"),
                Path.Combine(PluginPath, "Source/HoudiniEngineEditor/Public")
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                "HoudiniEngineEditor/Private",
                "HoudiniEngineRuntime/Private"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PlacementMode"
            }
        );

        // Add common dependencies.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "HoudiniEngineRuntime",
                "Slate",
                "SlateCore",
                "Landscape"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AppFramework",
                "AssetTools",
                "ContentBrowser",
                "DesktopWidgets",
                "EditorStyle",
                "EditorWidgets",
                "Engine",
                "InputCore",
                "LevelEditor",
                "MainFrame",
                "Projects",
                "PropertyEditor",
                "RHI",
                "RawMesh",
                "RenderCore",
                "ShaderCore",
                "TargetPlatform",
                "UnrealEd",
                "ApplicationCore",
                "CurveEditor",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                "PlacementMode",
            }
        );
    }
}
