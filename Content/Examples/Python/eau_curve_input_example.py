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

import math

import unreal

@unreal.uclass()
class CurveInputExample(unreal.PlacedEditorUtilityBase):
    # Use a FProperty to hold the reference to the API wrapper we create in
    # run_curve_input_example
    _asset_wrapper = unreal.uproperty(unreal.HoudiniPublicAPIAssetWrapper)

    @unreal.ufunction(meta=dict(BlueprintCallable=True, CallInEditor=True))
    def run_curve_input_example(self):
        # Get the API instance
        api = unreal.HoudiniPublicAPIBlueprintLib.get_api()
        # Ensure we have a running session
        if not api.is_session_valid():
            api.create_session()
        # Load our HDA uasset
        example_hda = unreal.load_object(None, '/HoudiniEngine/Examples/hda/copy_to_curve_1_0.copy_to_curve_1_0')
        # Create an API wrapper instance for instantiating the HDA and interacting with it
        wrapper = api.instantiate_asset(example_hda, instantiate_at=unreal.Transform())
        if wrapper:
            # Pre-instantiation is the earliest point where we can set parameter values
            wrapper.on_pre_instantiation_delegate.add_function(self, '_set_initial_parameter_values')
            # Jumping ahead a bit: we also want to configure inputs, but inputs are only available after instantiation
            wrapper.on_post_instantiation_delegate.add_function(self, '_set_inputs')
            # Jumping ahead a bit: we also want to print the outputs after the node has cook and the plug-in has processed the output
            wrapper.on_post_processing_delegate.add_function(self, '_print_outputs')
        self.set_editor_property('_asset_wrapper', wrapper)

    @unreal.ufunction(params=[unreal.HoudiniPublicAPIAssetWrapper], meta=dict(CallInEditor=True))
    def _set_initial_parameter_values(self, in_wrapper):
        """ Set our initial parameter values: disable upvectorstart and set the scale to 0.2. """
        # Uncheck the upvectoratstart parameter
        in_wrapper.set_bool_parameter_value('upvectoratstart', False)

        # Set the scale to 0.2
        in_wrapper.set_float_parameter_value('scale', 0.2)

        # Since we are done with setting the initial values, we can unbind from the delegate
        in_wrapper.on_pre_instantiation_delegate.remove_function(self, '_set_initial_parameter_values')

    @unreal.ufunction(params=[unreal.HoudiniPublicAPIAssetWrapper], meta=dict(CallInEditor=True))
    def _set_inputs(self, in_wrapper):
        """ Configure our inputs: input 0 is a cube and input 1 a helix. """
        # Create an empty geometry input
        geo_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPIGeoInput)
        # Load the cube static mesh asset
        cube = unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube')
        # Set the input object array for our geometry input, in this case containing only the cube
        geo_input.set_input_objects((cube, ))

        # Set the input on the instantiated HDA via the wrapper
        in_wrapper.set_input_at_index(0, geo_input)

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
        # Set the curve wrapper as an input object
        curve_input.set_input_objects((curve_object, ))
        # Copy the input data to the HDA as node input 1
        in_wrapper.set_input_at_index(1, curve_input)

        # unbind from the delegate, since we are done with setting inputs
        in_wrapper.on_post_instantiation_delegate.remove_function(self, '_set_inputs')

    @unreal.ufunction(params=[unreal.HoudiniPublicAPIAssetWrapper], meta=dict(CallInEditor=True))
    def _print_outputs(self, in_wrapper):
        """  Print the outputs that were generated by the HDA (after a cook) """
        num_outputs = in_wrapper.get_num_outputs()
        print('num_outputs: {}'.format(num_outputs))
        if num_outputs > 0:
            for output_idx in range(num_outputs):
                identifiers = in_wrapper.get_output_identifiers_at(output_idx)
                print('\toutput index: {}'.format(output_idx))
                print('\toutput type: {}'.format(in_wrapper.get_output_type_at(output_idx)))
                print('\tnum_output_objects: {}'.format(len(identifiers)))
                if identifiers:
                    for identifier in identifiers:
                        output_object = in_wrapper.get_output_object_at(output_idx, identifier)
                        output_component = in_wrapper.get_output_component_at(output_idx, identifier)
                        is_proxy = in_wrapper.is_output_current_proxy_at(output_idx, identifier)
                        print('\t\tidentifier: {}'.format(identifier))
                        print('\t\toutput_object: {}'.format(output_object.get_name() if output_object else 'None'))
                        print('\t\toutput_component: {}'.format(output_component.get_name() if output_component else 'None'))
                        print('\t\tis_proxy: {}'.format(is_proxy))
                        print('')


def run():
    # Spawn CurveInputExample and call run_curve_input_example
    curve_input_example_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(CurveInputExample.static_class(), unreal.Vector.ZERO, unreal.Rotator())
    curve_input_example_actor.run_curve_input_example()


if __name__ == '__main__':
    run()
