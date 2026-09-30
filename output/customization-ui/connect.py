import unreal, pathlib
api = unreal.get_default_object(unreal.UMGToolSet)
lib = unreal.BlueprintEditorLibrary
base = '/Game/02_JSY/MainLobby/'
bp = unreal.load_asset(base+'WBP_LobbyCustomizeWidget')
legacy = unreal.load_asset(base+'WBP_CustomizeWidget')
# [LCCON-001] 기존 로비 참조를 유지하는 호환 위젯을 새 화면의 자식으로 연결한다.
for graph in lib.list_graphs(legacy):
    lib.remove_graph(legacy, graph)
root = api.call_method('GetWidgets',(legacy,)).widgets[0].widget
assert api.call_method('RemoveWidget',(legacy,root))
lib.remove_unused_variables(legacy)
lib.reparent_blueprint(legacy,bp.generated_class())
picker = unreal.load_asset(base+'WBP_LobbyColorPickerWidget')
unreal.get_default_object(legacy.generated_class()).set_editor_property('color_picker_widget_class',picker.generated_class())
assert api.call_method('CompileWidgetBlueprint',(legacy,))
assert unreal.EditorAssetLibrary.save_loaded_asset(legacy)
print('LEGACY_CONNECTED')
