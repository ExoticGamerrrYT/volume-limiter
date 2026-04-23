@echo off
setlocal

:: ─── Elevar a administrador si es necesario ──────────────────────────────────
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set "EXE=%~dp0volume-limiter.exe"
set "TASK=VolumeLimiter"

if not exist "%EXE%" (
    echo ERROR: No se encontro volume-limiter.exe junto a este script.
    pause & exit /b 1
)

:: ─── Crear script PowerShell temporal ────────────────────────────────────────
set "PS=%TEMP%\reg-vlimiter.ps1"
del "%PS%" 2>nul

echo $exe  = '%EXE%' >> "%PS%"
echo $task = '%TASK%' >> "%PS%"
echo $a = New-ScheduledTaskAction -Execute $exe >> "%PS%"
echo $t = New-ScheduledTaskTrigger -AtLogOn >> "%PS%"
echo $s = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew >> "%PS%"
echo $s.Priority = 4 >> "%PS%"
echo $s.ExecutionTimeLimit = 'PT0S' >> "%PS%"
echo $p = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest >> "%PS%"
echo Register-ScheduledTask -TaskName $task -Action $a -Trigger $t -Settings $s -Principal $p -Force ^| Out-Null >> "%PS%"
echo Write-Host "OK: tarea '$task' registrada con prioridad alta." >> "%PS%"

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS%"
set ERR=%errorlevel%
del "%PS%" 2>nul

echo.
if %ERR% equ 0 (
    echo [OK] volume-limiter se ejecutara al iniciar sesion con prioridad alta.
    echo      Para eliminar: schtasks /delete /tn "%TASK%" /f
) else (
    echo [ERROR] No se pudo registrar la tarea. Revisa permisos de administrador.
)
pause
endlocal
