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

""" An example script that uses the API to instantiate an HDA and then
set 2 geometry inputs: one node input and one object path parameter
input. The inputs are set during post instantiation (before the first
cook). After the first cook and output creation (post processing) the 
input structure is fetched and logged.

"""

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/subnet_test_2_0.subnet_test_2_0'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def get_geo_asset_path():
    return '/Engine/BasicShapes/Cube.Cube'


def get_geo_asset():
    return unreal.load_object(None, get_geo_asset_path())


def get_cylinder_asset_path():
    return '/Engine/BasicShapes/Cylinder.Cylinder'


def get_cylinder_asset():
    return unreal.load_object(None, get_cylinder_asset_path())


def configure_inputs(in_wrapper):
    print('configure_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_instantiation_delegate.remove_callable(configure_inputs)

    # Deprecated input functions
    # in_wrapper.set_input_type(0, unreal.HoudiniInputType.GEOMETRY)
    # in_wrapper.set_input_objects(0, (get_geo_asset(), ))

    # in_wrapper.set_input_type(1, unreal.HoudiniInputType.GEOMETRY)
    # in_wrapper.set_input_objects(1, (get_geo_asset(), ))
    # in_wrapper.set_input_import_as_reference(1, True)

    # Create a geometry input
    geo_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPIGeoInput)
    # Set the input objects/assets for this input
    geo_asset = get_geo_asset()
    geo_input.set_input_objects((geo_asset, ))
    # Set the transform of the input geo
    geo_input.set_input_object_transform_offset(
        0,
        unreal.Transform(
            (200, 0, 100),
            (45, 0, 45),
            (2, 2, 2),
        )
    )
    # copy the input data to the HDA as node input 0
    in_wrapper.set_input_at_index(0, geo_input)
    # We can now discard the API input object
    geo_input = None

    # Create a another geometry input
    geo_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPIGeoInput)
    # Set the input objects/assets for this input (cylinder in this case)
    geo_asset = get_cylinder_asset()
    geo_input.set_input_objects((geo_asset, ))
    # Set the transform of the input geo
    geo_input.set_input_object_transform_offset(
        0,
        unreal.Transform(
            (-200, 0, 0),
            (0, 0, 0),
            (2, 2, 2),
        )
    )
    # copy the input data to the HDA as input parameter 'objpath1'
    in_wrapper.set_input_parameter('objpath1', geo_input)
    # We can now discard the API input object
    geo_input = None

    # Set the subnet_test HDA to output its first input
    in_wrapper.set_int_parameter_value('enable_geo', 1)


def print_api_input(in_input):
    print('\t\tInput type: {0}'.format(in_input.__class__))
    print('\t\tbKeepWorldTransform: {0}'.format(in_input.keep_world_transform))
    print('\t\tbImportAsReference: {0}'.format(in_input.import_as_reference))
    if isinstance(in_input, unreal.HoudiniPublicAPIGeoInput):
        print('\t\tbPackBeforeMerge: {0}'.format(in_input.pack_before_merge))
        print('\t\tbExportLODs: {0}'.format(in_input.export_lo_ds))
        print('\t\tbExportSockets: {0}'.format(in_input.export_sockets))
        print('\t\tbExportColliders: {0}'.format(in_input.export_colliders))

    input_objects = in_input.get_input_objects()
    if not input_objects:
        print('\t\tEmpty input!')
    else:
        print('\t\tNumber of objects in input: {0}'.format(len(input_objects)))
        for idx, input_object in enumerate(input_objects):
            print('\t\t\tInput object #{0}: {1}'.format(idx, input_object))
            if hasattr(in_input, 'get_input_object_transform_offset'):
                print('\t\t\tObject Transform Offset: {0}'.format(in_input.get_input_object_transform_offset(idx)))


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
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Configure inputs on_post_instantiation, after instantiation, but before first cook
    _g_wrapper.on_post_instantiation_delegate.add_callable(configure_inputs)
    # Bind on_post_processing, after cook + output creation
    _g_wrapper.on_post_processing_delegate.add_callable(print_inputs)


if __name__ == '__main__':
    run()
