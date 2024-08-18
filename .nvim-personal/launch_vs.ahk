ControlSend,, {shift down}{f5}{shift up}, ahk_exe devenv.exe
WinActivate, ahk_exe devenv.exe
sleep 250
Send, {Alt down}{Tab}{Alt up}
Send, {Alt down}{Tab}{Alt up}
sleep 100

if WinActive("File Modification Detected") {
    Send, {Enter}
    sleep 300
}

ControlSend,, {f5}, ahk_exe devenv.exe
