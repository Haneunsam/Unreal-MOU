import unreal, time, json, pathlib, traceback, base64
OUT = pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/customization-ui')
state = {'phase':0,'since':time.monotonic(),'start':time.monotonic(),'checks':[]}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
app = unreal.get_default_object(unreal.EditorAppToolset)
slate = unreal.get_default_object(unreal.SlateInspectorToolset)
editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)

# [LCTEST-006] 실제 로비 스택에 붙은 커스터마이징 페이지를 찾는다.
def active_page():
    found=unreal.WidgetLibrary.get_all_widgets_of_class(state['worlds'][1],unreal.LobbyCustomizeWidgetBase,False)
    return next((w for w in found if w.get_parent() is not None),None)

# [LCTEST-007] 메모리 주소를 제외한 외형 값으로 비교한다.
def data_key(d):
    return tuple([d.body_color.r,d.body_color.g,d.body_color.b,d.body_color.a,d.metallic,d.roughness_a,d.roughness_b,d.decal_index,d.decals_color.r,d.decals_color.g,d.decals_color.b,d.decals_color.a,d.tiling_x,d.tiling_y])

# [LCTEST-008] 두 클라이언트의 서버 확정 외형을 비교용으로 수집한다.
def snapshot():
    return [[(m.user_id,m.slot_index,data_key(m.customization)) for m in s.get_room_members()] for s in state['servers']]

# [LCTEST-001] 결과를 기록하고 실패 지점에서 검증을 중단한다.
def check(name, result, detail=''):
    state['checks'].append({'name':name,'pass':bool(result),'detail':str(detail)})
    assert result, name + ': ' + str(detail)

# [LCTEST-002] 비동기 검증의 다음 단계로 전환한다.
def advance():
    state['phase'] += 1
    state['since'] = time.monotonic()
    print('CUSTOM_TEST_PHASE',state['phase'])

# [LCTEST-003] 실제 Slate 화면을 캡처한다.
def capture(name):
    snap = slate.call_method('Snapshot',('',20,False))
    (OUT/(name+'.txt')).write_text(snap,encoding='utf-8')
    pic = slate.call_method('Screenshot',('',))
    if pic.data: (OUT/(name+'.png')).write_bytes(base64.b64decode(pic.data))

# [LCTEST-004] 테스트 세션만 정리하고 검증 결과를 저장한다.
def finish(error=None):
    if error: state['error']=error
    (OUT/'runtime-validation.json').write_text(json.dumps({k:state[k] for k in ['checks','error'] if k in state},ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    app.call_method('StopPIE',())
    for path,original in state.get('save_backups',{}).items():
        if original is None:
            path.unlink(missing_ok=True)
        else:
            path.write_bytes(original)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

# [LCTEST-005] 별도 서버의 두 사용자로 편집 격리와 확인 후 동기화를 검증한다.
def tick(delta):
    try:
        now=time.monotonic()
        if now-state['start']>210: raise RuntimeError('Runtime validation timed out at phase '+str(state['phase']))
        if now-state['since']<2: return
        p=state['phase']
        if p==0:
            worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if len(worlds)<2: return
            state['worlds']=worlds[:2]
            state['servers']=[unreal.ServerSubsystem.get(w) for w in worlds[:2]]
            for s in state['servers']: s.connect_to_chat_server('127.0.0.1',17883)
            advance()
        elif p==1:
            for i,s in enumerate(state['servers']): s.register_account('customtest'+str(i),'Test1234!','UI Test '+str(i))
            advance()
        elif p==2:
            for i,s in enumerate(state['servers']): s.login('customtest'+str(i),'Test1234!',0)
            advance()
        elif p==3:
            if not all(s.get_login_result().user_id>0 for s in state['servers']): return
            ids=[s.get_login_result().user_id for s in state['servers']]
            assert all(i>=900000 for i in ids), '테스트 전용 사용자 번호가 필요합니다. 기존 계정 저장 파일에 접근하지 않습니다.'
            save_dir=pathlib.Path(unreal.Paths.project_saved_dir())/'SaveGames'
            state['save_backups']={}
            for uid in ids:
                path=save_dir/('MOU_Customization_User_'+str(uid)+'.sav')
                state['save_backups'][path]=path.read_bytes() if path.exists() else None
            state['servers'][0].create_room('Customization isolated test','',17777)
            advance()
        elif p==4:
            room=state['servers'][0].get_current_room_id()
            if not room: return
            state['servers'][1].join_room(room,'')
            advance()
        elif p==5:
            if len(state['servers'][1].get_room_members())<2: return
            state['lobbies']=[]
            for world in state['worlds']:
                pc=unreal.GameplayStatics.get_player_controller(world,0)
                cls=unreal.load_class(None,'/Game/02_JSY/MainLobby/WBP_LobbyWidget.WBP_LobbyWidget_C')
                lobby=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,cls,pc))
                lobby.add_to_viewport(100)
                state['lobbies'].append(lobby)
            state['lobbies'][1].open_customize()
            state['widget']=active_page()
            advance()
        elif p==6:
            w=state['widget']
            check('Guest resolves own slot 1',w.get_editor_property('local_slot_index')==1,w.get_editor_property('local_slot_index'))
            check('Preview image uses isolated render target',w.get_editor_property('preview_image').get_visibility()==unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
            state['before']=snapshot()
            state['host_before']=data_key(state['servers'][0].get_room_members()[0].customization)
            state['original']=w.get_current_customization_data()
            for control,index,value in [('Slider_Metallic',6,.72),('Slider_RoughnessA',5,.25),('Slider_RoughnessB',13,.65),('Slider_TilingX',3,2.5),('Slider_TilingY',2,3.5)]:
                if control=='Slider_RoughnessB':
                    w.set_roughness_b(value)
                else:
                    w.call_method('BndEvt__WBP_CharacterCustomizationWidget_'+control+'_K2Node_ComponentBoundEvent_'+str(index)+'_OnFloatValueChangedEvent__DelegateSignature',(value,))
            w.set_decal_index(4)
            picker=w.open_body_color_picker()
            check('Migrated color picker opens',picker is not None)
            state['picker']=picker
            picker.set_rgb(.1,.8,.3)
            advance()
        elif p==7:
            capture('color-picker-runtime')
            check('Color picker edits only current draft',abs(state['widget'].get_current_customization_data().body_color.g-.8)<.001)
            check('No room snapshot changes before confirmation',state['before']==snapshot())
            state['picker'].cancel_color()
            check('Picker cancel restores original body color',state['widget'].get_current_customization_data().body_color==state['original'].body_color)
            state['picker']=state['widget'].open_decal_color_picker()
            state['picker'].set_rgb(.8,.2,.1)
            state['picker'].confirm_color()
            check('Picker confirm retains draft only',state['before']==snapshot())
            advance()
        elif p==8:
            capture('customization-runtime')
            state['widget'].call_method('BndEvt__WBP_CharacterCustomizationWidget_Btn_Confirm_K2Node_ComponentBoundEvent_7_OnButtonClickedEvent__DelegateSignature',())
            advance()
        elif p==9:
            w=state['widget']
            if w.get_editor_property('waiting_for_confirmation'): return
            uid=state['servers'][1].get_login_result().user_id
            for i,s in enumerate(state['servers']):
                members=s.get_room_members()
                own=next(m for m in members if m.user_id==uid)
                check('Server snapshot updated guest on client '+str(i),own.slot_index==1 and own.customization.decal_index==4 and abs(own.customization.metallic-.72)<.001)
                host=next(m for m in members if m.slot_index==0)
                check('Host appearance preserved on client '+str(i),data_key(host.customization)==state['host_before'])
            check('Confirmation closes customization page',active_page() is None)
            state['lobbies'][1].open_customize()
            state['widget']=active_page()
            check('Reopen restores approved appearance',abs(state['widget'].get_current_customization_data().metallic-.72)<.001)
            state['before_cancel']=snapshot()
            state['widget'].set_body_color(unreal.LinearColor(.9,.1,.9,1))
            state['widget'].cancel_and_exit()
            advance()
        elif p==10:
            check('Cancel closes page without server changes',active_page() is None and state['before_cancel']==snapshot())
            capture('lobby-after-confirmation')
            finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset','StartPIE',json.dumps({'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':0}}))

