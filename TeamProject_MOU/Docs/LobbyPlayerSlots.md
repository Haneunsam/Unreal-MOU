# 방 대기실 플레이어 슬롯

4개의 슬롯을 2열로 생성하고 재사용한다. 서버가 보내는 `SlotIndex`(0~3)에 배치하며 중간 인원이 나가도 다른 사람의 자리는 이동하지 않는다. 새 참가자는 가장 낮은 빈 번호에 배정된다.

## 디자이너 설정

1. `RoomPlayerSlotWidgetBase`를 부모로 `WBP_RoomPlayerSlot`을 만든다.
2. 빈 자리 패널 `EmptyPanel`, 참가자 패널 `OccupiedPanel`을 배치한다.
3. 선택 바인딩 이름: `NicknameText`는 TextBlock, `HostImage`와 `ReadyImage`는 Image, `SelfHighlight`는 자기 자신을 표시할 위젯이다. 각 위젯을 변수로 노출한다. `HostImage`는 방장에게만 보이고, `ReadyImage`는 방장이 아닌 참가자가 준비 완료했을 때만 보인다. 상태 이미지의 Brush는 WBP에서 지정한다.
4. 캐릭터 이미지와 배경은 슬롯 WBP에서 자유롭게 구성한다. `OnSlotChanged` 이벤트에서 `Member`, `bOccupied`, `bIsSelf`로 추가 외형을 갱신할 수 있다. 현재 캐릭터 외형 데이터/3D 미리보기는 구현 범위에 포함하지 않는다.
5. `RoomLobbyWidgetBase`를 부모로 한 방 대기실 WBP에 빈 UniformGridPanel을 만들고 이름을 `PlayerSlotGrid`로 지정한다. 슬롯 자식은 C++에서 추가하므로 디자이너에서 중복 배치하지 않는다.
6. 해당 WBP의 Class Defaults에서 `PlayerSlotWidgetClass`를 `WBP_RoomPlayerSlot`으로, `SlotColumns`를 2로 지정한다. 로비 루트의 방 대기실 페이지 클래스로 이 WBP를 연결한다.

WBP가 없으면 C++ 기본 카드 UI를 사용한다. 기존 `MemberListBox`만 있는 방 대기실 WBP도 내부에 그리드를 생성하는 호환 경로를 사용한다. 방 생성 화면과 방 대기실 화면은 별개 페이지이므로 준비/시작 버튼은 방 대기실에서만 디자인한다.

## 데이터와 검증

서버 응답 → 기존 FlowCoordinator/방 대기실 갱신 경로 → SlotIndex별 SetMember/ClearMember. 개별 슬롯은 서버 델리게이트를 구독하지 않는다.

프로토콜은 v12이며 RoomMemberInfo는 43바이트다. 서버와 클라이언트를 함께 다시 빌드하여 교체해야 한다.

`RoomSlotsTest`는 중간 퇴장 후 자리 유지, 빈 자리 재사용, 중복 입장, 정원 제한, 준비 변경 및 방장 퇴장을 검증한다. 에디터에서는 별도로 두 클라이언트 이상으로 입장/퇴장/준비 및 WBP 레이아웃을 확인한다.
