"""팀원 Blueprint 그래프를 보존하여 로비 전용 화면으로 복제한다."""
import unreal, pathlib

OUT = pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/customization-ui')
DEST = '/Game/02_JSY/MainLobby/'
SOURCE = '/Game/05_JYH/Customization_NPC/'
api = unreal.get_default_object(unreal.UMGToolSet)
lib = unreal.BlueprintEditorLibrary

# [LCMIG-001] 위젯 도구의 편집 함수를 호출한다.
def tool(name, *args):
    return api.call_method(name, args)

# [LCMIG-002] 이름을 키로 원본 위젯을 조회한다.
def widgets(bp):
    return {str(i.widget_name): i.widget for i in tool('GetWidgets', bp).widgets}

# [LCMIG-003] 새 위젯을 추가하고 Blueprint 변수로 노출한다.
def add(bp, cls, name, parent):
    w = tool('AddWidget', bp, cls, name, parent, -1).widget
    tool('ToggleWidgetAsVariable', bp, w, True)
    return w

# [LCMIG-004] 기존 그래프가 참조하는 위젯을 유지하며 캔버스에 배치한다.
def place(bp, w, canvas, x, y, width, height):
    slot = tool('MoveWidget', bp, w, canvas, -1).slot if w.get_parent() != canvas else w.slot
    slot.set_anchors(unreal.Anchors())
    slot.set_alignment(unreal.Vector2D(0, 0))
    slot.set_position(unreal.Vector2D(x, y))
    slot.set_size(unreal.Vector2D(width, height))
    return w

# [LCMIG-005] 기존 로비 텍스처의 브러시를 생성한다.
def brush(path, tint=(1,1,1,1), box=False):
    b = unreal.SlateBrush()
    b.set_editor_property('resource_object', unreal.load_asset(path))
    b.set_editor_property('draw_as', unreal.SlateBrushDrawType.BOX if box else unreal.SlateBrushDrawType.IMAGE)
    b.set_editor_property('tint_color', unreal.SlateColor(specified_color=unreal.LinearColor(*tint)))
    if box: b.set_editor_property('margin', unreal.Margin(.07,.13,.07,.13))
    return b

# [LCMIG-006] 빈 로비 버튼 리소스에 한국어 텍스트를 올린다.
def style_button(w):
    path = DEST + 'LobbyUI/Textures/T_Lobby_09'
    s = w.get_editor_property('widget_style')
    for prop, tint in [('normal',(1,1,1,1)),('hovered',(1.35,1.35,1.35,1)),('pressed',(.65,.9,1,1)),('disabled',(.4,.4,.4,1))]:
        s.set_editor_property(prop, brush(path,tint,True))
    s.set_editor_property('normal_padding', unreal.Margin(10,4,10,4))
    s.set_editor_property('pressed_padding', unreal.Margin(10,5,10,3))
    w.set_style(s)

# [LCMIG-007] 읽기 쉬운 로비 텍스트 스타일과 표시 내용을 설정한다.
def text_style(w, text=None, size=20, color=(.84,.94,1,1)):
    if text is not None: w.set_text(text)
    f = w.get_editor_property('font')
    f.set_editor_property('font_object', unreal.load_asset('/Engine/EngineFonts/Roboto'))
    f.set_editor_property('typeface_font_name','Regular')
    f.set_editor_property('size',size)
    w.set_font(f)
    w.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(*color)))
    w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

# [LCMIG-008] 고정 설계 캔버스를 화면 크기에 맞춰 축소한다.
def responsive(bp, root, width, height):
    size = tool('WrapWidgets',bp,[root],unreal.SizeBox)[0].widget
    size.set_width_override(width)
    size.set_height_override(height)
    scale = tool('WrapWidgets',bp,[size],unreal.ScaleBox)[0].widget
    scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    return scale

# [LCMIG-009] 원본을 건드리지 않고 신규 에셋을 만든다.
def duplicate(source, name):
    target = DEST + name
    assert not unreal.EditorAssetLibrary.does_asset_exist(target), '이미 생성된 에셋: ' + target
    return unreal.EditorAssetLibrary.duplicate_asset(SOURCE + source, target)

bp = duplicate('WBP_CharacterCustomizationWidget','WBP_LobbyCustomizeWidget')
lib.reparent_blueprint(bp, unreal.LobbyCustomizeWidgetBase)
ws = widgets(bp)
root = ws['CanvasPanel_43']
frame = add(bp,unreal.Image,'LobbyPanel',root)
frame.set_brush(brush(DEST+'LobbyUI/Lobby/WaitingPanel',box=True))
frame.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
place(bp,frame,root,20,20,1360,840).slot.set_z_order(-10)
portrait = add(bp,unreal.Image,'PortraitPanel',root)
portrait.set_brush(brush(DEST+'LobbyUI/Lobby/CharacterBackgroundBlock'))
portrait.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
place(bp,portrait,root,62,154,430,584).slot.set_z_order(-5)
preview = add(bp,unreal.Image,'PreviewImage',root)
place(bp,preview,root,74,174,402,548)
preview.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
hint = add(bp,unreal.TextBlock,'PreviewHint',root)
text_style(hint,'드래그하여 회전  ·  내 캐릭터 미리보기',18)
place(bp,hint,root,70,735,430,32)
status = add(bp,unreal.TextBlock,'CustomizationStatusText',root)
text_style(status,'',16)
place(bp,status,root,70,110,1250,32)
positions = {
 'Text_Title':(70,52,1240,42), 'Text_NPCGreeting':(70,98,1240,30),
 'Text_BaseHeader':(550,154,720,32), 'BodyColor':(560,205,170,36),
 'Btn_BodyColor':(1120,200,150,44),
 'TextBlock_0':(560,265,180,32),'Slider_Metallic':(760,265,510,32),
 'TextBlock_1':(560,323,180,32),'Slider_RoughnessA':(760,323,510,32),
 'TextBlock_2':(560,381,180,32),'Slider_RoughnessB':(760,381,510,32),
 'TextBlock_174':(550,445,720,32),'Btn_DecalPrev':(560,500,50,50),
 'SizeBox_0':(630,490,140,140),'Btn_DecalNext':(790,500,50,50),
 'DecalColor':(890,502,190,36),'Btn_DecalColor':(1120,500,150,44),
 'TextBlock':(560,652,180,32),'Slider_TilingX':(760,652,510,32),
 'TextBlock_3':(560,710,180,32),'Slider_TilingY':(760,710,510,32),
 'Btn_Cancel':(570,785,210,48),'Btn_Reset':(805,785,210,48),'Btn_Confirm':(1040,785,230,48)}
for name,rect in positions.items():
    if name in ws: place(bp,ws[name],root,*rect)
for name in ['Background_DragArea','Panel_Right_Border']:
    ws[name].set_visibility(unreal.SlateVisibility.COLLAPSED)
for w in ws.values():
    if isinstance(w,unreal.TextBlock): text_style(w)
    if isinstance(w,unreal.Button): style_button(w)
    if isinstance(w,unreal.Slider):
        w.set_render_scale(unreal.Vector2D(1,1))
        w.set_slider_bar_color(unreal.LinearColor(.04,.22,.36,1))
        w.set_slider_handle_color(unreal.LinearColor(.25,.85,1,1))
        w.set_step_size(.01)
labels = {'Text_Title':'캐릭터 커스터마이징','Text_NPCGreeting':'변경 사항은 이 창에서만 미리 보입니다. 확인을 누르면 내 플레이어 슬롯에 적용됩니다.',
 'Text_BaseHeader':'01   바디 · 표면','BodyColor':'바디 색상','TextBlock_0':'금속성','TextBlock_1':'거칠기 A','TextBlock_2':'거칠기 B',
 'TextBlock_174':'02   데칼 · 패턴','DecalColor':'데칼 색상','TextBlock':'가로 반복','TextBlock_3':'세로 반복','Confirm':'확인 · 적용','Cancel':'취소','Reset':'기본값 복원'}
for name,label in labels.items():
    if name in ws: text_style(ws[name],label,32 if name=='Text_Title' else 16 if name=='Text_NPCGreeting' else 20)
for name in ['Text_Title','Text_NPCGreeting','Text_BaseHeader','TextBlock_174']:
    ws[name].set_editor_property('justification', unreal.TextJustify.LEFT)
for name in ['Btn_BodyColor','Btn_DecalColor']:
    label = add(bp,unreal.TextBlock,name+'Label',ws[name])
    text_style(label,'색상 선택',18)
ws['SizeBox_0'].set_width_override(140)
ws['SizeBox_0'].set_height_override(140)
for name in ['Slider_TilingX','Slider_TilingY']:
    ws[name].set_min_value(.01)
    ws[name].set_max_value(20)
responsive(bp,root,1400,880)

picker = duplicate('WBP_ColorPickerWidget','WBP_LobbyColorPickerWidget')
pw = widgets(picker)
pw['Window_Border'].set_brush(brush(DEST+'LobbyUI/Lobby/WaitingPanel',box=True))
pw['Window_Border'].set_content_color_and_opacity(unreal.LinearColor(1,1,1,1))
pw['Window_Border'].slot.set_position(unreal.Vector2D(0,0))
for w in pw.values():
    if isinstance(w,unreal.Button): style_button(w)
    if isinstance(w,unreal.TextBlock): text_style(w)
unreal.get_default_object(bp.generated_class()).set_editor_property('color_picker_widget_class',picker.generated_class())
for asset in [picker,bp]:
    assert tool('CompileWidgetBlueprint',asset), 'Blueprint 컴파일 실패'
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    (OUT/(asset.get_name()+'-layout.txt')).write_text(tool('GetWidgetDescription',asset,None,-1).description,encoding='utf-8')
lobby = unreal.load_asset(DEST+'WBP_LobbyWidget')
unreal.get_default_object(lobby.generated_class()).set_editor_property('customize_widget_class',bp.generated_class())
assert tool('CompileWidgetBlueprint',lobby)
assert unreal.EditorAssetLibrary.save_loaded_asset(lobby)
print('MIGRATION_COMPLETE')

