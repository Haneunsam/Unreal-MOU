import unreal, pathlib
out = pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui')
api = unreal.get_default_object(unreal.UMGToolSet)
paths = ['/Game/05_JYH/MainMenuUI/WBP_SettingsMenuWidget', '/Game/05_JYH/MainMenuUI/WBP_KeyBindingMenu', '/Game/02_JSY/MainLobby/WBP_MainLobby', '/Game/02_JSY/MainLobby/WBP_LobbyWidget']
for path in paths:
    bp = unreal.load_asset(path)
    result = api.call_method('GetWidgetDescription', (bp, None, -1))
    (out / (path.split('/')[-1] + '.txt')).write_text(result.description, encoding='utf-8')
    print('INSPECT', path, api.call_method('GetWidgets', (bp,)).info.parent_class)
    print('DEFAULT', unreal.get_default_object(bp.generated_class()))
    task = unreal.AssetExportTask()
    task.object = bp
    task.filename = str(out / (path.split('/')[-1] + '.copy'))
    task.automated = True
    task.prompt = False
    unreal.Exporter.run_asset_export_task(task)
print('TOOLS', [x for x in dir(unreal) if 'Tool' in x and ('Blueprint' in x or 'Graph' in x or 'Object' in x)])

