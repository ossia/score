set SCORE_PATH=%CD%
cd build
cd install

REM makepri createconfig /cf priconfig.xml /dq en-US
REM makepri.exe new /pr %cd% /cf %cd%\priconfig.xml



mt.exe -nologo -manifest score.exe.manifest -outputresource:"score.exe;#1"

signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug score.exe
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug libc++.dll
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug libunwind.dll
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug libwinpthread-1.dll
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug ossia-score-vstpuppet.exe
signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug ossia-score-vst3puppet.exe

makeappx build /v /f %SCORE_PATH%\cmake\Deployment\Windows\store\PackagingLayout.xml /op ..\output /bv %GITTAGNOV%.0 /pv %GITTAGNOV%.0 /ca

signtool sign /fd sha256 /a /f %OSSIA_WIN32_CERTIFICATE% /p %OSSIA_WIN32_CERTIFICATE_PASSWORD% /debug %SCORE_PATH%\build\output\ossia-score.appxbundle

