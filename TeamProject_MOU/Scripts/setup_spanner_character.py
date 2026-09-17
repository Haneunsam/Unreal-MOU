import unreal
import json
from pathlib import Path

lib = unreal.EditorAssetLibrary
mesh = unreal.load_asset('/Game/EmoRobot00/Meshes/SKM_EmoRobot00')
skeleton = mesh.get_editor_property('skeleton')
socket = mesh.find_socket('SpannerSocket')
if not socket:
    # Use a transient mesh so only the skeleton gains a socket, without a mesh override.
    scratch = unreal.new_object(unreal.SkeletalMesh)
    scratch.skeleton = skeleton
    socket = unreal.new_object(unreal.SkeletalMeshSocket, outer=scratch)
    scratch.add_socket(socket, True)
    editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    assert editor.rename_socket(scratch, socket.get_editor_property('socket_name'), 'SpannerSocket')
    mesh.find_socket('SpannerSocket').set_socket_parent(mesh, 'hand_r')
    assert lib.save_loaded_asset(skeleton)
socket = mesh.find_socket('SpannerSocket')
assert socket and socket.get_outer() == skeleton

dest = '/Game/04_JJO/ItemAnim/Spanner'
hold_path = dest + '/AS_Spanner_Hold_R'
source = unreal.load_asset(dest + '/AS_Spanner_Swing_R')
hold = unreal.load_asset(hold_path) if lib.does_asset_exist(hold_path) else lib.duplicate_asset('/Game/04_JJO/ItemAnim/AO_Sequence/OneHand_Front_Sequence', hold_path)
controller = hold.get_editor_property('controller')
hold.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_NONE)
controller.open_bracket('Spanner grip pose', False)
for bone in unreal.AnimationLibrary.get_animation_track_names(source):
    pose = unreal.AnimationLibrary.get_bone_pose_for_time(source, bone, 0.0, False)
    assert controller.set_bone_track_keys(bone, [pose.translation]*2, [pose.rotation]*2, [pose.scale3d]*2, False)
controller.close_bracket(False)
assert lib.save_loaded_asset(hold)

abp_path = dest + '/ABP_JJO_SpannerPlayer'
abp = unreal.load_asset(abp_path) if lib.does_asset_exist(abp_path) else lib.duplicate_asset('/Game/05_JYH/MainPlayer/ABP_MainCharacter', abp_path)
graph = next(g for g in unreal.BlueprintEditorLibrary.list_graphs(abp) if g.get_name() == 'AnimGraph')
editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
nodes = editor.list_all_nodes()
if not any(n.get_node_title() == 'Get bIsHoldingHealingMelee' for n in nodes):
    carry = next(n for n in nodes if n.get_name() == 'AnimGraphNode_SequencePlayer_1')
    outgoing = list(carry.find_output_pin('Pose').list_connected_pins())
    blend = editor.create_node_from_name('Animation|Blends|BlendPosesbybool', unreal.Vector2D(-900, 1200), [])
    grip = editor.create_node_from_name("Animation|Sequences|Play'AS_Spanner_Hold_R'", unreal.Vector2D(-1400, 1000), [])
    state = editor.create_node_from_name('Variables|Animation|State|GetIsHoldingHealingMelee', unreal.Vector2D(-1400, 1350), [])
    assert blend and grip and state
    assert grip.find_output_pin('Pose').try_create_connection(blend.find_input_pin('BlendPose_0'))
    assert carry.find_output_pin('Pose').try_create_connection(blend.find_input_pin('BlendPose_1'))
    assert state.list_output_pins()[0].try_create_connection(blend.find_input_pin('bActiveValue'))
    for pin in outgoing:
        assert blend.find_output_pin('Pose').try_create_connection(pin)
    # This character uses Emo bone names, so its upper-body mask must start at spine.
    layer = next(n for n in nodes if n.get_name() == 'AnimGraphNode_LayeredBoneBlend_0')
    data = layer.get_editor_property('node')
    branch = unreal.BranchFilter()
    branch.set_editor_property('bone_name', 'spine')
    branch.set_editor_property('blend_depth', 1)
    mask = unreal.InputBlendPose()
    mask.set_editor_property('branch_filters', [branch])
    data.set_editor_property('layer_setup', [mask])
    layer.set_editor_property('node', data)
unreal.BlueprintEditorLibrary.compile_blueprint(abp)
assert not editor.list_nodes_with_errors()
assert lib.save_loaded_asset(abp)
player = unreal.load_asset('/Game/04_JJO/BP_JJO_EmoPlayer')
player_mesh = unreal.get_default_object(player.generated_class()).get_editor_property('mesh')
player_mesh.set_editor_property('anim_class', abp.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(player)
assert lib.save_loaded_asset(player)
report = {'socket': socket.get_path_name(), 'socket_bone': str(socket.get_editor_property('bone_name')), 'socket_location': str(socket.get_editor_property('relative_location'))}
report['types'] = [n for n in editor.list_available_nodes([]) if any(s in n.lower() for s in ['holdinghealing', 'blendposesbybool', 'spanner_hold'])]
report['nodes'] = []
for n in editor.list_all_nodes():
    if any(s in n.get_name() for s in ['SequencePlayer_1', 'Slot', 'LayeredBoneBlend', 'Root', 'BlendListByBool_0']):
        row = {'name': n.get_name(), 'title': n.get_node_title()}
        try:
            row['node'] = str(n.get_editor_property('node'))
        except Exception:
            pass
        row['methods'] = [x for x in dir(n) if any(s in x for s in ['pin', 'reconstruct'])]
        row['pins'] = [{'name': str(p.get_pin_name()), 'links': [str(q.get_owning_node().get_name()) + ':' + str(q.get_pin_name()) for q in p.list_connected_pins()]} for p in n.list_all_pins()]
        report['nodes'].append(row)
Path(unreal.Paths.project_dir(), 'Saved', 'spanner_setup.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
