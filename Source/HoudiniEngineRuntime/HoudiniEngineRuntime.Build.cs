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

    Houdini Version: 17.0.297
    Houdini Engine Version: 3.2.20
    Unreal Version: 4.20.0

*/

using UnrealBuildTool;
using System;
using System.IO;

public class HoudiniEngineRuntime : ModuleRules
{
    private string GetHFSPath()
    {
        string HoudiniVersion = "17.0.297";
        bool bIsRelease = true;
        string HFSPath = "";

        if ( !bIsRelease )
        {
            // Only use the preset build folder
            System.Console.WriteLine("Using HFS:" + HFSPath);
            return HFSPath;
        }

        // Look for the Houdini install folder for this platform
        PlatformID buildPlatformId = Environment.OSVersion.Platform;
        if (buildPlatformId == PlatformID.Win32NT)
        {
            // TODO: Look in the registry!
            // TODO: Use the registry's active version to use latest
            
            // Look for the HEngine registry install path
            //string HEngineRegistry = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Side Effects Software\Houdini Engine {0}", HoudiniVersion);
            //string HFS = Microsoft.Win32.Registry.GetValue(HEngineRegistry, "InstallPath", null) as string;
            
            // Look for the Houdini registry install path
            //string HoudiniRegistry = string.Format(@"HKEY_LOCAL_MACHINE\SOFTWARE\Side Effects Software\Houdini {0}", HoudiniVersion);
            //string HFS = Microsoft.Win32.Registry.GetValue(HoudiniRegistry, "InstallPath", null) as string;

            // We first check if Houdini Engine is installed.
            string HPath = "C:/Program Files/Side Effects Software/Houdini Engine " + HoudiniVersion;
            if (Directory.Exists(HPath))
                return HPath;

            // If Houdini Engine is not installed, we check for Houdini installation.
            HPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
            if (Directory.Exists(HPath))
                return HPath;

            // Finally, see if the preset build HFS exists
            if (Directory.Exists(HFSPath))
                return HFSPath;

            string Err = string.Format("Houdini Engine : Please install Houdini or Houdini Engine {0}", HoudiniVersion);
            System.Console.WriteLine(Err);
        }
        else if (buildPlatformId == PlatformID.MacOSX)
        {
            // Check for Houdini installation.
            string HPath = "/Applications/Houdini/Houdini" + HoudiniVersion + "/Frameworks/Houdini.framework/Versions/Current/Resources";
            if (Directory.Exists(HPath))
                return HPath;

            if (Directory.Exists(HFSPath))
                return HFSPath;

            string Err = string.Format("Houdini Engine : Please install Houdini {0}", HoudiniVersion);
            System.Console.WriteLine(Err);
        }
        else if (buildPlatformId == PlatformID.Unix)
        {
            HFSPath = System.Environment.GetEnvironmentVariable("HFS");

            if (Directory.Exists(HFSPath))
            {
                System.Console.WriteLine("Linux - found HFS:" + HFSPath);
                return HFSPath;
            }
        }
        else
        {
            System.Console.WriteLine(string.Format("Building on an unknown environment!"));
        }

        return "";
    }

    public HoudiniEngineRuntime( ReadOnlyTargetRules Target ) : base( Target )
    {
        bPrecompile = true;
        PCHUsage = PCHUsageMode.UseSharedPCHs;

        // Check if we are compiling on unsupported platforms.
        if( Target.Platform != UnrealTargetPlatform.Win64 &&
            Target.Platform != UnrealTargetPlatform.Mac &&
            Target.Platform != UnrealTargetPlatform.Linux &&
            Target.Platform != UnrealTargetPlatform.Switch )
        {
            System.Console.WriteLine( string.Format( "Houdini Engine : Compiling on untested platform.  Please let us know how it goes!" ) );
        }

        // Find HFS
        string HFSPath = GetHFSPath();
        if( HFSPath != "" )
        {
            PlatformID buildPlatformId = Environment.OSVersion.Platform;
            if (buildPlatformId == PlatformID.Win32NT)
            {
                PublicDefinitions.Add("HOUDINI_ENGINE_HFS_PATH_DEFINE=" + HFSPath);
            }
        }

        // Find the HAPI include directory
        string HAPIIncludePath = HFSPath + "/toolkit/include/HAPI";
        if (!Directory.Exists(HAPIIncludePath))
        {
            // Add the custom include path as well in case the toolkit path doesn't exist yet.
            HAPIIncludePath = HFSPath + "/custom/houdini/include/HAPI";

            if (!Directory.Exists(HAPIIncludePath))
            {
                System.Console.WriteLine(string.Format("Couldn't find the HAPI include folder!"));
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
                Path.Combine(PluginPath, "Source/HoudiniEngineRuntime/Public")
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

       PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                // ... add private dependencies that you statically link with here ...
            }
       );

       if (Target.bBuildEditor == true)
       {
            PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                    "AppFramework",
                    "AssetTools",
                    "EditorStyle",
                    "EditorWidgets",
                    "LevelEditor",
                    "MainFrame",
                    "MeshPaint",
                    "Projects",
                    "PropertyEditor",
                    "RawMesh",
                    "Settings",
                    "Slate",
                    "SlateCore",
                    "TargetPlatform",
                    "UnrealEd",
                    "ApplicationCore",
                }
            );
        }

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
        );
    }
}
