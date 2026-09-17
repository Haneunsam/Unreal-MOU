# 창고 배달 확정 목록 UI

## 데이터와 서버 처리

- `WarehouseDataSubsystem.GetPendingDeliveryDataCopy().SelectedItems`가 공유 확정 목록입니다.
- 위젯 내부 선택 배열은 사용하지 않습니다. 추가 버튼마다 `ServerAddWarehouseDeliveryItem(ItemClass, Quantity)`를 호출합니다.
- 추가 성공 순간 창고 재고가 차감되고 확정 목록이 누적됩니다. 저장/완료 버튼은 UI를 닫는 역할만 맡습니다.
- 기존 `ServerSaveWarehouseDelivery`는 호환용으로 남아 있지만 이 UI의 저장 버튼에서 호출하면 안 됩니다. 이미 추가된 목록을 다시 전달하면 중복 추가됩니다.
- 창고 재고와 확정 목록은 GameState의 동일 스냅샷으로 모든 클라이언트에 전달됩니다.
- `ServerRemoveWarehouseDeliveryItem(ItemClass, Quantity)`는 해당 수량의 개별 저장 상태를 확정 목록에서 창고로 반환합니다. 수량 부족, 0/음수, 오버플로, 개별 상태 부족은 변경 없이 실패합니다.
- 공유 목록이므로 다른 플레이어가 확정한 아이템도 제거할 수 있습니다. 개인별 소유권은 없습니다.
- 추가/제거는 현재 서버 맵이 GameMode BP의 `Run > Travel > Lobby Map`에 지정한 로비이고, 레벨 타이머가 활성/만료 상태가 아니며, seamless travel 중이 아닐 때 허용됩니다. 창고 컴포넌트는 필요 없습니다. Lobby Map이 비어 있으면 거절합니다. PIE 접두사는 제거하고 비교합니다. 이 검사는 서버용이며 클라이언트에서 `CanEditPendingDelivery`를 호출하면 false입니다. 프로젝트의 추가 출발 대기 규칙은 별도로 연결해야 합니다.

## WBP_DeliverySelect 연결

1. 확정 목록용 ScrollBox/VerticalBox를 별도로 추가합니다. 창고 재고 목록과 공유하지 않습니다.
2. `RefreshConfirmedDeliveryUI` 함수를 만듭니다.
   - 확정 목록 박스의 `Clear Children`.
   - `Get Warehouse Data Subsystem` → `Get Pending Delivery Data Copy` → `Break DeliveryData` → `SelectedItems` → ForEach.
   - 각 요소의 `ItemClass`, `Quantity`를 행 위젯에 전달합니다. 이름은 ItemClass의 Class Defaults에서 `ItemName`을 읽습니다.
   - 생성 위젯의 Owning Player는 `Get Owning Player`를 지정합니다.
   - 목록이 비었으면 아무 행도 생성하지 않습니다.
3. Construct에서 Subsystem의 `OnPendingDeliveryChanged`에 갱신 함수를 바인딩하고 즉시 한 번 갱신합니다. Destruct에서는 자신의 이벤트만 Unbind합니다.
4. 창고 목록은 기존 `OnStoredItemsChanged` 이벤트로 갱신합니다.
5. 확정 행의 제거 버튼은 `Get Owning Player` → 프로젝트 PlayerController로 Cast → `Server Remove Warehouse Delivery Item`을 호출합니다. 행의 ItemClass와 제거 수량(1 또는 전량)을 전달합니다.
6. `OnWarehouseDeliveryRemoveCompleted(bool)`는 요청한 Controller에서 결과를 알립니다. 실패 안내/버튼 대기 해제에 사용합니다. 실제 목록 갱신은 `OnPendingDeliveryChanged`를 사용합니다. 결과 RPC와 스냅샷 수신 순서는 보장하지 않습니다.
7. 위젯 생성은 로컬 Controller에서만 합니다. 서버 RPC를 호출한 직후 UI 배열만 임의로 지우지 않습니다.

## 즉시 추가 방식으로 기존 BP 전환

- `WBP_StoredItem` 추가 버튼에서 로컬 SelectedQuantity 증가 대신 소유 Controller의 `ServerAddWarehouseDeliveryItem`을 호출합니다. ItemClass는 해당 행, Quantity는 이번에 추가할 수량(기본 1)입니다. 누적 수량을 보내지 않습니다.
- `OnWarehouseDeliveryAddCompleted`는 실패 안내에 사용합니다. 목록 갱신은 `OnPendingDeliveryChanged`, 창고 갱신은 `OnStoredItemsChanged`로 처리합니다.
- 기존 RequestedItems/Selection/DraftItems 생성 및 로컬 선택 표시 경로를 이 UI에서 제거합니다. SelectedQuantity가 표시용으로 필요하면 서버 목록을 읽어 적용하며 직접 증감하지 않습니다.
- 저장 버튼의 `ServerSaveWarehouseDelivery` 연결을 제거하고 완료/닫기로 변경합니다. 다른 이동·닫기 처리는 유지합니다.
- UI를 닫았다 다시 열면 같은 서버 확정 목록을 읽습니다. 닫기 시 목록을 Clear하지 않습니다.
- 창고 재고 목록을 재생성하더라도 선택 데이터는 서버 목록에 유지됩니다.

## 확인 순서

- A 추가 버튼 두 번 → 저장 버튼 없이 A 2개 표시 및 창고 수량 2개 감소 → UI 닫기/열기 → 동일 목록.
- 완료 버튼 클릭 → 창고/확정 수량 추가 변경 없음.
- A 1개 반환 → 확정 A 1개, 창고 수량 1개 증가.
- 나머지 전량 반환 → 확정 행 제거, 원래 창고 수량 복구.
- 호스트와 클라이언트에서 서로 저장/반환 → 양쪽 창고 및 확정 목록 일치.
- 같은 마지막 아이템에 동시 제거 요청 → 하나만 성공, 재고 중복 반환 없음.
- 잘못된 수량 또는 존재하지 않는 클래스 → 실패, 재고/확정 목록 유지.
- 내구도·추가 상태가 다른 같은 클래스 아이템 → 반환 후 재선택/스폰 시 상태 보존.
- 진행 중 레벨에서 제거 요청 → 실패.
- 늦게 접속한 클라이언트 → 현재 확정 목록 수신.

구조체와 RPC가 변경되었으므로 에디터를 닫고 전체 C++ 빌드한 후 BP를 컴파일합니다.
