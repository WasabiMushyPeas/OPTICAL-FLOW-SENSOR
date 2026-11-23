@echo off
setlocal EnableExtensions
set "LOG=%~dp0_eda_agent_setup.log"
call :main > "%LOG%" 2>&1
type "%LOG%"
echo.
echo Done. You can close this window.
pause
exit /b

:main
echo === eda-agent setup %DATE% %TIME% ===
set "DEST=%USERPROFILE%\eda-agent"
set "PY="
for %%V in (3.13 3.12 3.11) do if not defined PY (py -%%V -c "import sys" >nul 2>&1 && set "PY=py -%%V")
if not defined PY (python -c "import sys; assert sys.version_info>=(3,11)" >nul 2>&1 && set "PY=python")
if not defined PY (echo ERROR: Python 3.11+ not found & exit /b 1)
echo Python: %PY%
%PY% --version

if exist "%DEST%\pyproject.toml" (
  echo Repo already at %DEST%
  where git >nul 2>&1 && git -C "%DEST%" pull
) else (
  where git >nul 2>&1
  if errorlevel 1 (
    echo git not found - downloading zip
    powershell -NoProfile -Command "$z=\"$env:TEMP\eda-agent.zip\"; Invoke-WebRequest https://github.com/salitronic/eda-agent/archive/refs/heads/main.zip -OutFile $z; Expand-Archive $z \"$env:TEMP\eda-agent-x\" -Force; Move-Item (Get-ChildItem \"$env:TEMP\eda-agent-x\")[0].FullName \"%DEST%\""
  ) else (
    git clone https://github.com/salitronic/eda-agent "%DEST%"
  )
)
if not exist "%DEST%\pyproject.toml" (echo ERROR: download failed & exit /b 1)

if not exist "%DEST%\.venv\Scripts\python.exe" %PY% -m venv "%DEST%\.venv"
set "VPY=%DEST%\.venv\Scripts\python.exe"
"%VPY%" -m pip install --upgrade pip
"%VPY%" -m pip install -e "%DEST%"
if errorlevel 1 (echo ERROR: pip install failed & exit /b 1)
"%VPY%" -m eda_agent install-scripts --force
echo.
echo VENV_PYTHON=%VPY%
echo PRJSCR:
dir /s /b "%USERPROFILE%\EDA Agent\scripts\*.PrjScr"

echo.
echo === Registering in Claude Desktop config (backup made first) ===
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f=\"$env:APPDATA\Claude\claude_desktop_config.json\"; if(Test-Path $f){Copy-Item $f \"$f.bak-edaagent\" -Force; $c=Get-Content $f -Raw | ConvertFrom-Json}else{$c=[pscustomobject]@{}}; if(-not $c.mcpServers){$c | Add-Member -NotePropertyName mcpServers -NotePropertyValue ([pscustomobject]@{}) -Force}; $s=[pscustomobject]@{command=$env:VPY; args=@('-m','eda_agent','--headless'); env=[pscustomobject]@{EDA_AGENT_READONLY='1'; EDA_AGENT_TOOLSET='minimal'; EDA_AGENT_UI_AUTOMATION='0'; EDA_AGENT_UPDATE_CHECK='0'}}; $c.mcpServers | Add-Member -NotePropertyName altium -NotePropertyValue $s -Force; [IO.File]::WriteAllText($f, ($c | ConvertTo-Json -Depth 20), (New-Object Text.UTF8Encoding $false)); Write-Host ('Wrote altium server to ' + $f)"
echo === SETUP OK ===
exit /b 0
