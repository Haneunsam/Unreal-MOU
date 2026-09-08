# 서버 소스 구성

각 모듈은 `MOU_Server/Code/<모듈명>/<모듈명>.h/.cpp`에 둔다. 진입점인
`Code/Server/Server.cpp`에는 `main()`만 남긴다. Shared, ThirdParty, TestClient는
기존 공유 코드·외부 코드·테스트 프로그램 경계를 유지한다.

| 모듈 | 책임 |
|---|---|
| ServerApp | 초기화, TCP accept, 클라이언트 수신 루프, 종료 순서 |
| ServerConfig | 명령행 인자 파싱·검증·사용법 |
| ServerContext | 프로세스 단위 세션 관리자, 실행 상태, 주소, UDP 소켓, 릴레이 인스턴스 |
| ServerLog | UTF-8 콘솔 설정, 무버퍼 출력, 출력 호출 직렬화 |
| PacketDispatcher | 오피코드별 요청 핸들러 선택 |
| AuthHandler | 가입·로그인 요청과 응답 |
| ChatHandler | 채널 권한, 채팅 전달, 생존 상태 변경 |
| RoomHandler | 로비 요청, 방 상태 변경 후 알림 |
| SocialHandler | 친구·DM 요청, 접속 상태 알림, 메시지 전달 |
| SessionMessaging | 인증된 세션 조회, 대상 사용자들에게 패킷 전송 |
| EndpointService | 호스트 주소 후보, UDP 등록 수신, 프로브·도달성 처리 |
| RelayRouteService | 방별 릴레이 경로·토큰 생성, 조회·해제 |
| Session | 접속 세션 데이터와 목록 관리 |
| Rooms | 메모리 방 레지스트리와 상태 규칙 |
| Accounts / Friends / DirectMessages | 각 기능의 동기 SQLite 저장소 |
| ChatLog | 채팅 기록 비동기 저장 큐 |
| Crypto | 계정 비밀번호 해시 |
| NatPortMapping | UPnP 포트 매핑 |
| UdpRelay | 게임 UDP 전달 |

요청은 `ServerApp → PacketDispatcher → Handler → 저장소/서비스` 순서로 처리된다.
핸들러 사이에 필요한 접속 상태·방 종료 알림 호출은 기존 실행 순서를 유지한다.
Rooms/DB 락을 잡은 채 세션 목록 락을 추가로 잡지 않는 기존 규칙도 유지한다.

ServerContext는 현재 단일 서버 프로세스를 위한 공용 상태 접근점이다. 이번 변경은
기존 스레드·소켓 수명과 동기화 방식을 유지하며, 여러 서버 인스턴스의 동시 실행을
한 프로세스에서 지원하도록 바꾸지는 않는다. 릴레이 경로 맵과 해당 mutex는
RelayRouteService.cpp가 소유한다.

ServerLog::Print는 기존 콘솔 문구 앞에 서버 PC 로컬 시각을
`[YYYY/MM/DD/HH시MM분SS초]` 형식으로 붙인다. 호출 하나씩 mutex로 보호하며,
여러 호출로 나뉜 로그 전체가 원자적으로 출력되는 것은 아니다. 각 비어 있지 않은
줄 시작에 한 번 표시하며, 배치 파일의 echo 출력에는 적용하지 않는다.
ChatLog의 DB 타임스탬프와 네트워크 본문은 별개다.

## 빌드와 검증

MOU_Server 디렉터리에서 실행한다.

```bat
build_server.bat
rem 실행 중인 Server.exe를 교체하지 않고 검증하려면:
build_server.bat Server_Build\Server_refactor_verify.exe
```

```powershell
cmake -S . -B Server_Build/cmake_refactor
cmake --build Server_Build/cmake_refactor --config Release
./Tests/RefactorSmoke.ps1
```

RefactorSmoke는 별도 포트·DB의 서버만 생성하고 종료한다. 테스트 로그와 DB는
빌드 폴더에 남긴다. FriendsTest는 실행 디렉터리의 m2.db를 지우므로 전용 빈
디렉터리에서 실행한다. 프로토콜은 Shared/ChatProtocol.h의 v11을 유지한다.
새 모듈 추가 시 CMakeLists.txt와 build_server.bat 양쪽 소스 목록을 갱신한다.
