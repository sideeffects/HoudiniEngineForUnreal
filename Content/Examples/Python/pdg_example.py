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

""" An example script that instantiates an HDA that contains a TOP network.
After the HDA itself has cooked (in on_post_process) we iterate over all
TOP networks in the HDA and print their paths. Auto-bake is then enabled for
PDG and the TOP networks are cooked.

"""

import os

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/pdg_pighead_grid_2_0.pdg_pighead_grid_2_0'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def get_hda_directory():
    """ Attempt to get the directory containing the .hda files. This is
    /HoudiniEngine/Examples/hda. In newer versions of UE we can use
    ``unreal.SystemLibrary.get_system_path(asset)``, in older versions
    we manually construct the path to the plugin.

    Returns:
        (str): the path to the example hda directory or ``None``.

    """
    if hasattr(unreal.SystemLibrary, 'get_system_path'):
        return os.path.dirname(os.path.normpath(
            unreal.SystemLibrary.get_system_path(get_test_hda())))
    else:
        plugin_dir = os.path.join(
            os.path.normpath(unreal.Paths.project_plugins_dir()),
            'Runtime', 'HoudiniEngine', 'Content', 'Examples', 'hda'
        )
        if not os.path.exists(plugin_dir):
            plugin_dir = os.path.join(
                os.path.normpath(unreal.Paths.engine_plugins_dir()),
                'Runtime', 'HoudiniEngine', 'Content', 'Examples', 'hda'
        )
        if not os.path.exists(plugin_dir):
            return None
        return plugin_dir


def delete_instantiated_asset():
    global _g_wrapper
    if _g_wrapper:
        result = _g_wrapper.delete_instantiated_asset()
        _g_wrapper = None
        return result
    else:
        return False


def on_pre_instantiation(in_wrapper):
    print('on_pre_instantiation')

    # Set the hda_directory parameter to the directory that contains the
    # example .hda files
    hda_directory = get_hda_directory()
    print('Setting "hda_directory" to {0}'.format(hda_directory))
    in_wrapper.set_string_parameter_value('hda_directory', hda_directory)

    # Cook the HDA (not PDG yet)
    in_wrapper.recook()


def on_post_bake(in_wrapper, success):
    # in_wrapper.on_post_bake_delegate.remove_callable(on_post_bake)
    print('bake complete ... {}'.format('success' if success else 'failed'))


def on_post_process(in_wrapper):
    print('on_post_process')

    # in_wrapper.on_post_processing_delegate.remove_callable(on_post_process)

    # Iterate over all PDG/TOP networks and nodes and log them
    print('TOP networks:')
    for network_path in in_wrapper.get_pdgtop_network_paths():
        print('\t{}'.format(network_path))
        for node_path in in_wrapper.get_pdgtop_node_paths(network_path):
            print('\t\t{}'.format(node_path))

    # Enable PDG auto-bake (auto bake TOP nodes after they are cooked)
    in_wrapper.set_pdg_auto_bake_enabled(True)
    # Bind to PDG post bake delegate (called after all baking is complete)
    in_wrapper.on_post_pdg_bake_delegate.add_callable(on_post_bake)
    # Cook the specified TOP node
    in_wrapper.pdg_cook_node('topnet1', 'HE_OUT_PIGHEAD_GRID')


def run():
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper

    # instantiate an asset, disabling auto-cook of the asset
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform(), enable_auto_cook=False)

    # Bind to the on pre instantiation delegate (before the first cook) and
    # set parameters
    _g_wrapper.on_pre_instantiation_delegate.add_callable(on_pre_instantiation)
    # Bind to the on post processing delegate (after a cook and after all
    # outputs have been generated in Unreal)
    _g_wrapper.on_post_processing_delegate.add_callable(on_post_process)


if __name__ == '__main__':
    run()
