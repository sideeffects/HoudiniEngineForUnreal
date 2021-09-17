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

""" An example script that uses the API to instantiate an HDA and then set
the ramp points of a float ramp and a color ramp.

"""
import math

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/ramp_example_1_0.ramp_example_1_0'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def set_parameters(in_wrapper):
    print('set_parameters')

    # Unbind from the delegate
    in_wrapper.on_post_instantiation_delegate.remove_callable(set_parameters)

    # There are two ramps: heightramp and colorramp. The height ramp is a float
    # ramp. As an example we'll set the number of ramp points and then set
    # each point individually
    in_wrapper.set_ramp_parameter_num_points('heightramp', 6)
    in_wrapper.set_float_ramp_parameter_point_value('heightramp', 0, 0.0, 0.1)
    in_wrapper.set_float_ramp_parameter_point_value('heightramp', 1, 0.2, 0.6)
    in_wrapper.set_float_ramp_parameter_point_value('heightramp', 2, 0.4, 1.0)
    in_wrapper.set_float_ramp_parameter_point_value('heightramp', 3, 0.6, 1.4)
    in_wrapper.set_float_ramp_parameter_point_value('heightramp', 4, 0.8, 1.8)
    in_wrapper.set_float_ramp_parameter_point_value('heightramp', 5, 1.0, 2.2)

    # For the color ramp, as an example, we can set the all the points via an
    # array.
    in_wrapper.set_color_ramp_parameter_points('colorramp', (
        unreal.HoudiniPublicAPIColorRampPoint(position=0.0, value=unreal.LinearColor.GRAY),
        unreal.HoudiniPublicAPIColorRampPoint(position=0.5, value=unreal.LinearColor.GREEN),
        unreal.HoudiniPublicAPIColorRampPoint(position=1.0, value=unreal.LinearColor.RED),
    ))


def print_parameters(in_wrapper):
    print('print_parameters')

    in_wrapper.on_post_processing_delegate.remove_callable(print_parameters)

    # Print the ramp points directly
    print('heightramp: num points {0}:'.format(in_wrapper.get_ramp_parameter_num_points('heightramp')))
    heightramp_data = in_wrapper.get_float_ramp_parameter_points('heightramp')
    if not heightramp_data:
        print('\tNone')
    else:
        for idx, point_data in enumerate(heightramp_data):
            print('\t\t{0}: position={1:.6f}; value={2:.6f}; interpoloation={3}'.format(
                idx,
                point_data.position,
                point_data.value,
                point_data.interpolation
            ))

    print('colorramp: num points {0}:'.format(in_wrapper.get_ramp_parameter_num_points('colorramp')))
    colorramp_data = in_wrapper.get_color_ramp_parameter_points('colorramp')
    if not colorramp_data:
        print('\tNone')
    else:
        for idx, point_data in enumerate(colorramp_data):
            print('\t\t{0}: position={1:.6f}; value={2}; interpoloation={3}'.format(
                idx,
                point_data.position,
                point_data.value,
                point_data.interpolation
            ))

    # Print all parameter values
    param_tuples = in_wrapper.get_parameter_tuples()
    print('parameter tuples: {}'.format(len(param_tuples) if param_tuples else 0))
    if param_tuples:
        for param_tuple_name, param_tuple in param_tuples.items():
            print('parameter tuple name: {}'.format(param_tuple_name))
            print('\tbool_values: {}'.format(param_tuple.bool_values))
            print('\tfloat_values: {}'.format(param_tuple.float_values))
            print('\tint32_values: {}'.format(param_tuple.int32_values))
            print('\tstring_values: {}'.format(param_tuple.string_values))
            if not param_tuple.float_ramp_points:
                print('\tfloat_ramp_points: None')
            else:
                print('\tfloat_ramp_points:')
                for idx, point_data in enumerate(param_tuple.float_ramp_points):
                    print('\t\t{0}: position={1:.6f}; value={2:.6f}; interpoloation={3}'.format(
                        idx,
                        point_data.position,
                        point_data.value,
                        point_data.interpolation
                    ))
            if not param_tuple.color_ramp_points:
                print('\tcolor_ramp_points: None')
            else:
                print('\tcolor_ramp_points:')
                for idx, point_data in enumerate(param_tuple.color_ramp_points):
                    print('\t\t{0}: position={1:.6f}; value={2}; interpoloation={3}'.format(
                        idx,
                        point_data.position,
                        point_data.value,
                        point_data.interpolation
                    ))


def run():
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Set the float and color ramps on post instantiation, before the first
    # cook.
    _g_wrapper.on_post_instantiation_delegate.add_callable(set_parameters)
    # Print the parameter state after the cook and output creation.
    _g_wrapper.on_post_processing_delegate.add_callable(print_parameters)


if __name__ == '__main__':
    run()
