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

import unreal

""" Example script for instantiating an asset, cooking it and baking an
individual output object.

"""

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/pig_head_subdivider_v01.pig_head_subdivider_v01'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def on_post_process(in_wrapper):
    print('on_post_process')
    # Print details about the outputs and record the first static mesh we find

    sm_index = None
    sm_identifier = None

    # in_wrapper.on_post_processing_delegate.remove_callable(on_post_process)
    
    num_outputs = in_wrapper.get_num_outputs()
    print('num_outputs: {}'.format(num_outputs))
    if num_outputs > 0:
        for output_idx in range(num_outputs):
            identifiers = in_wrapper.get_output_identifiers_at(output_idx)
            output_type = in_wrapper.get_output_type_at(output_idx)
            print('\toutput index: {}'.format(output_idx))
            print('\toutput type: {}'.format(output_type))
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

                    if (output_type == unreal.HoudiniOutputType.MESH and 
                            isinstance(output_object, unreal.StaticMesh)):
                        sm_index = output_idx
                        sm_identifier = identifier
    
    # Bake the first static mesh we found to the CB
    if sm_index is not None and sm_identifier is not None:
        print('baking {}'.format(sm_identifier))
        success = in_wrapper.bake_output_object_at(sm_index, sm_identifier)
        print('success' if success else 'failed')

    # Delete the instantiated asset
    in_wrapper.delete_instantiated_asset()
    global _g_wrapper
    _g_wrapper = None


def run():
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Bind to the on post processing delegate (after a cook and after all 
    # outputs have been generated in Unreal)
    _g_wrapper.on_post_processing_delegate.add_callable(on_post_process)


if __name__ == '__main__':
    run()
