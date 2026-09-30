# 로비 커스터마이징 연결

실제 로비 진입 클래스는 **WBP_LobbyCustomizeWidget**이며 부모는 **LobbyCustomizeWidgetBase**다.
팀원의 `WBP_CharacterCustomizationWidget`을 복제해 편집·초기화·회전 그래프를 보존했다.
기존 `WBP_CustomizeWidget`은 새 위젯을 상속하는 호환 에셋으로 연결했다.
`WBP_LobbyColorPickerWidget`도 팀원의 색상창을 복제하여 컬러 휠·명도·팔레트·선택/취소를 유지한다.
원본 NPC용 두 위젯은 변경하지 않았다. 로비에서는 소유 Pawn 검색, NPC 커스터마이징 종료, 직접 RemoveFromParent를
호출하지 않는다. `CharacterVisualComponent`는 이 경로에 사용하지 않는다.

## 편집 항목과 WBP 노드

| 항목 | 연결 함수 | UI / 범위 |
|---|---|---|
| 몸 색상 | SetBodyColor / OpenBodyColorPicker | 컬러 버튼, LinearColor RGBA 각각 0~1 |
| 금속성 | SetMetallic | Slider 0~1 |
| 거칠기 B | SetRoughnessB | Slider 0~1 |
| 거칠기 A | SetRoughnessA | Slider 0~1 |
| 데칼 종류 | SetDecalIndex | 썸네일 버튼, 기본 6종의 인덱스 0~5 |
| 데칼 색상 | SetDecalsColor / OpenDecalColorPicker | 컬러 버튼, LinearColor RGBA 각각 0~1 |
| 데칼 가로 반복 | SetTilingX | SpinBox 0.01~20, 기본 1 |
| 데칼 세로 반복 | SetTilingY | SpinBox 0.01~20, 기본 1 |

머티리얼 파라미터 이름은 CustomizationDataAsset의 매핑을 따른다.
거칠기 A는 원본 머티리얼의 `RougndessA` 오타와 `RoughnessA`를 함께 지원한다.
현재 기능은 머티리얼 편집이다. 메시 교체, 의상, 표정 선택은 포함하지 않는다.
네트워크 검증은 배포된 기본 데칼 6종을 기준으로 한다. 데칼 목록을 확장할 때는
`MOU_Server/Shared/CustomizationValidation.h`의 범위와 모든 클라이언트 에셋 목록도 함께 변경한다.

## 디자이너 설정

1. 이번 UI 이식은 에셋 변경이며 기존 C++ 연결을 사용한다. 변경한 네 WBP를 Compile/Save한다.
2. WBP_LobbyCustomizeWidget의 부모는 LobbyCustomizeWidgetBase, WBP_LobbyWidget의
   CustomizeWidgetClass는 WBP_LobbyCustomizeWidget으로 지정되어 있다.
   기존 WBP_CustomizeWidget 참조도 새 화면을 사용한다.
3. 선택 바인딩: `ConfirmButton`, `BackButton`, `ResetButton`은 Button,
   `CustomizationStatusText`는 TextBlock이다. Is Variable을 켠다.
   이 이름을 쓰면 C++가 클릭을 연결하므로 같은 버튼의 BP OnClicked에 중복 연결하지 않는다.
   다른 이름이면 각각 `ConfirmAndSave`, `CancelAndExit`, `ResetToDefault`를 호출한다.
   현재 이식된 화면은 원본 `Btn_Confirm`, `Btn_Cancel`, `Btn_Reset`의 BP 그래프를 사용한다.
   같은 버튼에 C++ 자동 바인딩을 추가하지 않는다.
4. Class Defaults의 `ColorPickerWidgetClass`는 `WBP_LobbyColorPickerWidget`이다.
   `EditingDataAsset`은 프리뷰 컴포넌트와 같은 데이터 에셋을 사용한다.
   미지정이면 기본 6종 데칼/기본 프리셋 에셋을 런타임에 만든다.
5. `OnCustomizationDataInitialized(InitialData)` → Break CharacterCustomizationData로
   슬라이더, SpinBox, 색상칩과 선택 데칼을 초기화한다. 초기화 중 BP 이벤트가 다시
   setter를 호출하지 않도록 `bRefreshingControls` 같은 플래그로 가드한다.
   이 이벤트는 최초 진입, 기본값 복원, 프리셋 선택에도 호출된다.
6. 슬라이더 OnValueChanged → 표의 setter. 색상 버튼 → OpenBodyColorPicker /
   OpenDecalColorPicker. `OnCustomizationPreviewChanged(Data)`에서 색상칩과 수치 라벨을 갱신한다.
7. `GetAvailableDecalCount`로 버튼 수를 구하고 `GetDecalTexture(Index)`로 썸네일을 채운다.
   각 버튼 클릭 → SetDecalIndex(Index). 프리셋은 GetAvailablePresetCount / ApplyPreset(Index).
8. 상태 표시는 CustomizationStatusText 또는 OnCustomizationStatus를 사용한다.
   확정 요청 중에는 페이지 입력을 잠그고, 서버 거절/연결 끊김/10초 무응답이면 다시 활성화한다.

## 슬롯의 실제 메쉬 연결

RenderTarget의 Image tint를 바꾸는 방식이 아니라 프리뷰 액터의 메쉬 머티리얼을 바꾼다.
기존 `BP_LobbyCharacterPreview`, `RT_LobbySlot0~3`, UI 머티리얼 구성을 사용할 수 있다.

1. MainLobby 레벨에 기존처럼 네 프리뷰 액터가 있고 각각 `PreviewSlotIndex`가 0~3이며
   각자 다른 RenderTarget을 쓰는지 확인한다. C++ SetMember가 이 인덱스로 액터를 찾는다.
   현재 MainLobby 에셋을 에디터 스크립트로 읽어 0→RT_LobbySlot0, 1→RT_LobbySlot1,
   2→RT_LobbySlot2, 3→RT_LobbySlot3 설정을 확인했다.
2. C++가 프리뷰 액터의 SkeletalMesh를 찾아 CharacterCustomizationComponent를 등록하고
   Member.Customization을 적용한다. 기존 WBP_PlayerSlotWidget이 PreviewActor 참조를 이미
   갖고 있다면 `SetPreviewActor(PreviewActor)`를 한 번 호출해 명시적으로 연결할 수도 있다.
3. 이후 서버 멤버 목록이 바뀌면 C++ SetMember가 `Member.Customization`을 자동 적용한다.
   외형만 바뀌어도 OnSlotChanged를 호출한다. 빈 슬롯의 등록된 프리뷰 액터는 숨기고,
   새 멤버가 들어오면 다시 표시하면서 새 멤버 외형을 적용한다.
4. SceneCapture가 Capture Every Frame=false라면 `OnSlotChanged` 뒤 CaptureScene을 호출한다.
   메쉬/머티리얼 자체를 교체했다면 ReinitializeAndApply 후 Member.Customization을 ApplyPreview한다.

커스터마이징 페이지는 서버 멤버 목록에서 로그인한 본인의 `SlotIndex`를 찾는다.
그 인덱스의 `BP_LobbyCharacterPreview`를 원본으로 별도의 메시·머티리얼·촬영 컴포넌트·RenderTarget을 생성한다.
`M_UI_LobbyCharacter` 동적 머티리얼의 텍스처 파라미터 **`PortraitRT`**에 선택한
슬롯의 RT를 넣어 `PreviewImage`에 표시한다. 머티리얼의 기본값은 RT_LobbySlot0이므로
파라미터 이름을 잘못 지정하면 교체가 조용히 실패해 항상 호스트가 보인다. 본인 슬롯 정보를 받기
전에는 PreviewImage를 숨기고 `OnRoomMembersChanged` 후 다시 연결한다.
디자이너에 **Image를 `PreviewImage` 이름으로 추가**하면 원하는 위치/크기에 표시한다.
없으면 Canvas 루트의 왼쪽에 Image가 자동 추가된다. 색상 피커의 OnColorConfirmed도
SetBodyColor/SetDecalsColor에 연결되므로 확인을 누른 최종값이 CurrentData에 남는다.
확정 전 편집은 **편집창 전용 메시/RT**에만 표시된다. 대기실 슬롯 원본도 변경하지 않는다.
`ApplyLobbyPreview`에는 서버 RPC가 없다. 확정 후 서버로 전송된 멤버 스냅샷을
다른 클라이언트가 받아 각자의 해당 슬롯 메쉬에 적용한다.
회전 입력은 RotateCharacter(마우스 DeltaX)에 연결한다.

## 데이터 흐름

```text
슬라이더 / 컬러 피커
  → CharacterCustomizationWidget의 CurrentData
  → LobbyCustomizeWidgetBase.UpdatePreview
  → 창 전용 LobbyCustomizationComponent.ApplyLobbyPreview (분리된 메시 / RT)

확정
  → UServerSubsystem.SubmitCustomization
  → TCP RoomCustomizationReq (RoomId, RequestId, 외형값)
  → 서버가 로그인 세션 UserId / 방 / 대기 상태 / 값 범위 검사
  → 해당 멤버 외형 저장 + 요청자 ACK + 전체 RoomMemberList 전송
  → ACK: GameInstance 서브시스템 메모리 + 기존 SaveGame에 저장
  → RoomMemberList: SlotIndex별 SetMember → 슬롯 프리뷰 메쉬 적용

게임 시작 / 맵 이동
  → GameInstance의 승인된 로컬 외형 유지
  → MainCharacter 컴포넌트 BeginPlay + PossessedBy / PawnClientRestart
  → 로컬 플레이어의 CharacterCustomizationComponent.ApplyLocalCustomization
  → 기존 ServerSetCustomizationData RPC
  → 서버 CustomizationData 복제 / OnRep → 다른 플레이어에게도 동일 외형
```

로비에서는 게임 Pawn이 없어도 편집/저장할 수 있다. 게임맵에서 소유권이 생긴 후에도
다시 적용하므로 클라이언트 BeginPlay가 possession보다 먼저 실행되는 경우를 처리한다.
프리뷰 Actor는 복제하지 않으며 ServerSetCustomizationData를 직접 호출하지 않는다.
로비는 독립 TCP 서버가 RoomMemberList를 방 전원에게 배포한다. 게임맵에서는 기존
CharacterCustomizationComponent의 Unreal 복제 경로가 사용된다. 별도의 UE Multicast RPC는
로비 외형 공유에 필요하지 않다.

취소/뒤로가기/Escape는 편집용 프리뷰를 폐기한 뒤 기존 로비 스택을 Pop한다.
확정 대기 중에는 일반 뒤로가기를 막는다. 방 종료나 맵 이동으로 페이지가 제거될 때에는
컬러 피커를 닫고 프리뷰를 승인된 로컬 값으로 복구한다.
디스크 저장 실패 시에도 현재 게임의 승인값은 유지되며 페이지에 경고를 표시한다.

로그인 후 저장 파일은 기존 C++의 `MOU_Customization_User_{UserId}`, UserIndex=0을 사용한다.
로그인 전에는 `MOU_CustomizationSaveSlot`을 사용한다. 각 창의 현재 플레이 외형은 GameInstance별 메모리에 보관한다.
테스트 서버도 같은 UserId 저장 경로를 사용하므로 자동 검증은 별도 사용자 번호 및 저장 파일 백업을 사용해야 한다.
EOS 백엔드는 기존 프로젝트에서 미구현 상태이며 이번 외형 전송도 CustomSocket 기준이다.

## 배포와 확인

- 프로토콜은 **v14**, RoomMemberInfo는 **99바이트**다. Server.exe와 UE 클라이언트를 함께 빌드/교체한다.
- RoomCustomizationTest: 자기 외형 변경, 다른 멤버 보호, NaN/범위/이전 방 요청 거절,
  늦은 참가자의 스냅샷, 빈 좌석 재사용 시 외형 초기화, 게임 시작 후 수정 거절.
- 에디터 확인: 두 클라이언트 입장 → 슬롯 1 참여자가 몸 색/데칼 변경 → 확정 전에는
  편집창 전용 메시만 변화하고 양쪽 대기실 슬롯은 그대로인지 확인
  → 확정 후 양쪽 화면의 슬롯 1이 변화 → 게임 시작 후 양쪽 Pawn 외형 확인.
- 취소, Escape, 페이지 재진입, 접속 종료, 다른 슬롯 재사용, 게임맵 재스폰도 확인한다.
- 새 화면에는 로비의 WaitingPanel, CharacterBackgroundBlock, T_Lobby_09 버튼을 재사용한다.
  1400×880 설계 캔버스를 ScaleBox로 축소하며, 반복 슬라이더는 서버가 허용하는 0.01~20 범위를 지원한다.
  색상창의 선택은 편집값만 바꾸고, 바깥 화면의 확인 버튼이 서버 전송을 담당한다.

구현 검증: UE 5.8 UHT/C++ 컴파일 및 별도 출력 경로 DLL 링크 성공,
Server/TestClient Release 빌드 성공, CTest의 RoomSlotsTest / RoomCustomizationTest /
SessionAuthTest 통과. 실행 중인 에디터의 DLL은 교체하지 않았다.

2026-09-30 UI 이식 검증: 변경한 4개 WBP 컴파일 성공, 원본 두 위젯의 그래프 목록 보존 확인.
별도 서버와 두 PIE 클라이언트에서 실제 로비 진입 → 참여자 슬롯 1 편집 → 색상창 취소/선택
→ 확인 전 서버 외형 불변 → 원본 확인 버튼 그래프를 통한 서버 승인 → 양쪽 참여자 외형 갱신 및
방장 외형 유지 → 재진입 복원 → 취소 시 서버 상태 불변을 확인했다.
`output/customization-ui/asset-validation.json`, `runtime-validation.json`과 실제 실행 PNG에 결과를 남겼다.
RoomCustomizationTest 및 RoomSlotsTest도 통과했다. 이번 작업에서는 C++를 수정하지 않았다.
