""" Example script that uses unreal.ActorIterator to iterate over all
HoudiniAssetActors in the editor world, creates API wrappers for them, and
logs all of their parameter tuples.

"""

import unreal


def run():
    # Get the API instance
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    # Get the editor world    
    editor_subsystem = None
    world = None
    try:
        # In UE5 unreal.EditorLevelLibrary.get_editor_world is deprecated and
        # unreal.UnrealEditorSubsystem.get_editor_world is the replacement,
        # but unreal.UnrealEditorSubsystem does not exist in UE4
        editor_subsystem = unreal.UnrealEditorSubsystem()
    except AttributeError:
        world = unreal.EditorLevelLibrary.get_editor_world()
    else:
        world = editor_subsystem.get_editor_world()

    # Iterate over all Houdini Asset Actors in the editor world
    for houdini_actor in unreal.ActorIterator(world, unreal.HoudiniAssetActor):
        if not houdini_actor:
            continue

        # Print the name and label of the actor
        actor_name = houdini_actor.get_name()
        actor_label = houdini_actor.get_actor_label()
        print(f'HDA Actor (Name, Label): {actor_name}, {actor_label}')

        # Wrap the Houdini asset actor with the API
        wrapper = unreal.HoudiniPublicAPIAssetWrapper.create_wrapper(world, houdini_actor)
        if not wrapper:
            continue
        
        # Get all parameter tuples of the HDA
        parameter_tuples = wrapper.get_parameter_tuples()
        if parameter_tuples is None:
            # The operation failed, log the error message
            error_message = wrapper.get_last_error_message()
            if error_message is not None:
                print(error_message)
            
            continue

        print(f'# Parameter Tuples: {len(parameter_tuples)}')
        for name, data in parameter_tuples.items():
            print(f'\tParameter Tuple Name: {name}')

            type_name = None
            values = None
            if data.bool_values:
                type_name = 'Bool'
                values = '; '.join(('1' if v else '0' for v in data.bool_values))
            elif data.float_values:
                type_name = 'Float'
                values = '; '.join((f'{v:.4f}' for v in data.float_values))
            elif data.int32_values:
                type_name = 'Int32'
                values = '; '.join((f'{v:d}' for v in data.int32_values))
            elif data.string_values:
                type_name = 'String'
                values = '; '.join(data.string_values)

            if not type_name:
                print('\t\tEmpty')
            else:
                print(f'\t\t{type_name} Values: {values}')
    

if __name__ == '__main__':
    run()
