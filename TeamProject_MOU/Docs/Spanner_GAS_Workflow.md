# 스패너 GAS 공격

## 호출 흐름

`WeaponItemBase::OnUse → 서버 Fire → GiveAbilityAndActivateOnce(GA_SpannerSwing) → WaitDelay → ResolveSwing → 치유 GE`

- 무기가 어빌리티 Spec의 SourceObject가 된다. 캐릭터 ASC에 서버 전용 어빌리티를 일회 부여하며 종료 후 Spec이 제거된다. 별도 BP 어빌리티 부여 설정은 필요 없다.
- 공격은 서버에서 실행한다. 클라이언트 예측은 사용하지 않으므로 네트워크 지연만큼 재생 시작이 늦을 수 있다.
- `PlayMontageAndWait`와 ASC의 몽타주 복제를 사용한다. 기존 `MulticastSwing`은 BP 연출 이벤트만 호출하며 몽타주를 중복 재생하지 않는다.
- 기존 BP `SwingMontage`, `SwingHitDelay`, `SwingCooldown`, `DurabilityCostPerSwing`, `SwingRange`, `SwingRadius`, `HealAmount`를 재사용한다.
- 타격 시점은 `WaitDelay(SwingHitDelay)`다. AnimNotify 연결은 필요 없다. 회복은 서버에서 한 번만 시도한다.
- `UGE_SpannerCooldown`은 지속형 GE로 `Cooldown.Weapon.SpannerSwing` 태그를 부여한다. 같은 캐릭터가 사용하는 스패너들에 공통 쿨다운이 적용된다. 공격 취소 후에도 쿨다운은 유지된다.
- `State.Weapon.SpannerSwing` 태그는 어빌리티 활성 동안 유지된다. 기존 인벤토리와 호환되도록 무기의 `bIsInUse`도 함께 관리한다.
- 내구도는 어빌리티 시작 시 한 번 차감한다. 헛스윙·시작 후 취소는 환불하지 않는다.
- 사망·그로기·기절·붙잡힘 태그 추가, 몽타주 중단, 무기 제거 시 취소한다. 타격 직전에도 장착·소유자·행동 가능 상태를 재검사한다.
- 종료·취소 시 태스크와 사용 상태를 정리한다. 정상 종료 시간은 `max(SwingHitDelay + 0.01, SwingCooldown)`이며 정상 종료로 몽타주를 강제 절단하지 않는다.

## Tick 제거와 디버그

무기의 `Tick`과 절대시간 비교 필드를 제거했다. `Show Swing Range`를 BP에서 켜고 플레이를 시작한 경우에만 0.1초 반복 타이머로 범위를 그린다. PIE에서 Range/Radius를 조정하면 다음 표시부터 반영된다. 디버그를 꺼두고 시작했다면 실행 중 체크만 켜서는 타이머가 시작되지 않는다. 결정한 값은 PIE 종료 후 BP에 저장한다.

## 확인 순서

1. 에디터를 재시작하고 기존 Spener BP를 컴파일한다.
2. 스패너 집기 → 공격 몽타주와 0.3초 치유 타이밍 확인.
3. 연타해 회복·내구도가 중복 적용되지 않는지 확인.
4. 헛스윙, 벽 뒤 대상, 최대 체력 대상, 사망 대상 확인.
5. 타격 전 사망·그로기·기절과 몽타주 중단 시 회복 취소 및 장비 잠금 해제 확인.
6. 2인 PIE에서 호스트와 클라이언트 각각 사용해 몽타주·내구도·회복 동기화 확인.

실제 PIE 및 멀티플레이 검증은 별도로 필요하다.
