param([string]$TestScript='runtime', [string]$TestLog='runtime')
$taskArgs = @(
    '"C:/Users/user1/Documents/GitHub/Unreal-MOU/TeamProject_MOU/TeamProject_MOU.uproject"',
    '/Game/02_JSY/MainLobby/MainLobby',
    '-EnablePlugins=PythonScriptPlugin',
    "-ExecutePythonScript=`"C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/$TestScript.py`"",
    '-UserDir="C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/test-user"',
    '-unattended','-nosplash','-NoSound','-NoLiveCoding','-RenderOffscreen',
    "-abslog=`"C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/$TestLog.log`""
)
$taskEditor = Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
$taskEditor.Id | Set-Content 'C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui/test-pid.txt'
$taskEditor.Id
