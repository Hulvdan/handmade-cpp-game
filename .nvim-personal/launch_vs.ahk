IfWinExist, ahk_exe devenv.exe
{
    WinActivate, ahk_exe devenv.exe
    sleep 200
    send {ctrl down}{shift down}{f5}{shift up}{ctrl up}
    send {f5}
}
