import unreal, pathlib
out = pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/customization-ui')
for name in ['WaitingPanel','CharacterBackgroundBlock','RoomTitleBlock','NicknameBlock','CustomizingBlock','ExitBlock']:
    t = unreal.AssetExportTask()
    t.object = unreal.load_asset('/Game/02_JSY/MainLobby/LobbyUI/Lobby/' + name)
    t.filename = str(out / (name + '.png'))
    t.automated = True
    t.prompt = False
    unreal.Exporter.run_asset_export_task(t)
for i in [8,9,10,11,12,13]:
    name = 'T_Lobby_%02d' % i
    t = unreal.AssetExportTask()
    t.object = unreal.load_asset('/Game/02_JSY/MainLobby/LobbyUI/Textures/' + name)
    t.filename = str(out / (name + '.png'))
    t.automated = True
    t.prompt = False
    unreal.Exporter.run_asset_export_task(t)
