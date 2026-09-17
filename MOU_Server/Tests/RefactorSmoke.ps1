param(
    [string]$ServerExe = "$PSScriptRoot/../Server_Build/cmake_refactor/Release/Server.exe",
    [string]$ClientExe = "$PSScriptRoot/../Server_Build/cmake_refactor/Release/TestClient.exe"
)
$ErrorActionPreference = 'Stop'
$serverPath = (Resolve-Path -LiteralPath $ServerExe).Path
$clientPath = (Resolve-Path -LiteralPath $ClientExe).Path
$testDir = New-Item -ItemType Directory -Path (Join-Path (Split-Path $serverPath) ('smoke-' + [guid]::NewGuid().ToString('N')))
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()
$server = Start-Process -FilePath $serverPath -ArgumentList @($port, 'smoke.db') -WorkingDirectory $testDir.FullName -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $testDir.FullName 'server.log') -RedirectStandardError (Join-Path $testDir.FullName 'server-error.log')
try {
    $ready = $false
    for ($i = 0; $i -lt 50; $i++) {
        if ($server.HasExited) { throw 'Test server exited during startup.' }
        $probe = [System.Net.Sockets.TcpClient]::new()
        try { $probe.Connect('127.0.0.1', $port); $ready = $true } catch {} finally { $probe.Dispose() }
        if ($ready) { break }
        Start-Sleep -Milliseconds 100
    }
    if (!$ready) { throw 'Test server did not become ready.' }
    foreach ($mode in @('split', 'merge', 'bad')) {
        $output = & $clientPath 127.0.0.1 $port "smoke_$mode" password123 $mode --register "smoke_$mode" 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Client failed: $mode" }
        Write-Output $output
    }
    @('/host SmokeRoom', '/rooms', '/go', '/hostready', '/q') | & $clientPath 127.0.0.1 $port smoke_room password123 --register smoke_room
    if ($LASTEXITCODE -ne 0) { throw 'Room client failed.' }
    $log = Get-Content -Raw -Encoding utf8 (Join-Path $testDir.FullName 'server.log')
    foreach ($line in ($log -split "`r?`n")) {
        if ($line.Length -gt 0 -and $line -notmatch '^\[\d{4}/\d{2}/\d{2}/\d{2}시\d{2}분\d{2}초\] ') {
            throw "Missing timestamp: $line"
        }
    }
    foreach ($expected in @('분할전송테스트', '합침테스트1', '합침테스트2', '합침테스트3', '[차단]', '[방 생성]', '[게임 시작]', '[호스트 준비]', '[방 삭제]')) {
        if (!$log.Contains($expected)) { throw "Missing server event: $expected" }
    }
    Write-Output "PASS: register/login, split/merge, malformed frame, room lifecycle. Logs: $($testDir.FullName)"
} finally {
    # Only the isolated process launched by this test is stopped.
    if (!$server.HasExited) { Stop-Process -Id $server.Id; $server.WaitForExit() }
}
