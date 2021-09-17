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

""" Example script that instantiates an HDA using the API and then
setting some parameter values after instantiation but before the 
first cook.

"""

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/pig_head_subdivider_v01.pig_head_subdivider_v01'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def on_post_instantiation(in_wrapper):
    print('on_post_instantiation')
    # in_wrapper.on_post_instantiation_delegate.remove_callable(on_post_instantiation)

    # Set some parameters to create instances and enable a material
    in_wrapper.set_bool_parameter_value('add_instances', True)
    in_wrapper.set_int_parameter_value('num_instances', 8)
    in_wrapper.set_bool_parameter_value('addshader', True)


def run():
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Bind on_post_instantiation (before the first cook) callback to set parameters
    _g_wrapper.on_post_instantiation_delegate.add_callable(on_post_instantiation)


if __name__ == '__main__':
    run()
