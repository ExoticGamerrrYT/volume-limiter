@echo off
:: Señala el evento Global\AudioLevelMgr_Stop para cerrar audmgr.exe limpiamente.

powershell -NoProfile -Command ^
  "try { $e = [System.Threading.EventWaitHandle]::OpenExisting('Global\AudioLevelMgr_Stop'); $e.Set(); $e.Dispose(); Write-Host '[OK] Proceso detenido.' } catch { Write-Host '[ERROR] El proceso no esta en ejecucion.' }"
