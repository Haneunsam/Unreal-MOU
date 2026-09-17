"""Reload saved assets and validate the spanner integration without modifying them."""
import json
from pathlib import Path
import unreal

base = '/Game/04_JJO/ItemAnim/Spanner/'
seq = unreal.load_asset(base + 'AS_Spanner_Swing_R')
hold = unreal.load_asset(base + 'AS_Spanner_Hold_R')
montage = unreal.load_asset(base + 'AM_Spanner_Swing_R')
abp = unreal.load_asset(base + 'ABP_JJO_SpannerPlayer')
mesh = unreal.load_asset('/Game/EmoRobot00/Meshes/SKM_EmoRobot00')
skeleton = mesh.get_editor_property('skeleton')
socket = mesh.find_socket('SpannerSocket')
assert socket.get_outer() == skeleton
assert str(socket.get_editor_property('bone_name')) == 'hand_r'
assert socket.get_editor_property('relative_location') == unreal.Vector(0, 0, 0)
assert socket.get_editor_property('relative_rotation') == unreal.Rotator(0, 0, 0)
for asset in [seq, hold, montage]:
    assert asset.get_editor_property('skeleton') == skeleton
assert seq.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE
assert hold.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE
assert abs(seq.get_editor_property('sequence_length') - 0.8) < 0.001
tracks = montage.get_editor_property('slot_anim_tracks')
assert str(tracks[0].get_editor_property('slot_name')) == 'DefaultSlot'
assert tracks[0].get_editor_property('anim_track').get_editor_property('anim_segments')[0].get_editor_property('anim_reference') == seq
weapon = unreal.get_default_object(unreal.load_asset('/Game/04_JJO/BluePrint/Weapon/Spener').generated_class())
assert weapon.get_editor_property('swing_montage') == montage
assert str(weapon.get_editor_property('hand_socket_name')) == 'SpannerSocket'
assert abs(weapon.get_editor_property('swing_hit_delay') - 0.3) < 0.001
player = unreal.get_default_object(unreal.load_asset('/Game/04_JJO/BP_JJO_EmoPlayer').generated_class())
assert player.get_editor_property('mesh').get_editor_property('anim_class') == abp.generated_class()
graph = next(g for g in unreal.BlueprintEditorLibrary.list_graphs(abp) if g.get_name() == 'AnimGraph')
editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
assert not editor.list_nodes_with_errors()
nodes = editor.list_all_nodes()
getters = [n for n in nodes if 'HoldingHealingMelee' in n.get_node_title()]
assert len(getters) == 1
assert getters[0].list_output_pins()[0].list_connected_pins()
bones = unreal.AnimationLibrary.get_animation_track_names(seq)
def quat_dot(a, b):
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w
for bone in bones:
    first = unreal.AnimationLibrary.get_bone_pose_for_time(seq, bone, 0.0, False)
    last = unreal.AnimationLibrary.get_bone_pose_for_time(seq, bone, 0.8, False)
    assert abs(quat_dot(first.rotation, last.rotation)) > 0.9999, str(bone)
first = unreal.AnimationLibrary.get_bone_pose_for_time(seq, 'arm_r', 0.0, False)
contact = unreal.AnimationLibrary.get_bone_pose_for_time(seq, 'arm_r', 0.3, False)
assert abs(quat_dot(first.rotation, contact.rotation)) < 0.99
report = {'result': 'PASS', 'skeleton_socket': socket.get_path_name(), 'parent_bone': 'hand_r', 'swing_seconds': 0.8, 'contact_seconds': 0.3, 'bone_tracks': len(bones), 'player_anim_blueprint': abp.get_path_name(), 'checks': ['saved socket belongs to skeleton, zero offset', 'weapon montage and socket assigned', 'player uses dedicated AnimBP', 'hold/swing use correct skeleton and absolute animation', 'montage contains swing on DefaultSlot', 'single conditional holding-state node is connected', 'all bones return to initial pose', 'right arm moves at contact']}
Path(unreal.Paths.project_dir(), 'Saved', 'spanner_verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('SPANNER_VERIFICATION_PASS')
