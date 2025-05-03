# Houdini Engine for Unreal - Version 2

> The source code in this branch is intended to be used with Unreal Engine 5.4 and 5.3, but should also be compatible with previous versions of UE5.

Welcome to the repository for the Houdini Engine For Unreal Plugin.

This plug-in brings Houdini's powerful and flexible procedural workflow into Unreal Engine through Houdini Digital Assets. Artists can interactively adjust asset parameters inside the editor and use Unreal assets as inputs. Houdini's procedural engine will then "cook" the asset and the results will be available in the editor without the need for baking.

Documentation for the plugin is available on the Side FX [Website](https://www.sidefx.com/docs/unreal/).


# Feedback

Please send bug reports, feature requests and questions to [Side FX's support](https://www.sidefx.com/bugs/submit/).


# Compatibility

Currently, the plugins has [binaries](https://github.com/sideeffects/HoudiniEngineForUnreal/releases) that have been built for UE5.4, UE5.3, and is linked with the latest production build of Houdini.

Source code for the plugin is available on this repository for UE5.

> Please note that all UE5.X versions of the plugin use the same source files/branches, the sources in the 5.0 branch are also intended to be used with more recent versions of the plugin (ie 5.4 , 5.3 etc.)

In general, we support the latest two releases of UE5.X, but will try to make the source code compatible with previous versions of Unreal.

# Installing the plugin
01. In this GitHub repository, click **Releases** on the right side. 
02. Download the Houdini Engine version zip file that matches your Houdini version.  
03. Extract the **HoudiniEngine** folder to the **Plugins\Runtime** of your Unreal Directory. You can either copy it to Unreal's engine version directory or your Unreal project directory.

    In this example, Unreal's directory location is `C:\Program Files\Epic Games\UE_5.4\Engine\Plugins\Runtime\HoudiniEngine` and the project directory is `C:\Unreal Projects\MyGameProject\Plugins\HoudiniEngine`

    **Note: For Unreal Engine 5, you must use Unreal's project directory.** 

## Verify the Plug-in works
Once you install the Houdini Engine plug-in, you can verify it's loaded properly. 

01. Open a new or existing Unreal project. 
02. In the **main menu bar**, you can see **Houdini Engine** as a new selection.

You should also check the Houdini Engine plug-in version matches your Houdini Version for the plug-in to work properly.

01. In Unreal Engine main menu bar, click **Edit** then **Plugins**.
02. For Houdini Engine, check the **HX.Y.Z.** version number matches your Houdini version. X.Y.Z means your Houdini Version number.

You can learn how to export an Houdini Digital Assets (HDA), import it into Unreal Engine, and update the asset from [Assets documentation.](https://www.sidefx.com/docs/unreal/_assets.html)

# Building from source

01. Get the UE source code from: https://github.com/EpicGames/UnrealEngine/releases
01. Within the UE source, navigate to `Engine/Plugins/Runtime`, and clone this repo into a folder named `HoudiniEngine`. Alternatively, you can also install the plugin in your project, in the `Plugins/Runtime` directory.
01. Download and install the correct build of 64-bit Houdini. To get the build number, look at the header of `Source/HoudiniEngine/HoudiniEngine.Build.cs`, under `Houdini Version`.
01. Generate the UE Project Files (by running `GenerateProjectFiles`) and build Unreal, either in x64 `Debug Editor` or x64 `Development Editor`.
01. When starting the Unreal Engine editor, go to Plug-ins menu and make sure to enable the `HoudiniEngine v2` plug-in (in the `Rendering` section). Restart UE if you had to enable it.
01. To confirm that the plug-in has been successfully installed and enabled, you can check that the editor main menu bar now has a new "Houdini Engine" menu, between "Edit" and "Window".
01. You should now be able to import Houdini Digital Assets (HDA) `.otl` or `.hda` files or drag and drop them into the `Content Browser`.
01. Once you have an HDA in the `Content Browser` you should be able to drag it into the Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters in the `Details` pane. Modifying any of the parameters will force the asset to recook and possibly update its geometry.


