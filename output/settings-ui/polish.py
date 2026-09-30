import unreal, pathlib
src=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/migrate.py').read_text(encoding='utf-8-sig')
exec(src.split("bp=duplicate('WBP_SettingsMenuWidget'")[0])
bp=unreal.load_asset(DEST+'WBP_LobbySettingsWidget')
ws=widgets(bp)
for w in ws.values():
    if isinstance(w,unreal.ComboBoxString):
        w.set_editor_property('foreground_color',unreal.SlateColor(specified_color=unreal.LinearColor(.84,.94,1,1)))
    elif isinstance(w,unreal.CheckBox):
        w.slot.set_size(unreal.Vector2D(32,32))
        w.set_render_transform_pivot(unreal.Vector2D(0,0))
        w.set_render_scale(unreal.Vector2D(1.8,1.8))
    elif isinstance(w,unreal.Slider):
        style=w.get_editor_property('widget_style')
        style.set_editor_property('bar_thickness',5.0)
        w.set_editor_property('widget_style',style)
canvas=ws['CanvasPanel_32']
for i in range(3):
    bar=add(bp,unreal.Border,'LobbyTabIndicator'+str(i),canvas)
    bar.set_brush_color(unreal.LinearColor(.1,.8,1,1))
    bar.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    bar.set_render_opacity(1.0 if i==0 else 0.0)
    place(bp,bar,canvas,78+425*i,225,394,3)
assert lib.compile_blueprint(bp)
graph=unreal.BlueprintGraphEditor.get_graph_editor(lib.find_event_graph(bp))
for i,name in enumerate(['Button_GraphicsTab','Button_AudioTab','Button_ControlsTab']):
    before=set(n.get_name() for n in graph.list_all_nodes())
    assert tool('BindToEventProperty',bp,'OnClicked',name,unreal.Button)
    event=next(n for n in graph.list_all_nodes() if n.get_name() not in before)
    lib.set_node_pos(event,unreal.IntPoint(0,900+250*i))
    then=lib.find_then_pin(event)
    for j in range(3):
        get=graph.add_get_member_variable_node('LobbyTabIndicator'+str(j))
        set_opacity=graph.add_call_function_node('/Script/UMG.Widget:SetRenderOpacity')
        assert lib.find_output_pin(get,'LobbyTabIndicator'+str(j)).try_create_connection(lib.find_self_pin(set_opacity))
        assert lib.find_input_pin(set_opacity,'InOpacity').set_pin_value('1.0' if i==j else '0.0')
        assert then.try_create_connection(lib.find_execute_pin(set_opacity))
        then=lib.find_then_pin(set_opacity)
        lib.set_node_pos(get,unreal.IntPoint(250+300*j,1020+250*i))
        lib.set_node_pos(set_opacity,unreal.IntPoint(250+300*j,900+250*i))
assert lib.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
print('SETTINGS_POLISH_OK')
