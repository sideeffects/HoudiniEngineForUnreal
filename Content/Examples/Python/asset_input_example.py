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

""" An example script that uses the API to instantiate two HDAs. The first HDA
will be used as an input to the second HDA. For the second HDA we set 2 inputs:
an asset input (the first instantiated HDA) and a curve input (a helix). The
inputs are set during post instantiation (before the first cook). After the
first cook and output creation (post processing) the input structure is fetched
and logged.

"""
import math

import unreal

_g_wrapper1 = None
_g_wrapper2 = None


def get_copy_curve_hda_path():
    return '/HoudiniEngine/Examples/hda/copy_to_curve_1_0.copy_to_curve_1_0'


def get_copy_curve_hda():
    return unreal.load_object(None, get_copy_curve_hda_path())


def get_pig_head_hda_path():
    return '/HoudiniEngine/Examples/hda/pig_head_subdivider_v01.pig_head_subdivider_v01'


def get_pig_head_hda():
    return unreal.load_object(None, get_pig_head_hda_path())


def configure_inputs(in_wrapper):
    print('configure_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_instantiation_delegate.remove_callable(configure_inputs)

    # Create a geo input
    asset_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPIAssetInput)
    # Set the input objects/assets for this input
    # asset_input.set_input_objects((_g_wrapper1.get_houdini_asset_actor().houdini_asset_component, ))
    asset_input.set_input_objects((_g_wrapper1, ))
    # copy the input data to the HDA as node input 0
    in_wrapper.set_input_at_index(0, asset_input)
    # We can now discard the API input object
    asset_input = None

    # Create a curve input
    curve_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPICurveInput)
    # Create a curve wrapper/helper
    curve_object = unreal.HoudiniPublicAPICurveInputObject(curve_input)
    # Make it a Nurbs curve
    curve_object.set_curve_type(unreal.HoudiniPublicAPICurveType.NURBS)
    # Set the points of the curve, for this example we create a helix
    # consisting of 100 points
    curve_points = []
    for i in range(10):
        t = i / 10.0 * math.pi * 2.0
        x = 100.0 * math.cos(t)
        y = 100.0 * math.sin(t)
        z = i * 10.0
        curve_points.append(unreal.Transform([x, y, z], [0, 0, 0], [1, 1, 1]))
    curve_object.set_curve_points(curve_points)
    # Set the curve wrapper as an input object
    curve_input.set_input_objects((curve_object, ))
    # Copy the input data to the HDA as node input 1
    in_wrapper.set_input_at_index(1, curve_input)
    # We can now discard the API input object
    curve_input = None


def print_api_input(in_input):
    print('\t\tInput type: {0}'.format(in_input.__class__))
    print('\t\tbKeepWorldTransform: {0}'.format(in_input.keep_world_transform))
    print('\t\tbImportAsReference: {0}'.format(in_input.import_as_reference))
    if isinstance(in_input, unreal.HoudiniPublicAPICurveInput):
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

    global _g_wrapper1, _g_wrapper2
    # instantiate the input HDA with auto-cook enabled
    _g_wrapper1 = api.instantiate_asset(get_pig_head_hda(), unreal.Transform())

    # instantiate the copy curve HDA
    _g_wrapper2 = api.instantiate_asset(get_copy_curve_hda(), unreal.Transform())

    # Configure inputs on_post_instantiation, after instantiation, but before first cook
    _g_wrapper2.on_post_instantiation_delegate.add_callable(configure_inputs)
    # Print the input state after the cook and output creation.
    _g_wrapper2.on_post_processing_delegate.add_callable(print_inputs)


if __name__ == '__main__':
    run()
