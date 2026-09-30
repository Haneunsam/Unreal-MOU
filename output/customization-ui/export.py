import unreal, pathlib
out = pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/customization-ui')
print('EXPORTERS', [x for x in dir(unreal) if 'Exporter' in x or 'BlueprintEditorLibrary' in x])
print('BPAPI', dir(unreal.BlueprintEditorLibrary))
for name, path in [('character','/Game/05_JYH/Customization_NPC/WBP_CharacterCustomizationWidget'),('picker','/Game/05_JYH/Customization_NPC/WBP_ColorPickerWidget')]:
    bp = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = bp
    task.filename = str(out / (name + '.copy'))
    task.automated = True
    task.prompt = False
    print('EXPORT', unreal.Exporter.run_asset_export_task(task))
    cdo = unreal.get_default_object(bp.generated_class())
    print('CDO', name, cdo.get_editor_property('color_picker_widget_class') if name == 'character' else '')
