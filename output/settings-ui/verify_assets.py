import unreal, pathlib, json
out=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui')
api=unreal.get_default_object(unreal.UMGToolSet)
checks=[]
for source,target in [('WBP_SettingsMenuWidget','WBP_LobbySettingsWidget'),('WBP_KeyBindingMenu','WBP_LobbyKeyBindingMenu')]:
    original=unreal.load_asset('/Game/05_JYH/MainMenuUI/'+source)
    copied=unreal.load_asset('/Game/02_JSY/MainLobby/'+target)
    src={str(w.widget_name) for w in api.call_method('GetWidgets',(original,)).widgets}
    dst={str(w.widget_name) for w in api.call_method('GetWidgets',(copied,)).widgets}
    controls={n for n in src if n.startswith(('Button_','KeySelector_','ComboBox_','Slider_','CheckBox_','WidgetSwitcher_','ProgressBar_','Panel_Conflict','Text_Conflict'))}
    assert controls<=dst,controls-dst
    assert unreal.BlueprintEditorLibrary.compile_blueprint(copied)
    checks.append({'asset':target,'preserved_controls':sorted(controls),'compiled':True})
    (out/(target+'.txt')).write_text(api.call_method('GetWidgetDescription',(copied,None,-1)).description,encoding='utf-8')
    task=unreal.AssetExportTask()
    task.object=copied
    task.filename=str(out/(target+'.copy'))
    task.automated=True
    task.prompt=False
    unreal.Exporter.run_asset_export_task(task)
(out/'asset-validation.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print('SETTINGS_ASSETS_VERIFIED',len(checks))
