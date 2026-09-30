import pathlib, re
task_script=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/runtime.py').read_text(encoding='utf-8-sig')
exec(task_script.split('# [LSTEST-008]')[0].replace('runtime-validation.json','input-validation.json'))

# [LSINPUT-001] 현재 Slate 트리에서 버튼을 찾아 실제 클릭 이벤트를 전달한다.
def click_label(label):
    snap=slate.call_method('Snapshot',('',30,False))
    refs=re.findall(r'button "'+re.escape(label)+r'"[^\n]*\[ref=([^\]]+)\]',snap)
    assert len(refs)==1,(label,refs)
    assert slate.call_method('Click',(refs[0],'left',False,unreal.SlateInspectorToolsetModifierKeys()))

# [LSINPUT-002] 탭 클릭과 포커스, Esc 입력을 실제 Slate 경로로 검증한다.
def tick(delta):
    try:
        now=time.monotonic()
        if now-state['start']>100: raise RuntimeError('input test timeout '+str(state['phase']))
        if now-state['since']<2: return
        p=state['phase']
        if p==0:
            worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds: return
            world=worlds[0]
            state['world']=world
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            cls=unreal.load_class(None,'/Game/02_JSY/MainLobby/WBP_LobbyWidget.WBP_LobbyWidget_C')
            lobby=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,cls,pc))
            lobby.add_to_viewport(100)
            state['lobby']=lobby
            lobby.open_settings()
            advance()
        elif p==1:
            state['settings']=settings_page()
            check('Settings receives keyboard focus',state['settings'].has_keyboard_focus())
            capture('settings-graphics-final')
            click_label('오디오')
            advance()
        elif p==2:
            w=state['settings']
            check('Real audio button changes tab',w.get_editor_property('widget_switcher_tabs').get_active_widget_index()==1)
            check('Selected tab indicator follows click',w.get_editor_property('LobbyTabIndicator1').get_render_opacity()==1.0 and w.get_editor_property('LobbyTabIndicator0').get_render_opacity()==0.0)
            capture('settings-audio-final')
            click_label('조작 / 키 설정')
            advance()
        elif p==3:
            check('Real controls button changes tab',state['settings'].get_editor_property('widget_switcher_tabs').get_active_widget_index()==2)
            capture('settings-controls-final')
            click_label('키 설정 변경')
            advance()
        elif p==4:
            state['keys']=key_page()
            check('Real key settings button opens popup',state['keys'] is not None)
            check('Key popup receives keyboard focus',state['keys'].has_keyboard_focus())
            capture('settings-keybindings-final')
            slate.call_method('PressKey',('Escape',))
            advance()
        elif p==5:
            check('Escape closes only key popup',key_page() is None and settings_page() is not None and state['lobby'].get_stack_depth()==2)
            check('Keyboard focus returns to settings',state['settings'].has_keyboard_focus())
            slate.call_method('PressKey',('Escape',))
            advance()
        elif p==6:
            check('Escape returns from settings to main lobby',settings_page() is None and state['lobby'].get_stack_depth()==1)
            finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset','StartPIE',json.dumps({'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':0}}))
