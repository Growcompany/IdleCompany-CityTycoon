@echo off
setlocal
chcp 65001 >nul
rem Balance Studio - local balance tuning tool. Double-click to run.
rem Messages here are ASCII on purpose: cmd re-reads this file per line and can
rem garble multibyte text right after chcp. All Korean UI lives in the browser.
where node >nul 2>nul || (
  echo [Balance Studio] Node.js 18 or later is required.
  echo Install Node.js, then run this file again.
  pause
  exit /b 1
)
for /f %%V in ('node -p "process.versions.node.split('.')[0]"') do set "BALANCE_NODE_MAJOR=%%V"
if %BALANCE_NODE_MAJOR% LSS 18 (
  echo [Balance Studio] Node.js 18 or later is required ^(found major %BALANCE_NODE_MAJOR%^).
  pause
  exit /b 1
)
node "%~dp0studio\server.js"
pause
