import pathlib, unreal
src=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/migrate.py').read_text(encoding='utf-8-sig')
exec(src.split("bp=duplicate('WBP_SettingsMenuWidget'")[0])
settings=unreal.load_asset(DEST+'WBP_LobbySettingsWidget')
graph=unreal.BlueprintGraphEditor.get_graph_editor(lib.find_event_graph(settings))
for comment in graph.list_comment_nodes(): graph.remove_comment_node(comment)
exec("bp=duplicate('WBP_KeyBindingMenu'"+src.split("bp=duplicate('WBP_KeyBindingMenu'")[1])
