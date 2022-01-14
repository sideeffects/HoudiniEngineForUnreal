/*
 * Copyright (c) <2021> Side Effects Software Inc.
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

    Houdini Version: 19.0.500
    Houdini Engine Version: 4.2.8
    Unreal Version: 4.27.0

*/

using UnrealBuildTool;
using System;
using System.IO;
using Tools.DotNETCommon;

public class HoudiniEngine : ModuleRules
{
    private string GetHFSPath()
    {
        string HoudiniVersion = "19.0.500";
        bool bIsRelease = true;
        string HFSPath = "";
        string RegistryPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Side Effects Software";
        string Registry32Path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Side Effects Software";
        string log;

        if ( !bIsRelease )
        {
            // Only use the preset build folder
            Log.TraceVerbose("Using stamped HFSPath:" + HFSPath);
            return HFSPath;
        }

        // Look for the Houdini install folder for this platform
        PlatformID buildPlatformId = Environment.OSVersion.Platform;
        if (buildPlatformId == PlatformID.Win32NT)
        {
            // Look for the HEngine install path in the registry
            string HEngineRegistry = RegistryPath + string.Format(@"\Houdini Engine {0}", HoudiniVersion);
            string HPath = Microsoft.Win32.Registry.GetValue(HEngineRegistry, "InstallPath", null) as string;
            if ( HPath != null )
            {
                log = string.Format("Houdini Engine : Looking for Houdini Engine {0} in {1}", HoudiniVersion, HPath );
                Log.TraceVerbose( log );
                if ( Directory.Exists( HPath ) )
                    return HPath;
            }
            
            HEngineRegistry = Registry32Path + string.Format(@"\Houdini Engine {0}", HoudiniVersion);
            HPath = Microsoft.Win32.Registry.GetValue(HEngineRegistry, "InstallPath", null) as string;
            if ( HPath != null )
            {
                log = string.Format("Houdini Engine : Looking for Houdini Engine {0} in {1}", HoudiniVersion, HPath );
                Log.TraceVerbose( log );
                if ( Directory.Exists( HPath ) )
                    return HPath;
            }

            // If we couldn't find the Houdini Engine registry path, try the default one
            string DefaultHPath = "C:/Program Files/Side Effects Software/Houdini Engine " + HoudiniVersion;
            if ( DefaultHPath != HPath )
            {
                log = string.Format("Houdini Engine : Looking for Houdini Engine {0} in {1}", HoudiniVersion, DefaultHPath );
                Log.TraceVerbose( log );
                if ( Directory.Exists( DefaultHPath ) )
                    return DefaultHPath;
            }

            // Look for the Houdini registry install path for the version the plug-in was compiled for
            string HoudiniRegistry = RegistryPath + string.Format(@"\Houdini {0}", HoudiniVersion);
            HPath = Microsoft.Win32.Registry.GetValue(HoudiniRegistry, "InstallPath", null) as string;
            if ( HPath != null )
            {
                log = string.Format("Houdini Engine : Looking for Houdini {0} in {1}", HoudiniVersion, HPath );
                Log.TraceVerbose( log );
                if ( Directory.Exists( HPath ) )
                    return HPath;
            }
            
            // Look for the Houdini registry install path for the version the plug-in was compiled for
            HoudiniRegistry = Registry32Path + string.Format(@"\Houdini {0}", HoudiniVersion);
            HPath = Microsoft.Win32.Registry.GetValue(HoudiniRegistry, "InstallPath", null) as string;
            if ( HPath != null )
            {
                log = string.Format("Houdini Engine : Looking for Houdini {0} in {1}", HoudiniVersion, HPath );
                Log.TraceVerbose( log );
                if ( Directory.Exists( HPath ) )
                    return HPath;
            }

            // If we couldn't find the Houdini registry path, try the default one
            DefaultHPath = "C:/Program Files/Side Effects Software/Houdini " + HoudiniVersion;
            if ( DefaultHPath != HPath )
            {
                log = string.Format("Houdini Engine : Looking for Houdini {0} in {1}", HoudiniVersion, DefaultHPath );
                Log.TraceVerbose( log );
                if ( Directory.Exists( DefaultHPath ) )
                    return DefaultHPath;
            }

            // See if the preset build HFS exists
            if ( Directory.Exists( HFSPath ) )
                return HFSPath;

            log = string.Format("Houdini Engine : Failed to find Houdini {0}, will attempt to build using the latest installed version", HoudiniVersion );
            Log.TraceVerbose( log );

            // We couldn't find the exact version the plug-in was built for, we can still try with the active version in the registry
            string ActiveHEngine = Microsoft.Win32.Registry.GetValue(RegistryPath, "ActiveEngineVersion", null) as string;
            if ( ActiveHEngine == null )
            {
                ActiveHEngine = Microsoft.Win32.Registry.GetValue(Registry32Path, "ActiveEngineVersion", null) as string;
            }
            if ( ActiveHEngine != null )
            {
                // See if the latest active HEngine version has the proper major/minor version
                if ( ActiveHEngine.Substring(0,4) == HoudiniVersion.Substring(0,4) )
                {
                    log = string.Format("Houdini Engine : Found Active Houdini Engine version: {0}", ActiveHEngine );
                    Log.TraceVerbose( log );
                    
                    // Active version contain the patch version that we need to strip off
                    //string[] ActiveVersion = ActiveHEngine.Split(".");

                    HEngineRegistry = RegistryPath + string.Format(@"\Houdini Engine {0}", ActiveHEngine);
                    HPath = Microsoft.Win32.Registry.GetValue(HEngineRegistry, "InstallPath", null) as string;
                    if ( HPath != null )
                    {
                        log = string.Format("Houdini Engine : Looking for Houdini Engine {0} in {1}", ActiveHEngine, HPath );
                        Log.TraceVerbose( log ); 
                        if ( Directory.Exists( HPath ) )
                            return HPath;
                    }
                }
            }

            // Active HEngine version didn't match, so try with the active Houdini version
            string ActiveHoudini = Microsoft.Win32.Registry.GetValue(RegistryPath, "ActiveVersion", null) as string;
            if ( ActiveHoudini == null )
            {
                ActiveHoudini = Microsoft.Win32.Registry.GetValue(Registry32Path, "ActiveVersion", null) as string;
            }
            if ( ActiveHoudini != null )
            {
                // See if the latest active Houdini version has the proper major/minor version
                if ( ActiveHoudini.Substring(0,4) == HoudiniVersion.Substring(0,4) )
                {
                    log = string.Format("Houdini Engine : Found Active Houdini version: {0}", ActiveHoudini );
                    Log.TraceVerbose( log );

                    HoudiniRegistry = RegistryPath + string.Format(@"\Houdini {0}", ActiveHoudini);
                    HPath = Microsoft.Win32.Registry.GetValue(HoudiniRegistry, "InstallPath", null) as string;
                    if ( HPath != null )
                    {
                        log = string.Format("Houdini Engine : Looking for Houdini {0} in {1}", ActiveHoudini, HPath );
                        Log.TraceVerbose( log );
                        
                        if ( Directory.Exists( HPath ) )
                            return HPath;
                    }
                }
            }
        }
        else if (buildPlatformId == PlatformID.MacOSX ||
            (buildPlatformId == PlatformID.Unix && File.Exists("/System/Library/CoreServices/SystemVersion.plist")))
        {
            // Check for Houdini installation.
            string HPath = "/Applications/Houdini/Houdini" + HoudiniVersion + "/Frameworks/Houdini.framework/Versions/Current/Resources";
            if ( Directory.Exists( HPath ) )
                return HPath;

            HPath = "/Users/Shared/Houdini/HoudiniIndieSteam/Frameworks/Houdini.framework/Versions/Current/Resources";
            if (Directory.Exists(HPath))
                return HPath;

            if ( Directory.Exists( HFSPath ) )
                return HFSPath;
        }
        else if ( buildPlatformId == PlatformID.Unix )
        {
            HFSPath = System.Environment.GetEnvironmentVariable("HFS");
            if ( Directory.Exists( HFSPath ) )
            {
                Log.TraceVerbose("Unix using $HFS: " + HFSPath);
                return HFSPath;
            }
        }
        else
        {
            Log.TraceVerbose(string.Format("Building on an unknown environment!"));
        }

        string Info = string.Format("Houdini Engine : Houdini {0} could not be found. Houdini Engine will not be available in this build.", HoudiniVersion);
        Log.TraceInformationOnce(Info);

        return "";
    }

    public HoudiniEngine( ReadOnlyTargetRules Target ) : base( Target )
    {
        bPrecompile = true;
        PCHUsage = PCHUsageMode.NoSharedPCHs;
        PrivatePCHHeaderFile = "Private/HoudiniEnginePrivatePCH.h";

        // Check if we are compiling on unsupported platforms.
        if ( Target.Platform != UnrealTargetPlatform.Win64 &&
            Target.Platform != UnrealTargetPlatform.Mac &&
            Target.Platform != UnrealTargetPlatform.Linux )
        {
            string Err = string.Format( "Houdini Engine : Compiling for unsupported platform." );
            Log.TraceError( Err );
            throw new BuildException( Err );
        }

        if (Target.bBuildEditor == false)
        {
            // Actual Runtime Houdini Engine, currently not supported
            string Err = string.Format( "Houdini Engine : Building as a runtime module is currently not supported." );
            Log.TraceError( Err );
            throw new BuildException( Err );
        }

        // Find HFS
        string HFSPath = GetHFSPath();
        HFSPath = HFSPath.Replace("\\", "/");

        if( HFSPath != "" )
        {
            string log = string.Format("Houdini Engine : Found Houdini in {0}", HFSPath );
            Log.TraceInformationOnce( log ); 

            PlatformID buildPlatformId = Environment.OSVersion.Platform;
            if (buildPlatformId == PlatformID.Win32NT)
            {
                PublicDefinitions.Add("HOUDINI_ENGINE_HFS_PATH_DEFINE=" + HFSPath);
            }
        }

        PublicIncludePaths.AddRange(
            new string[]
            {
                //Path.Combine(ModuleDirectory, "Public/HAPI"),
                //"HoudiniEngineRuntime/Public/HAPI"
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
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
                "HoudiniEngineRuntime",
                "RenderCore",
                "InputCore",
                "RHI",
                "Foliage",
                "Landscape",
                "StaticMeshDescription",
                "Chaos",
                "GeometryCollectionEngine",
                "FieldSystemEngine"
            }
        );

       PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Landscape",
                "PhysicsCore"
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
                    "Kismet",
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
                    "LandscapeEditor",
                    "MeshDescription",
                    "MeshDescriptionOperations",
                    "WorldBrowser",
                    "Messaging",
                    "SlateNullRenderer"
                }
            );
        }

        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "DirectoryWatcher"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
                "DirectoryWatcher"
            }
        );
    }
}
