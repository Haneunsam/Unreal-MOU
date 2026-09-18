# 터미널 상점 UI 원본 자산

`Icons/`의 PNG는 배경 투명도를 포함한다. 포션, 테이저, 골드, 로봇은 시안을 참고해 각각 다시 생성한 이미지이며, `quantity_minus.png`와 `quantity_plus.png`는 수량 버튼용 이미지다. 버튼의 숫자는 이미지에 포함하지 않고 UI 텍스트로 표시한다.

`Panels/`에는 글자·아이콘이 없는 재사용 배경이 있다. `shop_background.png`는 화면 전체 배경, `panel_main.png`는 좌우 큰 영역, `panel_header.png`는 영역 제목, `item_card_selected.png`와 `item_card_default.png`는 상품 행, `panel_detail.png`는 상품 상세, `panel_order.png`는 주문 확인, `gold_badge.png`는 상단 팀 골드, `quantity_value.png`는 수량 숫자 자리, `button_primary.png`와 `button_secondary.png`는 결제·취소 버튼, `divider.png`는 구분선에 사용한다. 모든 `Panels/` 파일은 전체 배경만 제외하고 투명 바깥 영역을 가진다. 텍스트와 아이콘은 위젯에서 별도로 올린다.

언리얼 UMG에서 크기를 크게 바꿀 `panel_main.png` 등 테두리 이미지는 Brush의 `Draw As: Box`와 약 0.06~0.10의 Margin으로 사용하면 모서리 변형이 줄어든다. 최종 크기가 정해진 카드·버튼은 원본 비율에 맞춰 배치해도 된다.

`Fonts/NotoSansKR-VF.ttf`는 시안의 한글 글꼴에 가까운 대체 폰트다. 생성된 시안 이미지에서 정확한 원본 폰트 파일을 추출할 수 없으므로 동일한 폰트라고 보장할 수 없다. 제목에는 굵은 두께, 설명에는 보통 두께를 권장한다. 폰트 고지와 사용 조건은 `Fonts/OFL.txt`에 있다.

이 파일들은 Unreal 프로젝트로 가져올 수 있는 원본 PNG/TTF다. 아직 `.uasset`으로 임포트하거나 위젯에 연결한 상태는 아니다.
