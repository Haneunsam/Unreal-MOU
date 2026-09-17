@echo off
REM VS install path differs per PC (2022 / 2026, Community / Professional).
REM Hardcoding one path made this script silently fail on other machines,
REM leaving a stale Server.exe behind. Try the known paths in order.
setlocal
set "VSDEVCMD="
for %%P in (
  "C:\Program Files\Microsoft Visual Studio\18\Community"
  "C:\Program Files\Microsoft Visual Studio\18\Professional"
  "C:\Program Files\Microsoft Visual Studio\2022\Community"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
) do if not defined VSDEVCMD if exist "%%~P\Common7\Tools\VsDevCmd.bat" set "VSDEVCMD=%%~P\Common7\Tools\VsDevCmd.bat"

if not defined VSDEVCMD (
  echo [ERROR] Visual Studio C++ toolset not found. Install the "Game development with C++" workload.
  exit /b 1
)

call "%VSDEVCMD%" -arch=x64
cd /d "%~dp0"
if not exist Server_Build mkdir Server_Build

REM Optional output path allows verification while the production server is running.
set "SERVER_OUTPUT=Server_Build\Server.exe"
if not "%~1"=="" set "SERVER_OUTPUT=%~1"

cl /nologo /std:c++17 /EHsc /utf-8 /O2 /DSQLITE_THREADSAFE=1 /DSQLITE_OMIT_LOAD_EXTENSION /DSQLITE_DQS=0 /DSQLITE_DEFAULT_MEMSTATUS=0 ^
  /Fe:"%SERVER_OUTPUT%" /Fo:Server_Build\ ^
  Code\Server\Server.cpp ^
  Code\Accounts\Accounts.cpp ^
  Code\ChatLog\ChatLog.cpp ^
  Code\Crypto\Crypto.cpp ^
  Code\DirectMessages\DirectMessages.cpp ^
  Code\Friends\Friends.cpp ^
  Code\NatPortMapping\NatPortMapping.cpp ^
  Code\Rooms\Rooms.cpp ^
  Code\Session\Session.cpp ^
  Code\UdpRelay\UdpRelay.cpp ^
  Code\EndpointService\EndpointService.cpp ^
  Code\RelayRouteService\RelayRouteService.cpp ^
  Code\ServerApp\ServerApp.cpp ^
  Code\ChatHandler\ChatHandler.cpp ^
  Code\AuthHandler\AuthHandler.cpp ^
  Code\SessionMessaging\SessionMessaging.cpp ^
  Code\SocialHandler\SocialHandler.cpp ^
  Code\RoomHandler\RoomHandler.cpp ^
  Code\PacketDispatcher\PacketDispatcher.cpp ^
  Code\ServerConfig\ServerConfig.cpp ^
  Code\ServerContext\ServerContext.cpp ^
  Code\ServerLog\ServerLog.cpp ^
  Shared\Framing.cpp ThirdParty\sqlite\sqlite3.c ^
  /ICode /IShared /IThirdParty\sqlite ^
  ws2_32.lib
if errorlevel 1 (
  echo [ERROR] Build failed. Server.exe was NOT updated.
  exit /b 1
)
echo [OK] %SERVER_OUTPUT% updated.
