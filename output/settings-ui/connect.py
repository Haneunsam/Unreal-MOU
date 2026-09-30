import unreal, pathlib
base='/Game/02_JSY/MainLobby/'
settings=unreal.load_asset(base+'WBP_LobbySettingsWidget')
key=unreal.load_asset(base+'WBP_LobbyKeyBindingMenu')
lib=unreal.BlueprintEditorLibrary
assert key
lib.reparent_blueprint(settings,unreal.LobbySettingsWidgetBase)
assert lib.compile_blueprint(settings)
unreal.get_default_object(settings.generated_class()).set_editor_property('is_focusable',True)
unreal.get_default_object(settings.generated_class()).set_editor_property('key_binding_menu_widget_class',key.generated_class())
lobby=unreal.load_asset(base+'WBP_LobbyWidget')
unreal.get_default_object(lobby.generated_class()).set_editor_property('settings_widget_class',settings.generated_class())
for bp in [settings,key,lobby,unreal.load_asset(base+'WBP_MainLobby')]:
    assert lib.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print('SETTINGS_COMPILE_OK',bp.get_path_name())
print('SETTINGS_CONNECT_OK')
