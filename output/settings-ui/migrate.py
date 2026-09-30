"""팀원 설정/키 설정을 로비 전용 복사본으로 이식한다. 원본은 저장하지 않는다."""
import unreal, pathlib, json
BASE = pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output')
exec((BASE/'customization-ui/migrate.py').read_text(encoding='utf-8-sig').split('bp = duplicate(')[0])
OUT = BASE/'settings-ui'
SOURCE = '/Game/05_JYH/MainMenuUI/'

# [LSMIG-001] 로비 패널과 제목을 설정 화면의 공통 배경으로 배치한다.
def frame(bp, root, title, subtitle):
    panel = add(bp,unreal.Image,'LobbyPanel',root)
    panel.set_brush(brush(DEST+'LobbyUI/Lobby/WaitingPanel',box=True))
    panel.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    place(bp,panel,root,20,20,1360,840).slot.set_z_order(-10)
    label(bp,root,'LobbySettingsTitle',title,70,50,1240,48,32)
    label(bp,root,'LobbySettingsSubtitle',subtitle,70,108,1240,30,18)

# [LSMIG-002] 설명과 항목 제목을 공통 글꼴로 배치한다.
def label(bp,root,name,text,x,y,w,h,size=20):
    widget = add(bp,unreal.TextBlock,name,root)
    text_style(widget,text,size)
    place(bp,widget,root,x,y,w,h)
    return widget

# [LSMIG-003] 스크롤 영역에 일정한 크기의 항목 캔버스를 생성한다.
def content_canvas(bp,scroll,name,width,height):
    size=add(bp,unreal.SizeBox,name+'Size',scroll)
    size.set_width_override(width)
    size.set_height_override(height)
    canvas=add(bp,unreal.CanvasPanel,name,size)
    scroll.set_scrollbar_thickness(unreal.Vector2D(6,6))
    return canvas,size

# [LSMIG-004] 화면마다 포커스를 받아 Esc가 현재 화면에서 먼저 처리되게 한다.
def focus_event(bp,event_name):
    graph=unreal.BlueprintGraphEditor.get_graph_editor(lib.find_event_graph(bp))
    event=graph.find_event_node(event_name)
    if not event:
        event=lib.add_event_override(bp,event_name,unreal.IntPoint(0,600))
    call=graph.add_call_function_node('/Script/UMG.Widget:SetFocus')
    assert call and event
    assert lib.find_then_pin(event).try_create_connection(lib.find_execute_pin(call))
    lib.set_node_pos(call,unreal.IntPoint(350,600))

# [LSMIG-005] 버튼과 입력 위젯을 기존 로비 리소스로 통일한다.
def restyle(bp):
    for w in widgets(bp).values():
        if isinstance(w,unreal.TextBlock):
            text_style(w,size=20)
        elif isinstance(w,unreal.Button):
            style_button(w)
        elif isinstance(w,unreal.Slider):
            w.set_slider_bar_color(unreal.LinearColor(.045,.14,.22,1))
            w.set_slider_handle_color(unreal.LinearColor(.28,.84,1,1))
            w.set_step_size(.01)
        elif isinstance(w,unreal.InputKeySelector):
            s=w.get_editor_property('widget_style')
            for prop,tint in [('normal',(1,1,1,1)),('hovered',(1.35,1.35,1.35,1)),('pressed',(.65,.9,1,1))]:
                s.set_editor_property(prop,brush(DEST+'LobbyUI/Textures/T_Lobby_09',tint,True))
            w.set_editor_property('widget_style',s)
            ts=w.get_editor_property('text_style')
            f=ts.get_editor_property('font')
            f.set_editor_property('font_object',unreal.load_asset('/Engine/EngineFonts/Roboto'))
            f.set_editor_property('typeface_font_name','Regular')
            f.set_editor_property('size',18)
            ts.set_editor_property('font',f)
            w.set_editor_property('text_style',ts)
            w.set_key_selection_text('입력 대기…')
            w.set_no_key_specified_text('미지정')
        elif isinstance(w,unreal.ComboBoxString):
            s=w.get_editor_property('widget_style')
            bs=s.get_editor_property('combo_button_style')
            button=bs.get_editor_property('button_style')
            for prop,tint in [('normal',(1,1,1,1)),('hovered',(1.3,1.3,1.3,1)),('pressed',(.65,.9,1,1))]:
                button.set_editor_property(prop,brush(DEST+'LobbyUI/Textures/T_Lobby_09',tint,True))
            bs.set_editor_property('button_style',button)
            s.set_editor_property('combo_button_style',bs)
            w.set_editor_property('widget_style',s)
            f=w.get_editor_property('font')
            f.set_editor_property('font_object',unreal.load_asset('/Engine/EngineFonts/Roboto'))
            f.set_editor_property('typeface_font_name','Regular')
            f.set_editor_property('size',20)
            w.set_editor_property('font',f)
            w.set_editor_property('content_padding',unreal.Margin(18,8,18,8))

bp=duplicate('WBP_SettingsMenuWidget','WBP_LobbySettingsWidget')
ws=widgets(bp)
root=ws['CanvasPanel_32']
frame(bp,root,'환경설정','화면 · 오디오 · 조작 설정')
for i,n in enumerate(['Button_GraphicsTab','Button_AudioTab','Button_ControlsTab']):
    place(bp,ws[n],root,70+425*i,165,410,56)
place(bp,ws['WidgetSwitcher_Tabs'],root,70,245,1260,480)
for n,x in [('Button_ResetDefaults',70),('Button_Back',920),('Button_Apply',1130)]:
    place(bp,ws[n],root,x,780,200,50)
ws['TextBlock'].set_text('기본값 복원')
ws['TextBlock_398'].set_text('조작 / 키 설정')
label(bp,root,'SettingsSaveHint','설정 변경 후 적용을 눌러 저장하세요. 키 설정은 변경 즉시 저장됩니다.',70,738,1250,28,17)

rows=[('TextBlock_3','ComboBox_Resolution'),('TextBlock_4','ComboBox_WindowMode'),('TextBlock_5','ComboBox_Quality'),('TextBlock_6','CheckBox_VSync'),('TextBlock_7','ComboBox_FrameRateLimit')]
for tab in ['Graphics','Audio','Controls']:
    scroll=ws['Panel_'+tab]
    original=list(scroll.get_all_children())
    canvas,size=content_canvas(bp,scroll,'Lobby'+tab+'Content',1230,460)
    if tab=='Graphics':
        for i,(text,control) in enumerate(rows):
            y=24+i*82
            place(bp,ws[text],canvas,24,y+9,430,40)
            place(bp,ws[control],canvas,530,y,640,50)
        ws['TextBlock_5'].set_text('그래픽 품질')
        ws['TextBlock_7'].set_text('프레임 제한')
    elif tab=='Audio':
        for i,(text,key) in enumerate([('TextBlock_8','MasterVolume'),('TextBlock_9','BGMVolume'),('TextBlock_10','SFXVolume'),('TextBlock_11','VoiceVolume'),('TextBlock_12','MicSensitivity')]):
            y=20+i*70
            place(bp,ws[text],canvas,24,y+6,380,40)
            place(bp,ws['Slider_'+key],canvas,450,y,580,44)
            label(bp,canvas,'Text_'+key,'100%',1070,y+6,125,40)
        label(bp,canvas,'MicLevelLabel','마이크 입력',24,390,380,34)
        place(bp,ws['ProgressBar_MicLevel'],canvas,450,390,725,24)
        ws['ProgressBar_MicLevel'].set_fill_color_and_opacity(unreal.LinearColor(.15,.8,1,1))
    else:
        for i,(text,control,value) in enumerate([('Text','Slider_MouseSensitivity','Text_MouseSensitivity'),('FOV','Slider_FOV','Text_FOV')]):
            y=24+i*92
            place(bp,ws[text],canvas,24,y+6,380,40)
            place(bp,ws[control],canvas,450,y,580,44)
            place(bp,ws[value],canvas,1070,y+6,125,40)
        place(bp,ws['Text_FOV_1'],canvas,24,214,380,40)
        place(bp,ws['CheckBox_InvertY'],canvas,530,208,640,50)
        place(bp,ws['Button_OpenKeyBindings'],canvas,450,310,725,58)
        label(bp,canvas,'KeyBindingHelp','이동 / 상호작용 / 무전 / 슬롯',24,320,410,40,18)
    for child in original: tool('RemoveWidget',bp,child)
tool('RemoveWidget',bp,ws['Border_2'])
tool('RemoveWidget',bp,ws['HBox_Header_Tabs'])
restyle(bp)
text_style(widgets(bp)['LobbySettingsTitle'],size=32)
text_style(widgets(bp)['LobbySettingsSubtitle'],size=18)
text_style(widgets(bp)['SettingsSaveHint'],size=17)
responsive(bp,root,1400,880)
focus_event(bp,'Construct')
assert tool('CompileWidgetBlueprint',bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
settings=bp

bp=duplicate('WBP_KeyBindingMenu','WBP_LobbyKeyBindingMenu')
ws=widgets(bp)
root=ws['CanvasPanel_53']
frame(bp,root,'키 설정','변경할 키를 선택한 뒤 새 키를 누르세요. 중복된 키는 확인 후 변경됩니다.')
place(bp,ws['ScrollBox_KeyList'],root,70,158,1260,582)
original=list(ws['ScrollBox_KeyList'].get_all_children())
canvas,size=content_canvas(bp,ws['ScrollBox_KeyList'],'LobbyKeyList',1230,580)
left=['MoveForward','MoveBackward','MoveLeft','MoveRight','Sprint','Jump','Light','LightColor','RadioPower','RadioTransmit','VoiceMute']
right=['Interact','GrabDrop','Use','Throw','Slap','Slot1','Slot2','Slot3','ViewEconomy','Quest','Emote']
for col,keys in enumerate([left,right]):
    x=24+630*col
    label(bp,canvas,'KeyGroup'+str(col),'이동 · 장비 · 음성' if col==0 else '행동 · 인벤토리 · 정보',x,0,570,34,22)
    for i,key in enumerate(keys):
        selector=ws['KeySelector_'+key]
        text=next(c for c in selector.get_parent().get_all_children() if isinstance(c,unreal.TextBlock))
        place(bp,text,canvas,x,48+i*47,315,38)
        place(bp,selector,canvas,x+330,40+i*47,230,42)
for child in original: tool('RemoveWidget',bp,child)
place(bp,ws['Button_ResetDefaults'],root,70,780,230,50)
place(bp,ws['Button_Back'],root,1090,780,240,50)
ws['TextBlock_198'].set_text('환경설정으로')
tool('RemoveWidget',bp,ws['Border_0'])
label(bp,root,'KeySaveHint','키 변경은 즉시 저장됩니다.  ·  Esc: 이전 화면',70,742,1250,28,17)

modal=ws['Panel_ConflictModal']
place(bp,modal,root,0,0,1400,880).slot.set_z_order(100)
modal.set_brush_color(unreal.LinearColor(.005,.015,.03,.97))
modal.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
modal.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
modal_size=add(bp,unreal.SizeBox,'ConflictSize',root)
modal_size.set_width_override(850)
modal_size.set_height_override(340)
modal_canvas=add(bp,unreal.CanvasPanel,'ConflictCanvas',modal_size)
modal_panel=add(bp,unreal.Image,'ConflictPanel',modal_canvas)
modal_panel.set_brush(brush(DEST+'LobbyUI/Lobby/WaitingPanel',box=True))
place(bp,modal_panel,modal_canvas,0,0,850,340)
label(bp,modal_canvas,'ConflictTitle','키 중복 확인',50,35,750,42,26)
message=ws['Text_ConflictWarningMessage']
place(bp,message,modal_canvas,50,100,750,110)
message.set_auto_wrap_text(True)
for name,text,x in [('Button_ConflictCancel','취소',365),('Button_ConflictConfirm','변경',590)]:
    b=ws[name]
    place(bp,b,modal_canvas,x,250,210,50)
    text_style(b.get_child_at(0),text)
tool('RemoveWidget',bp,ws['Border_307'])
tool('MoveWidget',bp,modal_size,modal,-1)
modal.set_visibility(unreal.SlateVisibility.COLLAPSED)
restyle(bp)
text_style(widgets(bp)['LobbySettingsTitle'],size=32)
text_style(widgets(bp)['LobbySettingsSubtitle'],size=18)
text_style(widgets(bp)['KeySaveHint'],size=17)
text_style(widgets(bp)['ConflictTitle'],size=26)
responsive(bp,root,1400,880)
focus_event(bp,'BP_OnMenuOpen')
assert tool('CompileWidgetBlueprint',bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
unreal.get_default_object(settings.generated_class()).set_editor_property('key_binding_menu_widget_class',bp.generated_class())
assert unreal.EditorAssetLibrary.save_loaded_asset(settings)
for asset in [settings,bp]:
    (OUT/(asset.get_name()+'.txt')).write_text(tool('GetWidgetDescription',asset,None,-1).description,encoding='utf-8')
print('SETTINGS_MIGRATION_OK')
