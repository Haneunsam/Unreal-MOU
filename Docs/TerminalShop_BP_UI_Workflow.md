# 터미널 상점 BP UI 작업 기록

2026-09-18 대화 내용을 작업 순서와 현재 상태 중심으로 정리한 문서다.

## 목표 화면과 역할 분담

- C++ `UTerminalShopWidget`은 상점 배경만 그린다. 배경 불투명도는 `0.8`이며 화면 전체를 채운다. BP 콘텐츠는 기존 1920×1080 기준 비율을 유지한다.
- 상품 목록, 상품 정보, 장바구니, 버튼, 패널 테두리는 `WBP_TerminalShop`에서 만든다.
- 왼쪽 상품 목록은 `ScrollBox`로 표시한다. 오른쪽 상품 정보 영역과 장바구니 영역도 각각 별도 스크롤 영역으로 만들 계획이다.
- 상품 카드 이미지는 `DT_Item`의 `ItemIcon`을 사용한다. 상품 선택 후 오른쪽 상세 이미지에도 같은 값을 표시한다.
- `장바구니 추가` 버튼을 누르면 선택한 상품과 수량을 장바구니에 넣고 합계를 갱신할 계획이다.
- 실제 결제·골드 차감·아이템 지급은 아직 연결하지 않았다.

참고 시안: `Docs/TerminalShop_UI_Concept_v2.png`. 준비된 이미지와 폰트는 `Docs/TerminalShop_UI_Assets/` 및 `/Game/04_JJO/TerminalShop/UI`에 있다.

## 현재 구현 상태

1. `/Game/04_JJO/TerminalShop/BP_TerminalShop`의 **Shop Widget Class**는 `WBP_TerminalShop`으로 지정했다. `WBP_TerminalShop`의 부모 클래스는 `Terminal Shop Widget`이다.
2. C++의 고정 포션·테이저 카드, 상품 정보, 모든 박스·테두리 이미지를 제거했다. `TeamProject_MOU/Source/TeamProject_MOU/Private/Item/TerminalShopWidget.cpp`에는 전체 화면 배경과 BP 콘텐츠를 겹쳐 표시하는 코드만 남았다.
3. `WBP_TerminalShop`의 `SB_Items`는 `DT_Item`의 행을 읽어 상품 카드를 생성한다. PIE에서 행 개수 `13`이 출력됐고, 상품 이름·아이콘·가격이 여러 카드로 표시되는 것을 확인했다.
4. `WBP_ShopItemRow`에는 아이콘, 이름, 가격, 설명 자리와 클릭 버튼이 있다. `SetItemData(Item Spawn Data)` 함수가 `ItemIcon → Set Brush from Texture`, `ItemName → Set Text`, `Price → Format Text("{Amount} G") → Set Text`를 연결한다.
5. `WBP_ShopItemRow`에 `RowName`(Name) 변수와 `OnProductSelected` 이벤트 디스패처를 만들고, 카드 버튼 `OnClicked`에서 `RowName`을 보내도록 연결했다는 사용자 확인이 있었다.
6. `DT_Item`의 `FItemSpawnRow`에는 `ItemClass`, `Category`, `ItemName`, `ItemIcon`, `Price` 등이 있지만 **상품 설명 전용 필드는 없다**. 현재 카드의 설명 자리에는 임시 `Text Block` 문구가 남아 있다.

## 상품 목록 BP 연결

`WBP_TerminalShop`의 `Event Construct` 흐름:

```text
Event Construct
→ SB_Items: Clear Children
→ Get Data Table Row Names (DT_Item)
→ For Each Loop
   → Get Data Table Row (DT_Item, Array Element)
   → Row Found: Create Widget (WBP_ShopItemRow)
   → SetItemData (Out Row)
   → SB_Items: Add Child (생성한 위젯)
```

상품 카드 `WBP_ShopItemRow`에는 다음을 설정한다.

- `RowName`: **Name** 타입, **Instance Editable** 및 **Expose on Spawn** 활성화.
- `OnProductSelected`: **Event Dispatcher**, 입력 `SelectedRowName`의 타입은 **Name**.
- 카드 전체 버튼 `OnClicked → Call OnProductSelected`, `SelectedRowName ← Get RowName`.

## 바로 다음 단계: 부모에서 카드 클릭 받기

이 단계는 안내했지만 완료 여부는 아직 확인되지 않았다.

1. `WBP_TerminalShop`의 `Create Widget (WBP_ShopItemRow)`에 나타나는 `RowName` 입력을 `For Each Loop`의 `Array Element`에 연결한다. 새 핀이 보이지 않으면 행 위젯을 Compile한 뒤 `Create Widget` 노드를 Refresh한다.
2. `Create Widget`의 **Return Value**에서 `Assign OnProductSelected`를 만든다.
3. 흰색 실행 흐름을 `Create Widget → SetItemData → Assign OnProductSelected → Add Child` 순서로 잇는다.
4. 생성된 이벤트의 `SelectedRowName`을 임시 `Print String`에 연결한다. PIE에서 카드를 누를 때 해당 DT 행 이름이 출력되면 클릭 연결이 성공한 것이다.
5. 출력 확인 후 `Print String` 대신 선택한 행을 다시 읽어 오른쪽 상품 정보 위젯을 갱신한다.

## 이후 작업

- 오른쪽 상세 정보에서 선택 행의 `ItemIcon`, `ItemName`, `Price`를 표시하고 수량 ±를 연결한다.
- 장바구니 항목은 `RowName + Quantity`로 관리한다. 같은 상품을 다시 담으면 수량을 합치고, `WBP_CartRow`를 장바구니 ScrollBox에 반복 생성한다.
- 장바구니 항목 삭제와 `Price × Quantity` 합계를 구현한다.
- 상품 설명이 필요하면 BP 전용 상품 정보 데이터에 설명 필드를 마련한다.
- 결제는 UI 표시와 분리해 서버에서 가격·보유 골드·상품을 검증한 뒤 연결한다.

## 주의사항

- `Event Construct`의 상품 카드 생성 결과는 디자이너가 아니라 PIE에서 확인한다.
- 상품 13개가 읽히는데 목록이 작거나 엉뚱한 위치에 보이면 각 카드가 아니라 `SB_Items`의 위치·크기·Visibility를 조정한다.
- 상품 목록과 장바구니는 서로 다른 ScrollBox를 사용한다.
- C++를 다시 빌드한 뒤 이미 열려 있던 언리얼 에디터에는 변경이 즉시 반영되지 않을 수 있으므로 에디터를 다시 열어 확인한다.
