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


class ProcessHDA(object):
    """ An object that wraps async processing of an HDA (instantiating,
    cooking/processing/baking an HDA), with functions that are called at the
    various stages of the process, that can be overridden by subclasses for
    custom funtionality:

        - on_failure()
        - on_complete(): upon successful completion (could be PostInstantiation
          if auto cook is disabled, PostProcessing if auto bake is disabled, or
          after PostAutoBake if auto bake is enabled.
        - on_pre_instantiation(): before the HDA is instantiated, a good place
          to set parameter values before the first cook.
        - on_post_instantiation(): after the HDA is instantiated, a good place
          to set/configure inputs before the first cook.
        - on_post_auto_cook(): right after a cook
        - on_pre_process(): after a cook but before output objects have been
          created/processed
        - on_post_processing(): after output objects have been created
        - on_post_auto_bake(): after outputs have been baked

    Instantiate the processor via the constructor and then call the activate()
    function to start the asynchronous process.

    """
    def __init__(
            self,
            houdini_asset,
            instantiate_at=unreal.Transform(),
            parameters=None,
            node_inputs=None,
            parameter_inputs=None,
            world_context_object=None,
            spawn_in_level_override=None,
            enable_auto_cook=True,
            enable_auto_bake=False,
            bake_directory_path="",
            bake_method=unreal.HoudiniEngineBakeOption.TO_ACTOR,
            remove_output_after_bake=False,
            recenter_baked_actors=False,
            replace_previous_bake=False,
            delete_instantiated_asset_on_completion_or_failure=False):
        """ Instantiates an HDA in the specified world/level. Sets parameters
        and inputs supplied in InParameters, InNodeInputs and parameter_inputs.
        If bInEnableAutoCook is true, cooks the HDA. If bInEnableAutoBake is
        true, bakes the cooked outputs according to the supplied baking
        parameters.

        This all happens asynchronously, with the various output pins firing at
        the various points in the process:

            - PreInstantiation: before the HDA is instantiated, a good place
              to set parameter values before the first cook (parameter values
              from ``parameters`` are automatically applied at this point)
            - PostInstantiation: after the HDA is instantiated, a good place
              to set/configure inputs before the first cook (inputs from
              ``node_inputs`` and ``parameter_inputs`` are automatically applied
              at this point)
            - PostAutoCook: right after a cook
            - PreProcess: after a cook but before output objects have been
              created/processed
            - PostProcessing: after output objects have been created
            - PostAutoBake: after outputs have been baked
            - Completed: upon successful completion (could be PostInstantiation
              if auto cook is disabled, PostProcessing if auto bake is disabled,
              or after PostAutoBake if auto bake is enabled).
            - Failed: If the process failed at any point.

        Args:
            houdini_asset (HoudiniAsset): The HDA to instantiate.
            instantiate_at (Transform):  The Transform to instantiate the HDA with.
            parameters (Map(Name, HoudiniParameterTuple)): The parameters to set before cooking the instantiated HDA.
            node_inputs (Map(int32, HoudiniPublicAPIInput)): The node inputs to set before cooking the instantiated HDA.
            parameter_inputs (Map(Name, HoudiniPublicAPIInput)): The parameter-based inputs to set before cooking the instantiated HDA.
            world_context_object (Object): A world context object for identifying the world to spawn in, if spawn_in_level_override is null.
            spawn_in_level_override (Level): If not nullptr, then the HoudiniAssetActor is spawned in that level. If both spawn_in_level_override and world_context_object are null, then the actor is spawned in the current editor context world's current level.
            enable_auto_cook (bool): If true (the default) the HDA will cook automatically after instantiation and after parameter, transform and input changes.
            enable_auto_bake (bool): If true, the HDA output is automatically baked after a cook. Defaults to false.
            bake_directory_path (str): The directory to bake to if the bake path is not set via attributes on the HDA output.
            bake_method (HoudiniEngineBakeOption): The bake target (to actor vs blueprint). @see HoudiniEngineBakeOption.
            remove_output_after_bake (bool): If true, HDA temporary outputs are removed after a bake. Defaults to false.
            recenter_baked_actors (bool): Recenter the baked actors to their bounding box center. Defaults to false.
            replace_previous_bake (bool): If true, on every bake replace the previous bake's output (assets + actors) with the new bake's output. Defaults to false.
            delete_instantiated_asset_on_completion_or_failure (bool): If true, deletes the instantiated asset actor on completion or failure. Defaults to false.

        """
        super(ProcessHDA, self).__init__()
        self._houdini_asset = houdini_asset
        self._instantiate_at = instantiate_at
        self._parameters = parameters
        self._node_inputs = node_inputs
        self._parameter_inputs = parameter_inputs
        self._world_context_object = world_context_object
        self._spawn_in_level_override = spawn_in_level_override
        self._enable_auto_cook = enable_auto_cook
        self._enable_auto_bake = enable_auto_bake
        self._bake_directory_path = bake_directory_path
        self._bake_method = bake_method
        self._remove_output_after_bake = remove_output_after_bake
        self._recenter_baked_actors = recenter_baked_actors
        self._replace_previous_bake = replace_previous_bake
        self._delete_instantiated_asset_on_completion_or_failure = delete_instantiated_asset_on_completion_or_failure

        self._asset_wrapper = None
        self._cook_success = False
        self._bake_success = False

    @property
    def asset_wrapper(self):
        """ The asset wrapper for the instantiated HDA processed by this node. """
        return self._asset_wrapper

    @property
    def cook_success(self):
        """ True if the last cook was successful. """
        return self._cook_success

    @property
    def bake_success(self):
        """ True if the last bake was successful. """
        return self._bake_success

    @property
    def houdini_asset(self):
        """ The HDA to instantiate. """
        return self._houdini_asset

    @property
    def instantiate_at(self):
        """ The transform the instantiate the asset with. """
        return self._instantiate_at

    @property
    def parameters(self):
        """ The parameters to set on on_pre_instantiation """
        return self._parameters

    @property
    def node_inputs(self):
        """ The node inputs to set on on_post_instantiation """
        return self._node_inputs

    @property
    def parameter_inputs(self):
        """ The object path parameter inputs to set on on_post_instantiation """
        return self._parameter_inputs

    @property
    def world_context_object(self):
        """ The world context object: spawn in this world if spawn_in_level_override is not set. """
        return self._world_context_object

    @property
    def spawn_in_level_override(self):
        """ The level to spawn in. If both this and world_context_object is not set, spawn in the editor context's level. """
        return self._spawn_in_level_override

    @property
    def enable_auto_cook(self):
        """ Whether to set the instantiated asset to auto cook. """
        return self._enable_auto_cook

    @property
    def enable_auto_bake(self):
        """ Whether to set the instantiated asset to auto bake after a cook. """
        return self._enable_auto_bake

    @property
    def bake_directory_path(self):
        """ Set the fallback bake directory, for if output attributes do not specify it. """
        return self._bake_directory_path

    @property
    def bake_method(self):
        """ The bake method/target: for example, to actors vs to blueprints. """
        return self._bake_method

    @property
    def remove_output_after_bake(self):
        """ Remove temporary HDA output after a bake. """
        return self._remove_output_after_bake

    @property
    def recenter_baked_actors(self):
        """ Recenter the baked actors at their bounding box center. """
        return self._recenter_baked_actors

    @property
    def replace_previous_bake(self):
        """ Replace previous bake output on each bake. For the purposes of this
        node, this would mostly apply to .uassets and not actors.

        """
        return self._replace_previous_bake

    @property
    def delete_instantiated_asset_on_completion_or_failure(self):
        """ Whether or not to delete the instantiated asset after Complete is called. """
        return self._delete_instantiated_asset_on_completion_or_failure

    def activate(self):
        """ Activate the process. This will:

                - instantiate houdini_asset and wrap it as asset_wrapper
                - call on_failure() for any immediate failures
                - otherwise bind to delegates from asset_wrapper so that the
                various self.on_*() functions are called as appropriate

            Returns immediately (does not block until cooking/processing is
            complete).

        Returns:
            (bool): False if activation failed.

        """
        # Get the API instance
        houdini_api = unreal.HoudiniPublicAPIBlueprintLib.get_api()
        if not houdini_api:
            # Handle failures: this will unbind delegates and call on_failure()
            self._handle_on_failure()
            return False

        # Create an empty API asset wrapper
        self._asset_wrapper = unreal.HoudiniPublicAPIAssetWrapper.create_empty_wrapper(houdini_api)
        if not self._asset_wrapper:
            # Handle failures: this will unbind delegates and call on_failure()
            self._handle_on_failure()
            return False

        # Bind to the wrapper's delegates for instantiation, cooking, baking
        # etc events
        self._asset_wrapper.on_pre_instantiation_delegate.add_callable(
            self._handle_on_pre_instantiation)
        self._asset_wrapper.on_post_instantiation_delegate.add_callable(
            self._handle_on_post_instantiation)
        self._asset_wrapper.on_post_cook_delegate.add_callable(
            self._handle_on_post_auto_cook)
        self._asset_wrapper.on_pre_process_state_exited_delegate.add_callable(
            self._handle_on_pre_process)
        self._asset_wrapper.on_post_processing_delegate.add_callable(
            self._handle_on_post_processing)
        self._asset_wrapper.on_post_bake_delegate.add_callable(
            self._handle_on_post_auto_bake)

        # Begin the instantiation process of houdini_asset and wrap it with
        # self.asset_wrapper
        if not houdini_api.instantiate_asset_with_existing_wrapper(
                self.asset_wrapper,
                self.houdini_asset,
                self.instantiate_at,
                self.world_context_object,
                self.spawn_in_level_override,
                self.enable_auto_cook,
                self.enable_auto_bake,
                self.bake_directory_path,
                self.bake_method,
                self.remove_output_after_bake,
                self.recenter_baked_actors,
                self.replace_previous_bake):
            # Handle failures: this will unbind delegates and call on_failure()
            self._handle_on_failure()
            return False

        return True

    def _unbind_delegates(self):
        """ Unbinds from self.asset_wrapper's delegates (if valid). """
        if not self._asset_wrapper:
            return

        self._asset_wrapper.on_pre_instantiation_delegate.add_callable(
            self._handle_on_pre_instantiation)
        self._asset_wrapper.on_post_instantiation_delegate.add_callable(
            self._handle_on_post_instantiation)
        self._asset_wrapper.on_post_cook_delegate.add_callable(
            self._handle_on_post_auto_cook)
        self._asset_wrapper.on_pre_process_state_exited_delegate.add_callable(
            self._handle_on_pre_process)
        self._asset_wrapper.on_post_processing_delegate.add_callable(
            self._handle_on_post_processing)
        self._asset_wrapper.on_post_bake_delegate.add_callable(
            self._handle_on_post_auto_bake)

    def _check_wrapper(self, wrapper):
        """ Checks that wrapper matches self.asset_wrapper. Logs a warning if
        it does not.

        Args:
            wrapper (HoudiniPublicAPIAssetWrapper): the wrapper to check
            against self.asset_wrapper

        Returns:
            (bool): True if the wrappers match.

        """
        if wrapper != self._asset_wrapper:
            unreal.log_warning(
                '[UHoudiniPublicAPIProcessHDANode] Received delegate event '
                'from unexpected asset wrapper ({0} vs {1})!'.format(
                    self._asset_wrapper.get_name() if self._asset_wrapper else '',
                    wrapper.get_name() if wrapper else ''
                )
            )
            return False
        return True

    def _handle_on_failure(self):
        """ Handle any failures during the lifecycle of the process. Calls
        self.on_failure() and then unbinds from self.asset_wrapper and
        optionally deletes the instantiated asset.

        """
        self.on_failure()

        self._unbind_delegates()

        if self.delete_instantiated_asset_on_completion_or_failure and self.asset_wrapper:
            self.asset_wrapper.delete_instantiated_asset()

    def _handle_on_complete(self):
        """ Handles completion of the process. This can happen at one of 
        three stages:

            - After on_post_instantiate(), if enable_auto_cook is False.
            - After on_post_auto_cook(), if enable_auto_cook is True but
              enable_auto_bake is False.
            - After on_post_auto_bake(), if both enable_auto_cook and
              enable_auto_bake are True.

        Calls self.on_complete() and then unbinds from self.asset_wrapper's
        delegates and optionally deletes the instantiated asset.

        """
        self.on_complete()

        self._unbind_delegates()

        if self.delete_instantiated_asset_on_completion_or_failure and self.asset_wrapper:
            self.asset_wrapper.delete_instantiated_asset()

    def _handle_on_pre_instantiation(self, wrapper):
        """ Called during pre_instantiation. Sets ``parameters`` on the HDA
        and calls self.on_pre_instantiation().

        """
        if not self._check_wrapper(wrapper):
            return

        # Set any parameters specified for the HDA
        if self.asset_wrapper and self.parameters:
            self.asset_wrapper.set_parameter_tuples(self.parameters)

        self.on_pre_instantiation()

    def _handle_on_post_instantiation(self, wrapper):
        """ Called during post_instantiation. Sets inputs (``node_inputs`` and
        ``parameter_inputs``) on the HDA and calls self.on_post_instantiation().

        Completes execution if enable_auto_cook is False.

        """
        if not self._check_wrapper(wrapper):
            return

        # Set any inputs specified when the node was created
        if self.asset_wrapper:
            if self.node_inputs:
                self.asset_wrapper.set_inputs_at_indices(self.node_inputs)
            if self.parameter_inputs:
                self.asset_wrapper.set_input_parameters(self.parameter_inputs)

        self.on_post_instantiation()

        # If not set to auto cook, complete execution now
        if not self.enable_auto_cook:
            self._handle_on_complete()

    def _handle_on_post_auto_cook(self, wrapper, cook_success):
        """ Called during post_cook. Sets self.cook_success and calls
        self.on_post_auto_cook().

        Args:
            cook_success (bool): True if the cook was successful.

        """
        if not self._check_wrapper(wrapper):
            return

        self._cook_success = cook_success

        self.on_post_auto_cook(cook_success)

    def _handle_on_pre_process(self, wrapper):
        """ Called during pre_process. Calls self.on_pre_process().

        """
        if not self._check_wrapper(wrapper):
            return

        self.on_pre_process()

    def _handle_on_post_processing(self, wrapper):
        """ Called during post_processing. Calls self.on_post_processing().

        Completes execution if enable_auto_bake is False.

        """
        if not self._check_wrapper(wrapper):
            return

        self.on_post_processing()

        # If not set to auto bake, complete execution now
        if not self.enable_auto_bake:
            self._handle_on_complete()

    def _handle_on_post_auto_bake(self, wrapper, bake_success):
        """ Called during post_bake. Sets self.bake_success and calls
        self.on_post_auto_bake().

        Args:
            bake_success (bool): True if the bake was successful.

        """
        if not self._check_wrapper(wrapper):
            return

        self._bake_success = bake_success

        self.on_post_auto_bake(bake_success)

        self._handle_on_complete()

    def on_failure(self):
        """ Called if the process fails to instantiate or fails to start
        a cook.

        Subclasses can override this function implement custom functionality.

        """
        pass

    def on_complete(self):
        """ Called if the process completes instantiation, cook and/or baking,
        depending on enable_auto_cook and enable_auto_bake.

        Subclasses can override this function implement custom functionality.

        """
        pass

    def on_pre_instantiation(self):
        """ Called during pre_instantiation.

        Subclasses can override this function implement custom functionality.

        """
        pass

    def on_post_instantiation(self):
        """ Called during post_instantiation.

        Subclasses can override this function implement custom functionality.

        """
        pass

    def on_post_auto_cook(self, cook_success):
        """ Called during post_cook.

        Subclasses can override this function implement custom functionality.

        Args:
            cook_success (bool): True if the cook was successful.

        """
        pass

    def on_pre_process(self):
        """ Called during pre_process.

        Subclasses can override this function implement custom functionality.

        """
        pass

    def on_post_processing(self):
        """ Called during post_processing.

        Subclasses can override this function implement custom functionality.

        """
        pass

    def on_post_auto_bake(self, bake_success):
        """ Called during post_bake.

        Subclasses can override this function implement custom functionality.

        Args:
            bake_success (bool): True if the bake was successful.

        """
        pass
