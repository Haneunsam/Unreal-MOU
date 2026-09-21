import unreal
import json
from pathlib import Path

out = {}
for name, path in {
    'player': '/Game/04_JJO/BP_JJO_EmoPlayer',
    'weapon': '/Game/04_JJO/BluePrint/Weapon/Spener',
    'abp': '/Game/04_JJO/ABP_MainCharacter2',
    'pose': '/Game/04_JJO/ItemAnim/AO_Sequence/OneHand_Front_Sequence',
    'punch': '/Game/EmoRobot00/Animations/A_EmoRobot00_BattlePunchRight1',
    'montage': '/Game/04_JJO/ItemAnim/Potion_Throw_Sequence_Montage',
}.items():
    asset = unreal.load_asset(path)
    row = {'asset': str(asset)}
    if isinstance(asset, unreal.Blueprint):
        cdo = unreal.get_default_object(asset.generated_class())
        row['class'] = str(cdo.get_class())
        for prop in ['mesh', 'mesh_component', 'swing_montage']:
            try:
                value = cdo.get_editor_property(prop)
                row[prop] = str(value)
                if prop == 'mesh':
                    row['skeletal_mesh'] = str(value.get_editor_property('skeletal_mesh_asset'))
                    row['anim_class'] = str(value.get_editor_property('anim_class'))
            except Exception as e:
                row[prop] = str(e)
    for prop in ['skeleton', 'slot_anim_tracks', 'sequence_length']:
        try:
            row[prop] = str(asset.get_editor_property(prop))
        except Exception:
            pass
    out[name] = row
out['controller_api'] = {n: getattr(unreal.AnimDataController, n).__doc__ for n in dir(unreal.AnimDataController) if any(k in n for k in ['bone_track', 'frame', 'initialize', 'model'])}
out['anim_api'] = {n: getattr(unreal.AnimationLibrary, n).__doc__ for n in dir(unreal.AnimationLibrary) if any(k in n for k in ['bone_pose', 'bone_names'])}
pose = unreal.load_asset('/Game/04_JJO/ItemAnim/AO_Sequence/OneHand_Front_Sequence')
out['sequence_methods'] = [n for n in dir(pose) if any(k in n for k in ['controller', 'model', 'bone'])]
out['model_methods'] = [n for n in dir(unreal.AnimDataModel) if 'bone' in n]
out['bones'] = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(pose)]
out['structs'] = {n: getattr(unreal, n).__doc__ for n in ['AnimSegment', 'AnimTrack', 'SlotAnimationTrack', 'AnimSequenceFactory', 'AnimMontageFactory']}
out['object_helpers'] = [n for n in dir(unreal) if any(k in n for k in ['objects_with', 'BlueprintEditor', 'AnimGraphNode'])]
out['poses'] = {b: str(unreal.AnimationLibrary.get_bone_pose_for_time(pose, b, 0.0, False)) for b in out['bones']}
Path(unreal.Paths.project_dir(), 'Saved', 'spanner_inspection.json').write_text(json.dumps(out, indent=2), encoding='utf-8')
