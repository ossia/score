
message(${QTDIR})
return()
if(WIN32)
	# Qt stuff
	if(CMAKE_BUILD_TYPE MATCHES Debug)
		get_target_property(QtWinPlugin Qt5::QWindowsIntegrationPlugin LOCATION_Debug)
		file(TO_CMAKE_PATH "C:\\Qt\\5.3\\msvc2013\\bin\\libEGLd.dll" LIBEGL_DLL)
	elseif(CMAKE_BUILD_TYPE MATCHES Release)
		get_target_property(QtWinPlugin Qt5::QWindowsIntegrationPlugin LOCATION)
		file(TO_CMAKE_PATH "C:\\Qt\\5.3\\msvc2013\\bin\\libEGL.dll" LIBEGL_DLL)
	endif()

	install(FILES "${QtWinPlugin}" DESTINATION "${plugin_dest_dir}/platforms" COMPONENT Runtime)
	install(FILES ${LIBEGL_DLL} DESTINATION bin COMPONENT Runtime)

	# Libraries
	file(TO_CMAKE_PATH "C:\\Qt\\5.3\\msvc2013\\bin" QT_FOLDER)
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\libxml2-2.7.8.win32\\bin" LIBXML_FOLDER)
	# Fuck this
	file(TO_CMAKE_PATH "W:\\OSSIA_win\\Jamoma\\Core\\Foundation\\library\\libiconv\\bin" LIBICONV_FOLDER)
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\zlib" ZLIB_FOLDER)

	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\redist\\x86\\Microsoft.VC120.CRT" MSVC_REDIST_RELEASE)
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\Microsoft Visual Studio 12.0\\VC\\redist\\Debug_NonRedist\\x86\\Microsoft.VC120.DebugCRT" MSVC_REDIST_DEBUG)

	install(FILES "${MSVC_REDIST_RELEASE}/msvcr120.dll"
				  "${MSVC_REDIST_RELEASE}/msvcp120.dll"
				  "${MSVC_REDIST_DEBUG}/msvcr120d.dll"
				  "${MSVC_REDIST_DEBUG}/msvcp120d.dll"
			DESTINATION bin
			COMPONENT Runtime)

	# Jamoma extensions
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\JamomaCore 0.6-dev\\lib\\jamoma" JAMOMA_EXTENSIONS_FOLDER)
	set(JAMOMA_EXTENSIONS
		"${JAMOMA_EXTENSIONS_FOLDER}/Scenario.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/Automation.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/Interval.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/OSC.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/Minuit.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/MIDI.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/AnalysisLib.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/DataspaceLib.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/FunctionLib.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/System.ttdll"
		"${JAMOMA_EXTENSIONS_FOLDER}/NetworkLib.ttdll")

	install(FILES ${JAMOMA_EXTENSIONS}
			DESTINATION bin
			COMPONENT Runtime)

	# Jamoma extensions also require their own DLLs.
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\Portmidi\\lib\\portmidi.dll" PORTMIDI_DLL)
	install(FILES ${PORTMIDI_DLL} DESTINATION bin COMPONENT Runtime)
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\pthread-win32\\dll\\x86\\pthreadVC2.dll" PTHREAD_DLL)
	install(FILES ${PTHREAD_DLL} DESTINATION bin COMPONENT Runtime)
	file(TO_CMAKE_PATH "C:\\Program Files (x86)\\Gecode\\bin" GECODE_DLL_FOLDER)

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(GECODE_WIN32_BUILD_TYPE "d")
	else()
		set(GECODE_WIN32_BUILD_TYPE "r")
	endif()

	set(GECODE_DLLS
		"${GECODE_DLL_FOLDER}/GecodeDriver-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeFlatZinc-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeInt-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeFloat-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeGist-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeKernel-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeMinimodel-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeSet-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeSupport-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll"
		"${GECODE_DLL_FOLDER}/GecodeSearch-4-3-0-${GECODE_WIN32_BUILD_TYPE}-x86.dll")
	install(FILES ${GECODE_DLLS} DESTINATION bin COMPONENT Runtime)


	set(DIRS "${QT_FOLDER}" "${JAMOMA_LIB_FOLDER}/../bin" "${JAMOMA_LIB_FOLDER}" "${JAMOMA_EXTENSIONS_FOLDER}" "${LIBXML_FOLDER}" "${LIBICONV_FOLDER}" "${ZLIB_FOLDER}" )
	#~ set(DIRS ${QtCore_location} ${QtSvg_location} ${QtXml_location} ${QtNetwork_location} ${QtWidgets_location} ${QtPrintSupport_location})
	install(CODE "
					file(GLOB_RECURSE QTPLUGINS
					  \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/platforms/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
					  set(PLUGINS ${QTPLUGINS} ${JAMOMA_EXTENSIONS})
					include(BundleUtilities)
					fixup_bundle(\"${APPS}\"   \"${PLUGINS}\"   \"${DIRS}\")
				 "
			COMPONENT Runtime)

	# VC++ redistributable
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/redist/vcredist_2010_x86.exe"
			DESTINATION redist)