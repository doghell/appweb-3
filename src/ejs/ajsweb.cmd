@echo off
setlocal

"ajs" -c "App.exeDir" >__ajsweb__.tmp
set /p DIR= <__ajsweb__.tmp
del __ajsweb__.tmp
call "ajs" %DIR%/ajsweb %*
