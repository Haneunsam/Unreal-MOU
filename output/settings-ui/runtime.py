"""격리된 UserDir에서 실제 PIE 위젯의 설정/키 변경과 로비 복귀를 검증한다."""
import unreal, time, json, pathlib, traceback, base64
OUT=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui')
state={'phase':0,'since':time.monotonic(),'start':time.monotonic(),'checks':[]}
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
app=unreal.get_default_object(unreal.EditorAppToolset)
slate=unreal.get_default_object(unreal.SlateInspectorToolset)
editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
assert 'settings-ui/test-user' in unreal.Paths.project_saved_dir().replace('\\','/'), unreal.Paths.project_saved_dir()
play=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
play.set_editor_property('PlayNumberOfClients',1)
play.set_editor_property('NewWindowWidth',1440)
play.set_editor_property('NewWindowHeight',1000)

# [LSTEST-001] 결과를 기록하고 실패하면 후속 변경을 중단한다.
def check(name,result,detail=''):
    state['checks'].append({'name':name,'pass':bool(result),'detail':str(detail)})
    assert result,name+': '+str(detail)

# [LSTEST-002] 렌더링과 닫기 타이머가 완료된 뒤 다음 검증으로 진행한다.
def advance():
    state['phase']+=1
    state['since']=time.monotonic()
    print('SETTINGS_TEST_PHASE',state['phase'])

# [LSTEST-003] 실제 렌더링 결과와 위젯 트리를 남긴다.
def capture(name):
    (OUT/(name+'.txt')).write_text(slate.call_method('Snapshot',('',30,False)),encoding='utf-8')
    pic=slate.call_method('Screenshot',('',))
    if pic.data: (OUT/(name+'.png')).write_bytes(base64.b64decode(pic.data))

# [LSTEST-004] 테스트 PIE만 정리하고 검증 결과를 저장한다.
def finish(error=None):
    if error: state['error']=error
    (OUT/'runtime-validation.json').write_text(json.dumps({k:state[k] for k in ['checks','error'] if k in state},ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    app.call_method('StopPIE',())
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

# [LSTEST-005] 부모 패널에 연결된 로비 설정 페이지를 찾는다.
def settings_page():
    return next((w for w in unreal.WidgetLibrary.get_all_widgets_of_class(state['world'],unreal.LobbySettingsWidgetBase,False) if w.get_parent()),None)

# [LSTEST-006] 뷰포트에 열린 로비용 키 설정 팝업을 찾는다.
def key_page():
    return next((w for w in unreal.WidgetLibrary.get_all_widgets_of_class(state['world'],unreal.KeyBindingMenuWidget,False) if w.is_in_viewport()),None)

# [LSTEST-007] FKey를 만들어 키 선택기와 동일한 입력으로 검증한다.
def key(name):
    value=unreal.Key()
    value.set_editor_property('key_name',name)
    return value

# [LSTEST-009] FKey 래퍼 주소 대신 저장된 키 이름을 비교한다.
def bound(action,default='Invalid'):
    return str(state['config'].get_custom_key_binding(action,key(default)).get_editor_property('key_name'))

# [LSTEST-008] 설정 저장, 키 충돌 취소/승인, 연결된 액션, 페이지 수명을 검증한다.
def tick(delta):
    try:
        now=time.monotonic()
        if now-state['start']>180: raise RuntimeError('settings runtime timeout phase '+str(state['phase']))
        if now-state['since']<2: return
        p=state['phase']
        if p==0:
            worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds: return
            world=worlds[0]
            state['world']=world
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            cls=unreal.load_class(None,'/Game/02_JSY/MainLobby/WBP_LobbyWidget.WBP_LobbyWidget_C')
            lobby=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,cls,pc))
            lobby.add_to_viewport(100)
            state['lobby']=lobby
            state['config']=unreal.MOU_GameUserSettings.get_mou_game_user_settings()
            check('User settings are isolated', 'settings-ui/test-user' in unreal.Paths.generated_config_dir().replace('\\','/'),unreal.Paths.generated_config_dir())
            advance()
        elif p==1:
            main=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(state['world'],unreal.LobbyMainWidgetBase,False) if w.get_parent())
            state['main']=main
            check('Main lobby starts at depth 1',state['lobby'].get_stack_depth()==1)
            main.call_method('HandleSettingsClicked',())
            advance()
        elif p==2:
            w=settings_page()
            state['settings']=w
            check('Main lobby settings button opens migrated page',w is not None and 'WBP_LobbySettingsWidget' in w.get_class().get_name())
            check('Settings pushed on lobby stack',state['lobby'].get_stack_depth()==2)
            check('Graphics tab defaults to first page',w.get_editor_property('widget_switcher_tabs').get_active_widget_index()==0)
            capture('settings-graphics')
            w.call_method('OnAudioTabClicked',())
            advance()
        elif p==3:
            w=state['settings']
            check('Audio tab works',w.get_editor_property('widget_switcher_tabs').get_active_widget_index()==1)
            for prop,val in [('MasterVolume',.63),('BGMVolume',.41),('SFXVolume',.52),('VoiceVolume',.74),('MicSensitivity',.35)]:
                w.call_method('On'+prop+'Changed',(val,))
            w.refresh_ui_from_settings()
            check('Volume and microphone values reach user settings',abs(state['config'].get_master_volume()-.63)<.001 and abs(state['config'].get_mic_sensitivity()-.35)<.001)
            capture('settings-audio')
            w.call_method('OnControlsTabClicked',())
            advance()
        elif p==4:
            w=state['settings']
            check('Controls tab works',w.get_editor_property('widget_switcher_tabs').get_active_widget_index()==2)
            w.call_method('OnMouseSensitivityChanged',(1.4,))
            w.call_method('OnFOVChanged',(95.0,))
            w.call_method('OnInvertYChanged',(True,))
            w.refresh_ui_from_settings()
            capture('settings-controls')
            w.call_method('OnApplyClicked',())
            saved=list(pathlib.Path(unreal.Paths.generated_config_dir()).rglob('GameUserSettings.ini'))
            check('Apply writes isolated settings file',bool(saved),saved)
            text=saved[0].read_text(encoding='utf-8-sig')
            check('Audio and controls saved', 'MasterVolume=0.630000' in text and 'MouseSensitivity=1.400000' in text and 'FieldOfView=95.000000' in text)
            w.call_method('OnOpenKeyBindingsClicked',())
            advance()
        elif p==5:
            w=key_page()
            state['keys']=w
            check('Controls opens migrated key menu',w is not None and 'WBP_LobbyKeyBindingMenu' in w.get_class().get_name())
            check('Key menu hides settings page',state['settings'].get_visibility()==unreal.SlateVisibility.COLLAPSED)
            w.call_method('OnResetDefaultsClicked',())
            check('Key defaults clear custom overrides',len(state['config'].get_all_custom_key_bindings())==0)
            capture('settings-keybindings')
            w.call_method('OnJumpKeySelected',(unreal.InputChord(key=key('J')),))
            check('Unique key saves immediately',bound('IA_Jump')=='J',bound('IA_Jump'))
            w.call_method('OnJumpKeySelected',(unreal.InputChord(key=key('W')),))
            advance()
        elif p==6:
            w=state['keys']
            check('Duplicate key opens conflict modal',w.get_editor_property('panel_conflict_modal').get_visibility()==unreal.SlateVisibility.VISIBLE)
            check('Conflict pending leaves saved key untouched',bound('IA_Jump')=='J')
            capture('settings-key-conflict')
            w.call_method('OnConflictCancelClicked',())
            check('Conflict cancel restores key',bound('IA_Jump')=='J' and w.get_editor_property('panel_conflict_modal').get_visibility()==unreal.SlateVisibility.COLLAPSED)
            w.call_method('OnJumpKeySelected',(unreal.InputChord(key=key('W')),))
            w.call_method('OnConflictConfirmClicked',())
            check('Conflict confirmation reassigns key and clears former action',bound('IA_Jump')=='W' and bound('Move_Forward','W')=='None',bound('Move_Forward','W'))
            w.call_method('OnQuestKeySelected',(unreal.InputChord(key=key('K')),))
            check('Linked quest actions stay synchronized',bound('IA_CapsLock')=='K' and bound('IA_QuestAsk')=='K')
            w.call_method('OnBackClicked',())
            advance()
        elif p==7:
            check('Key menu back restores settings',key_page() is None and state['settings'].get_visibility()==unreal.SlateVisibility.VISIBLE and state['lobby'].get_stack_depth()==2)
            state['settings'].call_method('OnBackClicked',())
            advance()
        elif p==8:
            check('Settings back returns to main lobby',settings_page() is None and state['lobby'].get_stack_depth()==1)
            state['main'].call_method('HandleSettingsClicked',())
            advance()
        elif p==9:
            w=settings_page()
            state['settings']=w
            check('Reopen reads persisted controls',abs(w.get_editor_property('slider_mouse_sensitivity').get_value()-1.4)<.001)
            w.open_key_binding_menu()
            advance()
        elif p==10:
            state['lobby'].navigate_back()
            advance()
        elif p==11:
            check('External lobby back cleans up key popup',settings_page() is None and key_page() is None and state['lobby'].get_stack_depth()==1)
            finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset','StartPIE',json.dumps({'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':0}}))
