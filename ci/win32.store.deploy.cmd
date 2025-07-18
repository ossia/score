@echo off
set SCORE_PATH=%cd%
set BUILD_PATH=%cd%\build
set OSSIA_WIN32_CERTIFICATE=%SCORE_PATH%\ossia-selfsigned.pfx

cd %BUILD_PATH%
rmdir output /s

cd install

REM Cleanup unused things
rmdir faust\docs /s

del /s /q *.eot
del /s /q *.ttf
del /s /q *.woff
del /s /q *.md
del /s /q *.css
del /s /q *.html
del /s /q *.gz
del /s /q *.jpg
del /s /q *.jpeg
del /s /q *.xml
del /s /q *.pri

REM Create resource files
REM (Necessary for multi-scale images, etc.)
makepri.exe createconfig /cf priconfig.xml /dq en-US
makepri.exe new /pr %cd% /cf %cd%\priconfig.xml /mn %cd%\manifests\Package.appxmanifest

REM Embed manifest inside score.exe
mt.exe -nologo -manifest score.exe.manifest -outputresource:"score.exe;#1"

REM Sign every binary file
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% score.exe
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% ossia-score-vstpuppet.exe
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% ossia-score-vst3puppet.exe
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% libc++.dll
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% libunwind.dll
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% libwinpthread-1.dll

REM Create the appxbundle
makeappx build /v /f %SCORE_PATH%\cmake\Deployment\Windows\store\PackagingLayout.xml /op %SCORE_PATH%\output /ca

REM Sign the appxbundle
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERT_PASSWORD% %SCORE_PATH%\output\ossia-score.appxbundle

