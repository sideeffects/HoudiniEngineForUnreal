# Houdini Engine for Unreal
Houdini Engine for Unreal Engine is a Unreal Engine plug-in that allows integration of Houdini technology into Unreal Engine through the use of Houdini Engine.

This plug-in brings Houdini's powerful and flexible procedural workflow into Unreal Engine through Houdini Digital Assets. Artists can interactively adjust asset parameters inside Unreal Engine and use Unreal Engine geometries as asset inputs. Houdini's procedural engine will then "cook" the asset and the results will be available inside Unreal Engine.

## Supported Unreal Engine versions
Currently, the supported Unreal Engine versions are:

* 4.6.1
* Please checkout Houdini14.0-Unreal4.7 branch if you require plugin for Unreal Engine 4.7


## Installing from Source
01. Have a local copy of Unreal Engine 4.6 checked out. Inside Unreal Engine copy navigate to Engine/Plugins/Runtime .
02. Clone the Houdini Engine Unreal plugin into this folder. Make sure the checked out folder is named HoudiniEngine .
03. Download and install the correct build of 64-bit Houdini.
04. Verify you have Houdini installed and it matches the version our Plugin is using. Easiest way is to look in commit history or look inside Source/HoudiniEngine/HoudiniEngine.Build.cs file (Houdini Version field). We no longer require you to add Houdini bin folder to PATH as we now load libHAPI dynamically.
05. Generate Visual Studio solution (by running GenerateProjectFiles.bat) and build Unreal Engine (either x64 Debug_Editor/UE4Editor-Debug or x64 Develop_Editor/UE4Editor).
06. When starting Unreal Engine editor, go to Plugins menu and enable HoudiniEngine plugin (it's in Rendering section).
07. Restart Unreal Engine.
08. You should now be able to load (import OTL files and drop OTL files into Content Browser) Houdini Digital Assets.
09. Once you have a Houdini Digital Asset in Content Browser you should be able to drag it into Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters in the Details panel. Modifying any of the parameters will force asset to recook and possibly update its geometry.
