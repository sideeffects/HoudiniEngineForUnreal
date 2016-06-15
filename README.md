# Houdini Engine for Unreal
Houdini Engine for Unreal Engine is a plug-in that allows integration of Houdini technology into Unreal.

This plug-in brings Houdini's powerful and flexible procedural workflow into Unreal Engine through Houdini Digital Assets. Artists can interactively adjust asset parameters inside the editor and use Unreal geometries as asset inputs. Houdini's procedural engine will then "cook" the asset and the results will be available in the editor without the need for baking.

You can use the [Houdini Engine for UE4 Forum](http://www.sidefx.com/forum/51/) on the SideFX website to ask questions, discuss new features work, and share your ideas.

# Installing from Source
01. Get the UE4 source code from: https://github.com/EpicGames/UnrealEngine/releases
01. Within the UE4 source, navigate to `Engine/Plugins/Runtime`, and clone this repo into a folder named `HoudiniEngine`.
01. Download and install the correct build of 64-bit Houdini. To get the build number, look at the header of `Source/HoudiniEngineRuntime/HoudiniEngineRuntime.Build.cs`, under `Houdini Version`.
01. Generate the UE4 Visual Studio solution (by running `GenerateProjectFiles.bat`) and build UE4 (either x64 `Debug Editor` or x64 `Development Editor`).
01. When starting Unreal Engine editor, go to Plugins menu and make sure to enable the `HoudiniEngine` plugin (it's in `Rendering` section). Restart UE4 if you had to enable it.
01. You should now be able to import Houdini Digital Assets (HDA) `.otl` or `.hda` files or drag and drop them into the `Content Browser`.
01. Once you have an HDA in the `Content Browser` you should be able to drag it into Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters in the `Details` pane. Modifying any of the parameters will force the asset to recook and possibly update its geometry.
