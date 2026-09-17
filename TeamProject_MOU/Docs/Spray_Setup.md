# 스프레이 설정

## 동작

`ASprayItem : AConsumableItemBase`. 기존 UseAction을 누르면 분사하고 Completed/Canceled에서 멈춘다. 서버에서 약 0.03초 간격으로 카메라 전방 Visibility 트레이스를 수행한다.

첫 히트에 원을 찍고, 마지막 원의 중심에서 DecalRadius를 초과했을 때 다음 원을 겹쳐 찍는다. 다른 컴포넌트/본 또는 법선 방향이 달라진 면은 즉시 찍는다. 이동하는 물체는 해당 표면의 로컬 좌표로 비교한다. 같은 곳에서 다시 클릭해도 거리 기준은 유지된다. 기존 원을 삭제하거나 이동시키지 않는다.

ConsumeUseCount를 사용하며 기본 MaxUseCount=60, SecondsPerUse=1이므로 총 약 60초 분사한다. 공중 분사도 소비한다. 짧게 반복 클릭해도 누적 분사 시간이 유지된다. 소진 시 손을 비우고 아이템을 파괴한다.

## 에디터 연결

1. 에디터를 재시작한 뒤 SprayItem을 부모로 `BP_SprayItem`을 생성한다. MeshComponent에 스프레이 메시를 지정한다.
2. `M_SprayCircle` 머티리얼을 만든다. Material Domain을 Deferred Decal로 설정한다. 원형 마스크는 TextureCoordinate와 (0.5, 0.5)의 Distance를 구한 뒤 `1 - SmoothStep(0.45, 0.5, Distance)`를 Opacity에 연결한다. Vector Parameter를 Base Color에 연결한다.
3. BP의 Decal Material에 해당 머티리얼 또는 인스턴스를 지정한다. Decal Radius=12cm, Spray Range=300cm, Projection Depth=2cm로 시작한다.
4. BP에 분사용 Niagara 컴포넌트를 추가하고 노즐 위치로 이동한다. Auto Activate를 끈다. `Event OnSprayStateChanged`의 bActive로 Branch하여 true에는 Activate, false에는 Deactivate를 연결한다. 반복 사운드도 같은 이벤트에 연결한다. 이 BP 이벤트는 선언만 제공하며 프로젝트의 실제 이펙트 에셋은 별도 지정해야 한다.
5. 칠할 메시의 Receives Decals를 켜고 Visibility를 Block으로 설정한다. UseAction은 좌클릭에 연결된 Boolean 액션이며 일반 누름 입력으로 설정한다.
6. BP와 머티리얼을 컴파일/저장하고 레벨에 배치한다.

## 확인

- 한 지점을 누르고 있으면 데칼이 계속 중복 생성되지 않는다.
- 조준점을 12cm 초과 이동하면 다음 원이 겹쳐 생성된다.
- 벽에서 바닥으로 이동하거나 다른 아이템을 가리키면 새 원이 생성된다.
- 움직이는 아이템의 데칼이 함께 이동한다.
- 버튼 해제, 수납, 드롭, 던지기, 행동 불가, 소진 시 분사가 멈춘다.
- 리슨 서버와 클라이언트에서 입력/중지와 데칼을 각각 확인한다.

데칼은 기본 120초 후 사라지고 스프레이당 각 클라이언트에 최대 256개를 유지한다. 늦게 접속하거나 당시 스프레이가 네트워크 관련성 범위 밖에 있던 클라이언트에 과거 데칼을 복구하는 기능은 포함하지 않는다. 스프레이별 Sort Order 증가로 같은 스프레이의 새 원이 위에 그려진다. 서로 다른 스프레이 사이의 전역 그리기 순서는 보장하지 않는다. 빠르게 조준점을 이동하면 샘플 간격 때문에 원 사이에 간격이 생길 수 있다.

이 문서의 BP/머티리얼 생성과 플레이 검사는 직접 에디터에서 수행해야 한다.
