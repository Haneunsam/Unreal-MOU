import unreal, time, json, pathlib, traceback, base64

OUT = pathlib.Path(__file__).parent
state = {'phase': 0, 'since': time.monotonic(), 'start': time.monotonic(), 'checks': []}
app = unreal.get_default_object(unreal.EditorAppToolset)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)

# [MICTEST-020] 확인 결과를 저장한다.
def check(name, passed):
    state['checks'].append({'name': name, 'passed': bool(passed)})
    assert passed, name

# [MICTEST-021] 테스트 전용 에디터를 종료한다.
def finish(error=None):
    (OUT / 'results.json').write_text(json.dumps({'checks': state['checks'], 'error': error}, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    app.call_method('StopPIE', ())
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

# [MICTEST-022] 실제 소유 플레이어로 설정 화면을 생성한다.
def create(path):
    cls = unreal.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    widget = unreal.get_default_object(unreal.WidgetLibrary).call_method('Create', (state['world'], cls, state['pc']))
    widget.add_to_viewport(100)
    widget.switch_to_tab(1)
    return widget

# [MICTEST-023] 두 설정의 게이지 교체, 입력 판정 설정과 저장 복원을 검사한다.
def tick(delta):
    try:
        if time.monotonic() - state['start'] > 120:
            raise RuntimeError('PIE timeout')
        if time.monotonic() - state['since'] < 2:
            return
        phase = state['phase']
        if phase in (0, 4):
            worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds: return
            state['world'] = worlds[0]
            state['pc'] = unreal.GameplayStatics.get_player_controller(worlds[0], 0)
            state['voice'] = unreal.VoiceSubsystem.get(worlds[0])
            if phase == 4:
                check('restart restores -60 dB threshold', abs(state['voice'].get_mic_sensitivity() - .001) < .000001)
                finish()
                return
            check('engine silence gate disabled', unreal.SystemLibrary.get_console_variable_float_value('voice.SilenceDetectionThreshold') == -1)
            check('engine noise gate disabled', unreal.SystemLibrary.get_console_variable_float_value('voice.MicNoiseGateThreshold') == -1)
            state['lobby'] = create('/Game/02_JSY/MainLobby/WBP_LobbySettingsWidget')
        elif phase == 1:
            lobby = state['lobby']
            check('lobby uses input meter', isinstance(lobby.get_editor_property('slider_mic_sensitivity'), unreal.MicSensitivitySlider))
            check('lobby has no mic icon preview', lobby.get_editor_property('mic_preview') is None)
            lobby.call_method('OnMicSensitivityChanged', (0.,))
            check('left endpoint applies zero gate', state['voice'].get_mic_sensitivity() == 0.)
            lobby.apply_settings()
            lobby.remove_from_parent()
            state['game'] = create('/Game/05_JYH/MainMenuUI/WBP_SettingsMenuWidget')
        elif phase == 2:
            game = state['game']
            slider = game.get_editor_property('slider_mic_sensitivity')
            check('game uses input meter', isinstance(slider, unreal.MicSensitivitySlider))
            check('game has no mic icon preview', game.get_editor_property('mic_preview') is None)
            check('game restores lobby zero', slider.get_value() == 0.)
            game.call_method('OnMicSensitivityChanged', (.25,))
            check('quarter scale applies -60 dB', abs(state['voice'].get_mic_sensitivity() - .001) < .000001)
            slider.set_value(.25)
            game.apply_settings()
            screenshot = unreal.get_default_object(unreal.SlateInspectorToolset).call_method('Screenshot', ('',))
            if screenshot.data:
                (OUT / 'settings.png').write_bytes(base64.b64decode(screenshot.data))
            app.call_method('StopPIE', ())
        elif phase == 3:
            unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset', 'StartPIE', json.dumps({'options': {'bSimulate': False, 'playMode': 'PlayMode_InEditorFloating', 'warmupSeconds': 0}}))
        state['phase'] += 1
        state['since'] = time.monotonic()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset', 'StartPIE', json.dumps({'options': {'bSimulate': False, 'playMode': 'PlayMode_InEditorFloating', 'warmupSeconds': 0}}))
