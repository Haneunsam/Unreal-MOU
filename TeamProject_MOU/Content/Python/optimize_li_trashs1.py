"""
LI_Trashs1 최적화 스크립트 (검증된 최종 버전)

BP_Configuration_7 / _11 / _13(x5) / _14 액터들에 흩어져 있는
InstancedStaticMeshComponent(152개)를 고유 스태틱 메시 기준으로 묶어서,
새 액터 하나 아래에 HierarchicalInstancedStaticMeshComponent로 재구성한다.
Brush_0(빌더 브러시 잔재)도 함께 제거한다.

새로 만든 HISM에는 최적화 세팅(Mobility / Collision / Cull / DistanceField)도 함께 건다.
BP 컨스트럭션이 만든 원본 ISM은 인스턴스 단위 프로퍼티 오버라이드가 저장되지 않고
되돌아가기 때문에, 세팅은 반드시 '새로 만든 HISM'에 걸어야 한다.
그래서 병합과 세팅이 한 스크립트에 들어 있다.

컴포넌트 생성 + 인스턴스 채우기 + 원본 삭제 + 저장을 전부 한 세션 안에서
끝낸다 (중간에 MCP로 저장했다가 다시 읽는 방식은 월드 파티션 External Actor가
디스크에 실제로 안 써져서 실패했었음 - 반드시 이 스크립트 하나로 끝까지 실행할 것).

실행 방법:
  py "Content/Python/optimize_li_trashs1.py"

TARGET_LEVEL을 바꿔서 복제본으로 먼저 검증한 뒤, 실제 자산에 적용하세요.
"""

import unreal

TARGET_LEVEL = "/Game/06_HES/_Level/_Level_Instance/LI_Trashs1"
NEW_ACTOR_LABEL = "SM_TrashCluster_HISM"
TARGET_CLASS_PREFIX = "BP_Configuration_"

# --- 컬링 거리 ---------------------------------------------------------------
# 인스턴스가 이 거리(cm)를 넘어가면 통째로 그리지 않는다.
#
#   마을 안 바닥 디테일용   : 15000 / 25000  (150m / 250m) <- 현재값
#   맵 가장자리 쓰레기 산용 : 0 / 0          (컬링 끔)
#       가장자리 산은 맵 반대편(최대 504m)에서도 스카이라인으로 보여야 하므로
#       컬링을 걸면 산이 통째로 사라진다. 그 용도라면 0/0으로 바꿀 것.
INSTANCE_START_CULL = 15000
INSTANCE_END_CULL = 25000

# 콜리전은 같은 레벨의 SM_Trashs1_CollisionProxy1 액터가 담당한다.
# 152개 ISM에 QueryAndPhysics가 걸려 있던 건 순수한 중복이었다.
DISABLE_COLLISION = True

# 새 HISM에 적용할 세팅. (프로퍼티명, 값) 순서대로 set_editor_property 시도.
HISM_SETTINGS = [
    ("mobility", unreal.ComponentMobility.STATIC),
    ("instance_start_cull_distance", INSTANCE_START_CULL),
    ("instance_end_cull_distance", INSTANCE_END_CULL),
    ("affect_distance_field_lighting", False),
]
# -----------------------------------------------------------------------------


def _hism_component_name_for_mesh(mesh):
    short = mesh.get_name()
    if short.startswith("SM_"):
        short = short[3:]
    return "HISM_" + short


def _collect_and_delete_sources():
    mesh_to_transforms = {}
    actors_to_delete = []

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        cls_name = actor.get_class().get_name()
        is_config = cls_name.startswith(TARGET_CLASS_PREFIX)
        is_brush = isinstance(actor, unreal.Brush)

        if not (is_config or is_brush):
            continue

        if is_config:
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                mesh = comp.static_mesh
                if mesh is None:
                    continue
                count = comp.get_instance_count()
                for i in range(count):
                    world_tf = comp.get_instance_transform(i, world_space=True)
                    mesh_to_transforms.setdefault(mesh, []).append(world_tf)

        actors_to_delete.append(actor)

    return mesh_to_transforms, actors_to_delete


def _apply_settings(component):
    """새 HISM에 최적화 세팅을 건다. 실패한 항목 목록을 반환한다."""
    failed = []

    for prop_name, value in HISM_SETTINGS:
        try:
            component.set_editor_property(prop_name, value)
        except Exception as exc:
            failed.append(f"{prop_name}: {exc}")

    if DISABLE_COLLISION:
        # 콜리전은 전용 API로 꺼야 프로파일까지 같이 정리된다.
        try:
            component.set_collision_profile_name("NoCollision")
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        except Exception as exc:
            failed.append(f"collision: {exc}")

    return failed


def _add_hism_component(actor, subsystem, root_handle, comp_name, mesh):
    params = unreal.AddNewSubobjectParams(
        parent_handle=root_handle,
        new_class=unreal.HierarchicalInstancedStaticMeshComponent,
        blueprint_context=None,
    )
    new_handle, fail_reason = subsystem.add_new_subobject(params)
    if str(fail_reason):
        raise RuntimeError(f"add_new_subobject failed for {comp_name}: {fail_reason}")

    subsystem.rename_subobject(new_handle, unreal.Text(comp_name))
    subobj_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(new_handle)
    component = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(subobj_data)
    component.set_static_mesh(mesh)
    return component


def _build_hism_actor(mesh_to_transforms):
    new_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.Actor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
    )
    new_actor.set_actor_label(NEW_ACTOR_LABEL)

    # 루트가 Movable이면 자식 컴포넌트의 Static이 강제로 Movable로 되돌려진다.
    root = new_actor.get_editor_property("root_component")
    if root is not None:
        try:
            root.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        except Exception as exc:
            unreal.log_warning(f"[optimize_li_trashs1] 루트 Mobility 설정 실패: {exc}")

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root_handle = subsystem.k2_gather_subobject_data_for_instance(new_actor)[0]

    all_failures = []
    for mesh, transforms in mesh_to_transforms.items():
        comp_name = _hism_component_name_for_mesh(mesh)
        component = _add_hism_component(new_actor, subsystem, root_handle, comp_name, mesh)

        # 인스턴스를 넣기 전에 세팅을 걸어야 트리 빌드가 최종 세팅으로 한 번에 끝난다.
        failures = _apply_settings(component)
        if failures:
            all_failures.append(f"{comp_name} -> {failures}")

        for tf in transforms:
            component.add_instance(tf, True)

    if all_failures:
        for line in all_failures:
            unreal.log_warning(f"[optimize_li_trashs1] 세팅 실패 {line}")
    else:
        unreal.log("[optimize_li_trashs1] 모든 HISM에 최적화 세팅 적용 완료")

    return new_actor


def run():
    original_level = unreal.EditorLevelLibrary.get_editor_world().get_path_name().split(".")[0]

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_LEVEL)

    mesh_to_transforms, actors_to_delete = _collect_and_delete_sources()
    unreal.log(f"[optimize_li_trashs1] 고유 메시 {len(mesh_to_transforms)}종, "
               f"인스턴스 총 {sum(len(v) for v in mesh_to_transforms.values())}개, "
               f"삭제 대상 액터 {len(actors_to_delete)}개")

    new_actor = _build_hism_actor(mesh_to_transforms)

    for actor in actors_to_delete:
        unreal.EditorLevelLibrary.destroy_actor(actor)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(f"[optimize_li_trashs1] 완료: {new_actor.get_actor_label()} 생성, "
               f"컴포넌트 {len(mesh_to_transforms)}개, 원본 액터 {len(actors_to_delete)}개 삭제")

    if original_level and original_level != TARGET_LEVEL:
        unreal.EditorLoadingAndSavingUtils.load_map(original_level)


run()
