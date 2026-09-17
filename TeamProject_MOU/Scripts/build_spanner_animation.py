"""Author the EmoRobot right-hand spanner swing and assign it to Spener."""
import json
from pathlib import Path
import unreal

DEST = '/Game/04_JJO/ItemAnim/Spanner'
lib = unreal.EditorAssetLibrary
source = unreal.load_asset('/Game/04_JJO/ItemAnim/AO_Sequence/OneHand_Front_Sequence')
mesh = unreal.load_asset('/Game/EmoRobot00/Meshes/SKM_EmoRobot00')
skeleton = source.get_editor_property('skeleton')
bones = unreal.AnimationLibrary.get_animation_track_names(source)
lib.make_directory(DEST)
seq = unreal.load_asset(DEST + '/AS_Spanner_Swing_R') if lib.does_asset_exist(DEST + '/AS_Spanner_Swing_R') else lib.duplicate_asset(source.get_path_name(), DEST + '/AS_Spanner_Swing_R')
controller = seq.get_editor_property('controller')
seq.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_NONE)
controller.open_bracket('Author one-handed spanner swing', False)
controller.set_frame_rate(unreal.FrameRate(30, 1), False)
controller.set_number_of_frames(unreal.FrameNumber(24), False)

# Rest, anticipation, release, contact (0.30 s), follow-through, recovery.
keys = [(0, 0, 0), (5, -1, 0), (7, -0.65, 0.1), (9, 0.7, 0.85), (12, 1, 1), (16, 0.55, 0.6), (24, 0, 0)]
def sample(frame):
    for (a, x0, y0), (b, x1, y1) in zip(keys, keys[1:]):
        if frame <= b:
            t = max(0.0, (frame - a) / (b - a))
            t = t*t*(3-2*t)
            return x0+(x1-x0)*t, y0+(y1-y0)*t
    return 0, 0

for bone in bones:
    name = str(bone)
    base = unreal.AnimationLibrary.get_bone_pose_for_time(source, bone, 0.0, False)
    rotations = []
    for frame in range(25):
        swing, follow = sample(frame)
        pitch = yaw = roll = 0.0
        if name == 'arm_r':
            pitch, yaw, roll = 38*swing, -22*swing, -12*follow
        elif name == 'forearm_r':
            pitch, roll = -28*(1-abs(swing)), 12*follow
        elif name == 'hand_r':
            pitch, yaw = 16*swing, -8*follow
        elif name == 'chest':
            yaw, roll = -9*swing, 4*follow
        elif name == 'spine':
            yaw = -4*swing
        elif name.startswith('f_') and name.endswith('_r'):
            # Close the right fingers around the tool throughout the motion.
            pitch = 32 if '2_' in name else 22
            if 'thumb' in name:
                pitch = 18
        delta = unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll).quaternion()
        rotations.append(base.rotation * delta)
    assert controller.set_bone_track_keys(bone, [base.translation]*25, rotations, [base.scale3d]*25, False)
controller.close_bracket(False)
lib.save_loaded_asset(seq)

montage_path = DEST + '/AM_Spanner_Swing_R'
if lib.does_asset_exist(montage_path):
    montage = unreal.load_asset(montage_path)
else:
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', skeleton)
    factory.set_editor_property('source_animation', seq)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AM_Spanner_Swing_R', DEST, unreal.AnimMontage, factory)
assert montage
for prop, duration in [('blend_in', 0.08), ('blend_out', 0.12)]:
    blend = montage.get_editor_property(prop)
    blend.set_editor_property('blend_time', duration)
    montage.set_editor_property(prop, blend)
lib.save_loaded_asset(montage)

weapon = unreal.load_asset('/Game/04_JJO/BluePrint/Weapon/Spener')
cdo = unreal.get_default_object(weapon.generated_class())
cdo.set_editor_property('swing_montage', montage)
cdo.set_editor_property('swing_cooldown', 0.8)
cdo.set_editor_property('swing_hit_delay', 0.3)
cdo.set_editor_property('hand_socket_name', 'SpannerSocket')
unreal.BlueprintEditorLibrary.compile_blueprint(weapon)
weapon_saved = lib.save_loaded_asset(weapon)

report = {'sequence': seq.get_path_name(), 'montage': montage.get_path_name(), 'length': seq.get_editor_property('sequence_length'), 'tracks': len(bones), 'slot': str(montage.get_editor_property('slot_anim_tracks'))}
abp = unreal.load_asset('/Game/05_JYH/MainPlayer/ABP_MainCharacter')
report['graphs'] = {}
for graph in unreal.BlueprintEditorLibrary.list_graphs(abp):
    editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
    report['graphs'][graph.get_name()] = [{'name': n.get_name(), 'title': n.get_node_title(), 'class': n.get_class().get_name()} for n in editor.list_all_nodes()]
report['weapon_saved'] = weapon_saved
report['graph_api'] = {n: getattr(unreal.BlueprintGraphEditor, n).__doc__ for n in dir(unreal.BlueprintGraphEditor) if any(k in n for k in ['node', 'pin', 'connect'])}
report['node_api'] = {n: getattr(unreal.EdGraphNode, n).__doc__ for n in dir(unreal.EdGraphNode) if any(k in n for k in ['pin', 'export'])}
Path(unreal.Paths.project_dir(), 'Saved', 'spanner_build.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
