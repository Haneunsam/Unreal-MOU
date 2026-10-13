# 로비 배경 영상 촬영 준비

## MainLobby 연결 및 검증

2026-10-13: WBP_LobbyBackground를 LobbyBackgroundWidgetBase에 연결하고, MainLobby PIE에서 로그인 UI 뒤로 TrashTown 영상이 표시되는 것을 확인했다. M_UI_LobbyVideo의 VideoTexture 샘플러를 Color로 수정·저장한 뒤 PIE를 다시 시작해 표시되는 것도 확인했다. 패키징 실행은 아직 검증하지 않았다. 패키징에서도 동일한 출력 방식을 사용하도록 NativeConstruct의 MediaTexture 생성 직후 NewStyleOutput=true를 명시하는 한 줄을 사용자 승인 후 추가했다.

추가 영상은 Content/Movies/LobbyBackgrounds에 고유한 이름으로 저장하고, Media 폴더에 해당 영상을 가리키는 File Media Source를 만든다. WBP_LobbyBackground의 Class Defaults → MOU | Lobby Background → Background Videos 배열에 추가한다. 위젯 생성 시 배열에서 한 영상을 무작위 선택해 반복 재생한다. 현재 등록 영상은 Spot01 하나다. Video Material은 M_UI_LobbyVideo를 그대로 사용한다.

## 준비된 에셋

콘텐츠 브라우저의 `/Game/Cinematics/LobbyBackgrounds` 폴더:

- `LS_BG_TrashTown_Spot01`: 60초, 30fps, 위치 고정 및 Yaw +360도 선형 회전.
- `MRP_BG_1080p30_MP4`: 1920×1080, 30fps, PNG 원본과 H.264 MP4 출력 프리셋.
- `MRQ_BG_TrashTown`: TrashTown 맵, 위 시퀀스와 프리셋을 연결한 렌더 큐.

Movie Render Queue 플러그인은 기존 프로젝트에서 이미 활성화되어 있었다. 설치된 FFmpeg의 libx264 인코더 및 에디터에서 ffmpeg.exe 경로 검색을 확인했다. DefaultEngine.ini에 CLI Encoder 설정을 추가했다. 다른 PC에서는 FFmpeg가 PATH에 있어야 하며, 없다면 프로젝트 설정의 Movie Pipeline CLI Encoder에서 실행 파일 경로를 지정한다.

## 촬영 순서

1. `/Game/06_HES/_Level/_Levels/Lv_TrashTown`을 연다.
2. `LS_BG_TrashTown_Spot01`을 열고 Camera Cuts의 카메라 아이콘으로 촬영 화면을 확인한다.
3. 타임라인을 움직여 한 바퀴 전체 구도를 확인한다. 현재 위치는 맵에 저장되어 있던 에디터 뷰포트를 기준으로 잡은 임시 스팟이다.
4. Movie Render Queue 창에서 저장된 `MRQ_BG_TrashTown`을 불러온다. 현재 에디터에는 이미 로드되어 있다.
5. 작업의 맵이 Lv_TrashTown이고 프리셋이 MRP_BG_1080p30_MP4인지 확인한다.
6. `렌더(로컬)`을 누르면 촬영한다. 2026-10-13에 사용자가 확정한 현재 구도를 저장하고 60초 전체를 렌더했다.

결과는 프로젝트의 `Saved/MovieRenders/LobbyBackgrounds` 아래 시퀀스 이름/버전별 폴더에 출력하도록 설정했다. PNG 원본은 자동 삭제하지 않는다. MP4는 렌더 종료 후 FFmpeg가 인코딩한다. 실제 출력 영상과 반복 경계는 첫 렌더 후 확인해야 한다.

## 촬영 위치 변경과 스팟 추가

카메라는 맵에 영구 배치한 액터가 아니라 시퀀스가 생성하는 Spawnable이다. 맵을 저장하지 않아도 촬영 구성을 시퀀스 에셋으로 관리할 수 있다.

- 시퀀서의 `BG_Camera_MoveThisSpot` 아래 Transform을 펼친다.
- Location X/Y/Z를 수정하면 전체 촬영 위치가 바뀐다. 현재 고정 위치 채널에는 키가 없고 기본값을 사용한다. 키를 사용할 때는 프레임 0에 하나만 두고, 중간 프레임에 위치 키를 추가하지 않는다.
- 아래를 내려다보려면 Rotation의 Pitch(Y)를 -10~-20도로 고정한다. Roll(X)은 0도로 유지하고 Yaw(Z)만 회전시킨다. 기존 수평 구도는 보존하고 복제한 Spot02에서 설정하는 것이 좋다.
- Yaw 키는 프레임 0/450/900/1350/1800에 있고, 값은 34.8125/124.8125/214.8125/304.8125/394.8125도다. 시작 방향을 바꿀 때는 다섯 키에 같은 각도를 더한다.
- 재생 구간은 0 이상 1800 미만으로, 마지막 360도 키는 반복 경계 계산용이다. 총 출력은 1800프레임이다.
- 다른 지점은 시퀀스를 복제해 Spot02 등으로 이름을 바꾸고 위치 키를 수정한다. 렌더 큐에 해당 시퀀스를 추가하고 같은 프리셋을 선택한다.
- 다른 맵을 촬영할 때는 해당 맵에서 구도를 잡고, 렌더 큐 작업의 Map도 함께 바꾼다.

렌더 프리셋은 촬영용 Game Overrides 및 시작 워밍업(엔진 60프레임, 렌더 16프레임)을 포함한다. 맵의 날씨·구름·파티클 등은 카메라와 별개로 움직이므로 한 바퀴 회전만으로 영상 전체가 무봉제 반복된다고 보장할 수 없다. 필요하면 영상 편집 단계에서 시작/끝을 연결한다.

## 확인 범위

2026-10-13: 현재 구도를 저장하고 PNG 1,800프레임 렌더 완료. 시작·중간 프레임에서 에디터 아이콘, 선택 외곽선, HUD가 없음을 확인했다. 실제 맵의 장벽·캐릭터·이펙트는 출력에 포함되며 자동으로 제거하지 않는다.

최종 영상: `Content/Movies/LobbyBackgrounds/BG_TrashTown_Spot01.mp4` (H.264, 1920×1080, 30fps, 60초, 무음, 44,201,811바이트). ffprobe 메타데이터와 FFmpeg 전체 디코드 검증 통과.

PNG 원본: `Saved/MovieRenders/LobbyBackgrounds/LS_BG_TrashTown_Spot01/v001`.

첫 렌더에서는 Command Line Encoder의 Additional Command Line Args에 출력용 `-movflags`가 들어가 자동 MP4 인코딩에 실패했다. PNG로부터 아래 명령으로 MP4를 생성했고, 프리셋의 Additional Command Line Args는 비웠다. 출력 옵션은 DefaultEngine.ini의 High 품질 옵션에만 둔다.

```powershell
ffmpeg -n -framerate 30 -start_number 0 -i "Saved/MovieRenders/LobbyBackgrounds/LS_BG_TrashTown_Spot01/v001/LS_BG_TrashTown_Spot01.%04d.png" -frames:v 1800 -an -c:v libx264 -crf 20 -preset medium -pix_fmt yuv420p -movflags +faststart "Content/Movies/LobbyBackgrounds/BG_TrashTown_Spot01.mp4"
```

명령은 TeamProject_MOU 프로젝트 폴더 기준이다. `-n`은 기존 영상 덮어쓰기를 막는다. 다른 촬영에서는 입력 폴더·이름·총 프레임 수와 출력 이름을 바꾼다.

주의: 프리셋 원본이 지정된 렌더 작업은 실행할 때 프리셋 설정을 다시 읽는다. 테스트 구간을 일시적으로 바꾸려면 테스트 전용 프리셋/작업을 사용하거나 프리셋 원본 연결을 해제한 복사 작업을 사용한다.
