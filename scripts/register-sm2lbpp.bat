@ECHO OFF
REM @file register-sm2lbpp.bat
REM @author Daniel Starke
REM
REM DISCLAIMER
REM This file has no copyright assigned and is placed in the Public Domain.
REM All contributions are also assumed to be in the Public Domain.
REM Other contributions are not permitted.
REM
REM THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
REM EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
REM MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
REM IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
REM OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
REM ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
REM OTHER DEALINGS IN THE SOFTWARE.
REM
REM See https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers

SETLOCAL EnableExtensions
SET "APP=%~dp0sm2lbpp.exe"
SET "ICO=%SYSTEMROOT%\System32\shell32.dll,302"
SET "KEY=HKCU\Software\Classes\.nc\shell\AddSnapmakerThumbnail"

IF EXIST "%APP%" GOTO APP_FOUND
	ECHO Error: %APP% not found. Please place sm2lbpp.exe next to this script. >&2
	PAUSE
	GOTO :EOF
:APP_FOUND

>NUL 2>&1 (
	%__APPDIR__%reg.exe ADD "%KEY%" /ve /d "Add Snapmaker Thumbnail" /f
	%__APPDIR__%reg.exe ADD "%KEY%" /v Icon /t REG_SZ /d "\"%ICO%\"" /f
	%__APPDIR__%reg.exe ADD "%KEY%" /v MultiSelectModel /t REG_SZ /d "Single" /f
	%__APPDIR__%reg.exe ADD "%KEY%\command" /ve /d "\"%APP%\" \"%%1\"" /f
) && GOTO REG_ADDED
	ECHO Error: Failed to add registry key. Missing permissions? >&2
	PAUSE
	GOTO :EOF
:REG_ADDED
