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
set 2 inputs: a geometry input (a cube) and a curve input (a helix). The
inputs are set during post instantiation (before the first cook). After the
first cook and output creation (post processing) the input structure is fetched
and logged.

"""
import math

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/copy_to_curve_1_0.copy_to_curve_1_0'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def get_geo_asset_path():
    return '/Engine/BasicShapes/Cube.Cube'


def get_geo_asset():
    return unreal.load_object(None, get_geo_asset_path())


def configure_inputs(in_wrapper):
    print('configure_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_instantiation_delegate.remove_callable(configure_inputs)

    # Create a geo input
    geo_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPIGeoInput)
    # Set the input objects/assets for this input
    geo_object = get_geo_asset()
    if not geo_input.set_input_objects((geo_object, )):
        # If any errors occurred, get the last error message
        print('Error on geo_input: {0}'.format(geo_input.get_last_error_message()))
    # copy the input data to the HDA as node input 0
    in_wrapper.set_input_at_index(0, geo_input)
    # We can now discard the API input object
    geo_input = None

    # Create a curve input
    curve_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPICurveInput)
    # Create a curve wrapper/helper
    curve_object = unreal.HoudiniPublicAPICurveInputObject(curve_input)
    # Make it a Nurbs curve
    curve_object.set_curve_type(unreal.HoudiniPublicAPICurveType.NURBS)
    # Set the points of the curve, for this example we create a helix
    # consisting of 100 points
    curve_points = []
    for i in range(100):
        t = i / 20.0 * math.pi * 2.0
        x = 100.0 * math.cos(t)
        y = 100.0 * math.sin(t)
        z = i
        curve_points.append(unreal.Transform([x, y, z], [0, 0, 0], [1, 1, 1]))
    curve_object.set_curve_points(curve_points)
    # Error handling/message example: try to set geo_object on curve input
    if not curve_input.set_input_objects((geo_object, )):
        print('Error (example) while setting \'{0}\' on curve input: {1}'.format(
            geo_object.get_name(), curve_input.get_last_error_message()
        ))
    # Set the curve wrapper as an input object
    curve_input.set_input_objects((curve_object, ))
    # Copy the input data to the HDA as node input 1
    in_wrapper.set_input_at_index(1, curve_input)
    # We can now discard the API input object
    curve_input = None

    # Check for errors on the wrapper
    last_error = in_wrapper.get_last_error_message()
    if last_error:
        print('Error on wrapper during input configuration: {0}'.format(last_error))


def print_api_input(in_input):
    print('\t\tInput type: {0}'.format(in_input.__class__))
    print('\t\tbKeepWorldTransform: {0}'.format(in_input.keep_world_transform))
    print('\t\tbImportAsReference: {0}'.format(in_input.import_as_reference))
    if isinstance(in_input, unreal.HoudiniPublicAPIGeoInput):
        print('\t\tbPackBeforeMerge: {0}'.format(in_input.pack_before_merge))
        print('\t\tbExportLODs: {0}'.format(in_input.export_lo_ds))
        print('\t\tbExportSockets: {0}'.format(in_input.export_sockets))
        print('\t\tbExportColliders: {0}'.format(in_input.export_colliders))
    elif isinstance(in_input, unreal.HoudiniPublicAPICurveInput):
        print('\t\tbCookOnCurveChanged: {0}'.format(in_input.cook_on_curve_changed))
        print('\t\tbAddRotAndScaleAttributesOnCurves: {0}'.format(in_input.add_rot_and_scale_attributes_on_curves))

    input_objects = in_input.get_input_objects()
    if not input_objects:
        print('\t\tEmpty input!')
    else:
        print('\t\tNumber of objects in input: {0}'.format(len(input_objects)))
        for idx, input_object in enumerate(input_objects):
            print('\t\t\tInput object #{0}: {1}'.format(idx, input_object))
            if isinstance(input_object, unreal.HoudiniPublicAPICurveInputObject):
                print('\t\t\tbClosed: {0}'.format(input_object.is_closed()))
                print('\t\t\tCurveMethod: {0}'.format(input_object.get_curve_method()))
                print('\t\t\tCurveType: {0}'.format(input_object.get_curve_type()))
                print('\t\t\tReversed: {0}'.format(input_object.is_reversed()))
                print('\t\t\tCurvePoints: {0}'.format(input_object.get_curve_points()))


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
    # Print the input state after the cook and output creation.
    _g_wrapper.on_post_processing_delegate.add_callable(print_inputs)


if __name__ == '__main__':
    run()
