# Houdini Engine for Unreal Engine
Houdini Engine for Unreal Engine is a Unreal Engine plug-in that allows deep integration of
Houdini technology into Unreal Engine through the use of Houdini Engine.

This plug-in brings Houdini's powerful and flexible procedural workflow into
Unreal Engine through Houdini Digital Assets. Artists can interactively adjust the
asset's parameters inside Unreal Engine, and use Unreal Engine geometries as an asset's inputs.
Houdini's procedural engine will then "cook" the asset and the results will be
available right inside Unreal Engine.

## Supported Unreal Engine versions
Currently, the supported Unreal Engine versions are:

* 4.2

## Installing from Source
1. Have a local copy of Unreal Engine 4.2 checked out. Inside Unreal Engine copy navigate to Engine/Plugins/Runtime .
2. Clone the Houdini Engine Unreal plugin into this folder. Make sure the checked out folder is named HoudiniEngine .
3. Download and install the correct build of 64-bit Houdini.
4. Generate Visual Studio solution (by running GenerateProjectFiles.bat) and build Unreal Engine.
5. When starting Unreal Engine editor, go to Plugins menu and enable HoudiniEngine plugin (it's in Rendering section). 
6. Restart Unreal Engine.
7. You should now be able to load (import OTL files and drop OTL files into Content Browser) Houdini Digital Assets. Asset cooking will be done in a separate thread and Thumbnail for the asset will be generated asynchronously.
8. Once you have a Houdini Digital Asset in Content Browser you should be able to drag it into Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters in the Details panel. Modifying any of the parameters will force asset to recook and possibly update its geometry.
