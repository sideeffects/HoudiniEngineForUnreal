# Copyright (c) <2021> Side Effects Software Inc.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. The name of Side Effects Software may not be used to endorse or
#     promote products derived from this software without specific prior
#     written permission.
#
#  THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
#  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
#  NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
#  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

""" Example script using the API to instantiate an HDA and set a landscape
input.

"""

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/hilly_landscape_erode_1_0.hilly_landscape_erode_1_0'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def load_map():
    return unreal.EditorLoadingAndSavingUtils.load_map('/HoudiniEngine/Examples/Maps/LandscapeInputExample.LandscapeInputExample')

def get_landscape():
    return unreal.EditorLevelLibrary.get_actor_reference('PersistentLevel.Landscape_0')

def configure_inputs(in_wrapper):
    print('configure_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_instantiation_delegate.remove_callable(configure_inputs)

    # Create a landscape input
    landscape_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPILandscapeInput)
    # Send landscapes as heightfields
    landscape_input.landscape_export_type = unreal.HoudiniLandscapeExportType.HEIGHTFIELD
    # Keep world transform
    landscape_input.keep_world_transform = True
    # Set the input objects/assets for this input
    landscape_object = get_landscape()
    landscape_input.set_input_objects((landscape_object, ))
    # copy the input data to the HDA as node input 0
    in_wrapper.set_input_at_index(0, landscape_input)
    # We can now discard the API input object
    landscape_input = None


def print_api_input(in_input):
    print('\t\tInput type: {0}'.format(in_input.__class__))
    print('\t\tbKeepWorldTransform: {0}'.format(in_input.keep_world_transform))
    if isinstance(in_input, unreal.HoudiniPublicAPILandscapeInput):
        print('\t\tbLandscapeAutoSelectComponent: {0}'.format(in_input.landscape_auto_select_component))
        print('\t\tbLandscapeExportLighting: {0}'.format(in_input.landscape_export_lighting))
        print('\t\tbLandscapeExportMaterials: {0}'.format(in_input.landscape_export_materials))
        print('\t\tbLandscapeExportNormalizedUVs: {0}'.format(in_input.landscape_export_normalized_u_vs))
        print('\t\tbLandscapeExportSelectionOnly: {0}'.format(in_input.landscape_export_selection_only))
        print('\t\tbLandscapeExportTileUVs: {0}'.format(in_input.landscape_export_tile_u_vs))
        print('\t\tbLandscapeExportType: {0}'.format(in_input.landscape_export_type))

    input_objects = in_input.get_input_objects()
    if not input_objects:
        print('\t\tEmpty input!')
    else:
        print('\t\tNumber of objects in input: {0}'.format(len(input_objects)))
        for idx, input_object in enumerate(input_objects):
            print('\t\t\tInput object #{0}: {1}'.format(idx, input_object))


def print_inputs(in_wrapper):
    print('print_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_processing_delegate.remove_callable(print_inputs)

    # Fetch inputs, iterate over it and log
    node_inputs = in_wrapper.get_inputs_at_indices()
    parm_inputs = in_wrapper.get_input_parameters()

    if not node_inputs:
        print('No node inputs found!')
    else:
        print('Number of node inputs: {0}'.format(len(node_inputs)))
        for input_index, input_wrapper in node_inputs.items():
            print('\tInput index: {0}'.format(input_index))
            print_api_input(input_wrapper)

    if not parm_inputs:
        print('No parameter inputs found!')
    else:
        print('Number of parameter inputs: {0}'.format(len(parm_inputs)))
        for parm_name, input_wrapper in parm_inputs.items():
            print('\tInput parameter name: {0}'.format(parm_name))
            print_api_input(input_wrapper)


def run():
    # Load the example map
    load_map()

    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Configure inputs on_post_instantiation, after instantiation, but before first cook
    _g_wrapper.on_post_instantiation_delegate.add_callable(configure_inputs)
    # Print the input state after the cook and output creation.
    _g_wrapper.on_post_processing_delegate.add_callable(print_inputs)


if __name__ == '__main__':
    run()
