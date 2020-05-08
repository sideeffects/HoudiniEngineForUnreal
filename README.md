# Houdini Engine for Unreal
Houdini Engine for Unreal Engine is a plug-in that allows integration of Houdini technology into Unreal.

This plug-in brings Houdini's powerful and flexible procedural workflow into Unreal Engine through Houdini Digital Assets. Artists can interactively adjust asset parameters inside the editor and use Unreal geometries as asset inputs. Houdini's procedural engine will then "cook" the asset and the results will be available in the editor without the need for baking.

You can use the [Houdini Engine for UE4 Forum](http://www.sidefx.com/forum/51/) on the SideFX website, or join the [Think Procedural](https://discord.gg/b8U5Hdy) discord server to ask questions, discuss new features, and share your ideas.

For more information:

* [Houdini Engine for Unreal](https://www.sidefx.com/products/houdini-engine/ue4-plug-in/)
* [Documentation](http://www.sidefx.com/docs/unreal/)
* [FAQ](https://www.sidefx.com/faq/houdini-engine-faq/)

For support and reporting bugs:

* [Houdini Engine for UE4 Forum](http://www.sidefx.com/forum/51/)
* [Bug Submission](https://www.sidefx.com/bugs/submit/)

You can see the latest updates and bug fixes made to the plugin in the [Daily Changelog](https://www.sidefx.com/changelog/?journal=16.5&categories=52&body=&version=&build_0=&build_1=&show_versions=on&show_compatibility=on&items_per_page=100).

# Installing from Source
01. Get the UE4 source code from: https://github.com/EpicGames/UnrealEngine/releases. This requires signing up for an Epic account: https://github.com/EpicGames/Signup
01. Within the UE4 source, navigate to `Engine/Plugins/Runtime`, and clone this repo into a folder named `HoudiniEngine`.
01. Download and install the correct build of 64-bit Houdini. To get the build number, look at the header of `Source/HoudiniEngineRuntime/HoudiniEngineRuntime.Build.cs`, under `Houdini Version`.
01. Generate the UE4 Project Files (by running `GenerateProjectFiles`) and build Unreal, either in x64 `Debug Editor` or x64 `Development Editor`.
01. When starting the Unreal Engine editor, go to Plug-ins menu and make sure to enable the `HoudiniEngine` plug-in (it's in `Rendering` section). Restart UE4 if you had to enable it.
01. You should now be able to import Houdini Digital Assets (HDA) `.otl` or `.hda` files or drag and drop them into the `Content Browser`.
01. Once you have an HDA in the `Content Browser` you should be able to drag it into Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters in the `Details` pane. Modifying any of the parameters will force the asset to recook and possibly update its geometry.

*The source code on this github is only automatically updated when a change in the plugin's source code is made. This could lead to the plugin's build.cs files to reference a daily build of Houdini that is not available anymore. If this happens, you simply need to update the version number in the HoudiniEngineRuntime.build.cs  and HoudiniEngineEditor.build.cs files to match the latest daily build's version number.*

*The Houdini Engine for Unreal is not officially supported on Linux, but the plug-in can still be compiled from sources. Since setting up the Houdini environment is required for the session (HARS process) to be created successfully,  you need to call "source houdini_setup" in a shell from the installed Houdini directory that matches the plug-in prior to launching the Unreal Editor from that shell.*
