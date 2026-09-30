import unreal, pathlib, json
out=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/customization-ui')
api=unreal.get_default_object(unreal.UMGToolSet)
lib=unreal.BlueprintEditorLibrary
base='/Game/02_JSY/MainLobby/'
checks=[]
for name in ['WBP_CustomizeWidget','WBP_LobbyCustomizeWidget','WBP_LobbyColorPickerWidget','WBP_LobbyWidget']:
    bp=unreal.load_asset(base+name)
    compiled=api.call_method('CompileWidgetBlueprint',(bp,))
    checks.append({'asset':name,'compiled':compiled,'parent':lib.get_blueprint_parent_class(bp).get_path_name(),'graphs':[g.get_name() for g in lib.list_graphs(bp)]})
    (out/(name+'-final-layout.txt')).write_text(api.call_method('GetWidgetDescription',(bp,None,-1)).description,encoding='utf-8')
    assert compiled,name
for original,copy in [('WBP_CharacterCustomizationWidget','WBP_LobbyCustomizeWidget'),('WBP_ColorPickerWidget','WBP_LobbyColorPickerWidget')]:
    source=unreal.load_asset('/Game/05_JYH/Customization_NPC/'+original)
    dest=unreal.load_asset(base+copy)
    assert sorted(g.get_name() for g in lib.list_graphs(source))==sorted(g.get_name() for g in lib.list_graphs(dest))
lobby=unreal.get_default_object(unreal.load_asset(base+'WBP_LobbyWidget').generated_class())
assert lobby.get_editor_property('customize_widget_class').get_name()=='WBP_LobbyCustomizeWidget_C'
page=unreal.get_default_object(unreal.load_asset(base+'WBP_LobbyCustomizeWidget').generated_class())
assert page.get_editor_property('color_picker_widget_class').get_name()=='WBP_LobbyColorPickerWidget_C'
(out/'asset-validation.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print('WIDGET_API', [(x,str(getattr(unreal.WidgetLibrary,x).__doc__)) for x in dir(unreal.WidgetLibrary) if 'all_widgets' in x])
print('ASSETS_VERIFIED')
