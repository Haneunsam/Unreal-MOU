# 치유 스패너 근접무기 작업 정리

작성일: 2026-09-14  
프로젝트: TeamProject_MOU / Unreal Engine 5.8  
대상 플레이어: BP_JJO_EmoPlayer  
대상 무기 BP: Spener

## 1. 구현 내용

프로젝트의 무기 베이스인 `AWeaponItemBase`를 상속한 `AHealingMeleeWeapon`을 추가했다. 스패너를 휘두르면 서버에서 전방 플레이어 한 명을 판정하여 GAS GameplayEffect로 체력을 회복한다.

- 사용자의 자신, NPC, 사망자는 치유 대상에서 제외한다.
- 전방 구체 스윕으로 후보를 찾고, 거리·방향과 Visibility 트레이스로 가림 여부를 검사한다.
- 유효 후보 중 가까운 대상 한 명에게 한 번만 치유를 시도한다. 첫 대상의 체력이 가득 찬 경우 다음 대상을 찾지는 않는다.
- 체력이 0 이하이거나 최대 체력인 대상은 회복하지 않는다. 최대 체력 제한은 기존 AttributeSet 처리를 따른다.
- 공격 간격 동안 중복 사용을 제한하고, 사용 중 내려놓기 및 인벤토리 교체를 차단한다.
- 명중 여부와 관계없이 실제 휘두르기 시작 시 내구도를 소모한다.
- 휘두르기 몽타주와 치유 연출 이벤트는 멀티캐스트로 전달한다.

### 설정값

| 항목 | 값 | 설명 |
| --- | --- | --- |
| HealAmount | 25 | 1회 회복량 |
| SwingRange | C++ 기본 260cm | 전방 판정 거리, 기존 BP 오버라이드 확인 필요 |
| SwingRadius | 45cm | 구체 스윕 반경 |
| SwingCooldown | C++ 기본 0.7초 / Spener BP 0.8초 | 공격 간격 |
| SwingHitDelay | 0.3초 | 휘두르기 시작 후 치유 판정 시점 |
| DurabilityCostPerSwing | 1 | 휘두르기당 내구도 소모 |
| HandSocketName | SpannerSocket | 장착 소켓 |

타격 시점은 애니메이션 노티파이가 아니라 서버의 시간 검사로 처리한다. 몽타주 길이나 재생 속도를 변경하면 `SwingHitDelay`와 `SwingCooldown`도 함께 조정해야 한다.

## 2. 손 소켓 위치 문제 수정

메시 생성자의 상대 위치를 `(0,0,0)`으로 설정했지만, 집기·재장착 과정에서 바운딩박스 중심 보정이 다시 적용되고 있었다.

이를 해결하기 위해 다음을 추가했다.

- `AItemBase::ShouldCenterOnCarrySocket()` — 기본값은 기존 동작을 유지하는 true.
- 치유 무기는 위 함수를 false로 재정의하여 자동 위치 보정을 생략한다.
- `AItemBase::GetCarrySocketOverride()` — 아이템별 장착 소켓 지정 기능.
- `UCarryingComponent`의 집기와 재장착 경로 모두 무기 지정 소켓을 사용한다.

### 생성한 소켓

| 항목 | 값 |
| --- | --- |
| 스켈레톤 | `/Game/EmoRobot00/Skeleton/SK_EmoRobot00` |
| 부모 본 | `hand_r` |
| 소켓 이름 | `SpannerSocket` |
| 상대 위치 | X=0, Y=0, Z=0 |
| 상대 회전 | Pitch=0, Yaw=0, Roll=0 |

소켓은 메시 전용 소켓이 아니라 **스켈레톤 소켓**으로 저장했다. 사용자가 직접 그립 위치를 잡기로 했으므로 위치와 회전은 0으로 남겼다.

## 3. 한손 애니메이션

`BP_JJO_EmoPlayer`의 실제 메시 `SKM_EmoRobot00`와 스켈레톤을 확인하고, 기존 `OneHand_Front_Sequence`를 기반으로 오른손 스패너 자세와 휘두르기 동작을 작성했다.

에셋 폴더: `/Game/04_JJO/ItemAnim/Spanner`

| 에셋 | 용도 |
| --- | --- |
| AS_Spanner_Hold_R | 오른손으로 스패너를 잡는 대기 자세 |
| AS_Spanner_Swing_R | 준비 → 휘두르기 → 복귀 동작, 30fps·0.8초 |
| AM_Spanner_Swing_R | DefaultSlot에서 재생하는 공격 몽타주 |
| ABP_JJO_SpannerPlayer | 스패너 사용 상태에 따라 한손 자세를 선택하는 전용 AnimBP |

- 오른팔·팔꿈치·손목 및 상체 회전을 키프레임으로 작성했다.
- 오른손 손가락은 도구를 잡는 형태로 회전을 적용했다.
- 동작 종료 시 모든 본이 시작 자세로 돌아오도록 구성했다.
- 대기·휘두르기 에셋은 일반 애니메이션으로 설정하여 원본의 additive 설정이 남지 않도록 했다.
- 몽타주 블렌드 인은 0.08초, 블렌드 아웃은 0.12초다.

### 캐릭터 연결

1. 기존 `/Game/05_JYH/MainPlayer/ABP_MainCharacter`를 복제해 전용 AnimBP를 만들었다.
2. `UMainAnimInstance`에 `bIsHoldingHealingMelee` 상태를 추가했다.
3. 치유 무기를 들 때 한손 자세, 그 외에는 기존 운반 자세를 선택하도록 연결했다.
4. 전용 AnimBP의 운반 상체 블렌드 기준 본을 EmoRobot의 `spine`으로 설정했다.
5. 스패너를 들고 있는 동안 기존 AimOffset이 한손 자세와 공격 동작을 덮어쓰지 않도록 차단했다.
6. `BP_JJO_EmoPlayer`의 Anim Class를 `ABP_JJO_SpannerPlayer`로 지정했다.
7. `/Game/04_JJO/BluePrint/Weapon/Spener`에 공격 몽타주와 소켓 이름을 연결했다.

`OnSwingEffect`, `OnHealEffect`는 추가 사운드·나이아가라 연출을 연결할 수 있는 BP 이벤트다. 이번 작업에서 해당 연출 자체를 새로 제작한 것은 아니다.

## 4. 주요 코드 파일

아래 경로는 프로젝트 폴더 `TeamProject_MOU` 기준이다.

| 파일 | 변경 내용 |
| --- | --- |
| Source/TeamProject_MOU/Public/Item/HealingMeleeWeapon.h | 무기 클래스, 설정값, 소켓·연출 인터페이스 |
| Source/TeamProject_MOU/Private/Item/HealingMeleeWeapon.cpp | 서버 판정, 지연 치유, 내구도, 몽타주 재생 |
| Source/TeamProject_MOU/Public/Base/ItemBase.h | 중심 보정 여부와 소켓 재정의 인터페이스 |
| Source/TeamProject_MOU/Private/Components/CarryingComponent.cpp | 집기·재장착 시 전용 소켓 적용 |
| Source/TeamProject_MOU/Public/Animation/MainAnimInstance.h | 한손 무기 보유 상태 변수 |
| Source/TeamProject_MOU/Private/Animation/MainAnimInstance.cpp | 무기 보유 상태 계산, AimOffset 차단 |

### 작업 스크립트

| 파일 | 용도 |
| --- | --- |
| Scripts/inspect_spanner_animation.py | 원본 플레이어·무기·애니메이션 조사 |
| Scripts/build_spanner_animation.py | 휘두르기 시퀀스와 몽타주 생성, Spener 설정 |
| Scripts/setup_spanner_character.py | 소켓·대기 자세·전용 AnimBP 및 플레이어 연결 |
| Scripts/verify_spanner_animation.py | 저장된 에셋을 다시 로드하여 연결 검증 |

생성·설정 스크립트는 에셋을 수정하므로, 에디터에서 수동으로 동작을 조정한 뒤에는 재실행 시 덮어쓸 수 있는 항목을 먼저 확인해야 한다.

## 5. 검증 결과

- Unreal Engine 5.8의 `TeamProject_MOUEditor Win64 Development` 최종 빌드 성공.
- 전용 AnimBP 컴파일 및 그래프 노드 오류 검사 통과.
- 저장된 에셋을 다시 로드하는 검증 스크립트 PASS.
- 스켈레톤 소켓의 소유자·부모 본·위치·회전 확인.
- Spener의 몽타주·소켓·0.3초 타격 지연 연결 확인.
- BP_JJO_EmoPlayer의 전용 AnimBP 연결 확인.
- 시퀀스와 몽타주의 스켈레톤 일치, DefaultSlot 연결 확인.
- 38개 본 트랙의 시작·종료 자세 일치와 타격 시점 오른팔 회전 변화 확인.

검증 결과 파일: `Saved/spanner_verification.json`  
관련 로그: `Saved/SpannerBuild.log`, `Saved/SpannerSetup.log`, `Saved/SpannerVerify.log`

작업 도중 에디터·디버거가 파일을 사용해 저장 및 링크가 막힌 적이 있었으나, 종료 후 최종 저장과 빌드를 완료했다.

## 6. 남은 확인 및 조정

실제 PIE 화면에서 동작의 자연스러움, 그립 모양과 멀티플레이 치유 동작은 아직 검증하지 않았다. 에셋 연결·수치 검증과 시각적 플레이 검증은 별개다.

1. 스켈레톤 에디터에서 `hand_r → SpannerSocket`을 선택한다.
2. 스패너 손잡이가 손에 맞도록 소켓 위치·회전을 조정하고 저장한다.
3. BP_JJO_EmoPlayer로 Spener를 집었을 때 한손 자세와 소켓 부착을 확인한다.
4. 휘두르기 동작의 관절 각도, 손가락 그립, 복귀 블렌딩을 눈으로 확인한다.
5. 이동 중 공격과 인벤토리 수납·재장착을 확인한다.
6. 2인 PIE에서 회복량·판정 거리·벽 가림·연타 제한 및 클라이언트 동기화를 확인한다.

손잡이 위치는 무기 생성자의 기본 위치보다 **SpannerSocket의 상대 Transform**을 기준으로 조정하면 된다.

### 판정 범위 실시간 조절

`SwingRange` 기본값을 180cm에서 260cm로 늘렸다. `SwingRadius`는 45cm를 유지한다. 두 값과 `Show Swing Range`는 배치된 인스턴스에서도 편집할 수 있다. PIE에서 스패너를 든 뒤 서버/Standalone 월드의 해당 Spener 액터를 선택해 조절한다. 클라이언트에서만 바꾸면 서버 판정에는 반영되지 않는다.

`Show Swing Range`를 켜면 노란 캡슐은 구체 스윕의 후보 탐색 영역, 파란 구는 대상 액터 중심까지의 최대 거리 제한을 나타낸다. 실제 적용은 전방 여부·거리·벽 가림·대상 유효성 검사도 통과해야 한다. 표시는 개발 빌드에서만 사용한다. PIE에서 수정한 값은 종료 후 사라지므로 결정한 값을 BP Class Defaults에 옮겨 저장한다. 기존 BP가 SwingRange를 별도 저장했다면 직접 260으로 변경하거나 기본값으로 재설정한다.

## 7. 모든 무기의 오른손 소켓 공통 적용

- `HandSocketName`과 소켓 선택·중심 보정 재정의를 `AHealingMeleeWeapon`에서 `AWeaponItemBase`로 이동했다.
- 모든 파생 무기는 기본적으로 `hand_r` 하위의 기존 `SpannerSocket`을 사용하며, 집기·인벤토리 재장착 모두 같은 설정을 따른다.
- 무기 BP의 `Weapon|Animation → HandSocketName`에서 소켓을 지정할 수 있다. 대상 캐릭터 스켈레톤에 해당 소켓이 있어야 한다.
- 일반 아이템의 기존 운반 소켓과 중심 보정 동작은 유지한다. 무기별 그립 위치와 PIE 시각 검증은 별도로 필요하다.

### 무기별 장착 위치·회전

`MeshComponent`가 루트이므로 무기 BP의 컴포넌트 Transform에서 상대 위치·회전을 직접 편집할 수 없다. 대신 무기 BP의 Class Defaults에서 `Weapon|Animation`의 `HandLocationOffset`, `HandRotationOffset`을 설정한다. 두 값은 소켓 기준이며 집기·재장착 직후 적용된다. 기본값은 0이고 기존 메시 스케일은 유지한다. 공유 소켓을 수정하지 않고 무기별 그립을 맞출 수 있다.

`HandLocalRotationOffset`은 기본 자세에서 무기 로컬 축으로 추가 회전하는 값이다. 최종 회전은 `기본 Quaternion * 추가 Quaternion`으로 합성한다. 기본값 0은 기존 자세를 유지하며, 기본 Pitch가 ±90도일 때도 추가 회전을 0에서 조절할 수 있다. 추가 회전 자체를 ±90도 Pitch로 설정하면 Euler 입력의 축 중첩이 다시 생길 수 있으므로 작은 각도로 한 축씩 조절한다. 예를 들어 기본 `X=90, Y=-90, Z=90`을 유지한 채 추가 `Y=15`와 `Y=-15`를 비교한다. 화면 방향은 손 소켓 자세에 따라 달라 PIE에서 확인해야 한다.
