IfWinExist, ahk_exe devenv.exe
{
    WinActivate, ahk_exe devenv.exe
    send {shift down}{f5}{shift up}
    sleep 200
    send {f5}
}
