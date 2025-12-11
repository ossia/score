/*

			SpoutUtils

			Utility functions

	CREDIT - logging based on Openframeworks ofLog
	https://github.com/openframeworks/openFrameworks/tree/master/libs/openFrameworks/utils

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	Copyright (c) 2017-2025, Lynn Jarvis. All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, 
	are permitted provided that the following conditions are met:

		1. Redistributions of source code must retain the above copyright notice, 
		   this list of conditions and the following disclaimer.

		2. Redistributions in binary form must reproduce the above copyright notice, 
		   this list of conditions and the following disclaimer in the documentation 
		   and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"	AND ANY 
	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE	ARE DISCLAIMED. 
	IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	========================

		31.05.15 - started
		01.01.18 - added check for subkey
		17.03.18 - Document SetLogLevel function in header
		16.10.18 - Add SpoutLogNotice, Warning, Error, Fatal
		28.10.18 - Checks for lastlog and fatal messagebox moved from SpoutLog to _doLog
		15.11.18 - Removed delay after SpoutPanel open
		10.12.18 - Add Timeout option to SpoutMessageBox
		12.12.18 - Add SpoutLogFile
		13.12.18 - Add GetSpoutLog and ShowSpoutLogs
		14.12.18 - Clean up
		02.10.19 - Change registry functions including hKey
				   to allow HKLM and changed argument order
				 - Add RemoveSubKey and FindSubKey
		26.01.19 - Corrected Verbose log to show verbose and not notice
		07.04.19 - Add SpoutLog for logging without specifying level
		28.04.19 - Change OpenSpoutConsole() to check for existing console
		19.05.19 - Cleanup
		16.06.19 - Include calling process file name in SpoutMessageBox
		13.10.19 - Corrected EnableSpoutLogFile for a filename without an extension
				   Changed default extension from "txt" to "log"
		27.11.19 - Prevent multiple logs for warnings and errors
		22.12.19 - add pragma in header for registry function lbraries
		22.12.19 - Remove calling process name from SpoutMessageBox
		18.02.20 - Remove messagebox for Fatal errors
		19.05.20 - Add missing LPCSTR cast in SpoutMessageBox ShellExecute
		12.06.20 - Add timing functions for testing
		01.09.20 - Add seconds to log file header
		03.09.20 - Add DisableSpoutLogFile() DisableLogs() and EnableLogs() 
				   for more control over logging
		09.09.20 - move _doLog outside anonymous namespace
		23.09.20 - _doLog : always prevent multiple logs by comparing with the last
				   instead of reserving for > warnings
		16.10.20 - Add bool WriteBinaryToRegistry
		04.03.21 - Add std::string GetSDKversion()
		09.03.21 - Fix code if USE_CHRONO not defined
		17.04.21 - Disable close button on console
				   Bring the main window to the top again
		07.05.21 - Remove noisy warning from ReadPathFromRegistry
		09.06.21 - Update Version to "2.007.002"
		26.07.21 - Update Version to "2.007.003"
		16.09.21 - Update Version to "2.007.004"
		04.10.21 - Remove shlobj.h include due to redifinition conflict with ShObjIdl.h
				   Replace code using environment variable "APPDATA"
		24.10.21 - Update Version to "2.007.005"
		08.11.21 - Change to high_resolution_clock for timer
		15.12.21 - Change back to steady clock
				   Use .clear() instead of "" to clear strings
		20.12.21 - Change from string to to char array for last log
				   Update Version to "2.007.006"
		29.01.21 - Change return logic of RemovePathFromRegistry
		24.02.22 - Update Version to "2.007.007"
		25.02.22 - OpenSpoutConsole - check AllocConsole error for existing console
				   Fix for Processing.
		14.04.22 - Add option in SpoutCommon.h to disable warning 26812 (unscoped enums)
		23.06.22 - Add ElapsedMicroseconds (usec since epoch)
		30.10.22 - Code cleanup and documentation
		01.11.22 - Add IsLaptop(char* computername)
		30.11.22 - Update Version to "2.007.009"
		05.12.22 - Change ordering of _logtofile function to avoid ambiguous warning.
				   GetSpoutLog - optional log file argument. Remove redundant file open.
				   See SpoutSettongs "Log" option.
		07.12.22 - EnableSpoutLogFile allow null file name argument.
				   If a file name was not specified, use the executable name.
				   Use "GetCurrentModule" instead of NULL for GetModuleFileNameA
				   in case the process is a dll (https://gist.github.com/davidruhmann/8008844).
		08.12.22 - _dolog - clean up file log code.
				 - Corrected ExecuteProcess for EndTiming milliseconds return.
		11.12.22 - Initialize all char arrays and structures with {}
				   https://en.cppreference.com/w/cpp/language/zero_initialization
		14.12.22 - Add RemoveSpoutLogFile
		18.12.22 - Add buffer size argument to ReadPathFromRegistry
				   Correct code review warnings where possible
				   Add more documentation to Group: Logs
		19.12.22 - Add GetCurrentModule / LogsEnabled / LogFileEnabled / GetSpoutLogPath
		20.12.22 - Add SPOUT_DLLEXP to all header function declarations for dll export.
				 - Code cleanup
		22.12.22 - Compiler compatibility check
				   Change all {} initializations to "={}"
		31.12.22 - increase log char buffer from 512 to 1024
		01.12.22 - Registry functions
				     check for empty subkey and valuename strings
					 include valuename in warnings
		14.01.23 - OpenSpoutConsole - add MessageBox warning if using a dll
				   EnableSpoutLog - open console rather than call OpenSpoutConsole
		15.01.23 - Use SpoutMessageBox so it doesn't freeze the application GUI
		16.01.23 - Add SpoutMessageBox caption
		17.01.23 - Add SpoutMessageBox with variable arguments
				   Add ConPrint for SpoutUtils console (printf replacement)
				   Remove dll build warning MessageBox.
				   Change "ConPrint" to "_conprint" and use Writefile instead of cout.
		18.01.23 - _conprint - cast WriteFile size argument to DWORD
		19.03.23 - Update SDKversion to 2.007.010
		Version 2.007.011
		14.04.23 - Update SDKversion to 2.007.011
		24.04.23 - GetTimer - independent start and end variables startcount/endcount
		09.05.23 - Yellow console text for warnings and errors
		17.05.23 - Set console title to executable name
		04-07-23 - _getLogPath() - allow for getenv if not Microsoft compiler (PR #95)
		Version 2.007.012
		01.08.23 - Add MessageTaskDialog instead of dependence on SpoutPanel
		04.08.23 - const WCHAR* in MessageTaskDialog
		13.08.23 - MessageTaskDialog - remove MB_TOPMOST
		20.08.23 - Change TaskdialogcallbackProc to TDcallbackProc to avoid naming conflicts
		21.08.23 - MessageTaskDialog - Restore topmost function
				   Change bTopmost to bTopMost to avoid naming conflicts
		23.08.23 - MessageTaskDialog - Fixed topmost recover for calling application
		26.08.23 - PFTASKDIALOGCALLBACK cast for TDcallbackProc
		04.09.23 - MessageTaskDialog - add MB_ICONINFORMATION option. Default no icon.
				   Add MB_ICONSTOP and MB_ICONHAND. MB_TOPMOST flag removal only if specified.
		05.09.23 - Add SpoutMessageBoxIcon for custom icon
		07.09.23 - Add round to ElapsedMicroseconds
		08.09.23 - Check TDN_CREATED for taskdialog topmost
		11.09.23 - MessageTaskDialog - correct button and icon type detection
				 - Allow for NULL caption
		12.09.23 - Add SpoutMessageBox overload including main instruction large text
				 - Correct missing SPOUT_DLLEXP for SpoutMessageBox standard function
		12.10.23 - Add SpoutMessageBoxButton and CopyToClipBoard
		20.10.23 - Add OpenSpoutLogs to open Spout log folder in Windows explorer
		20-11-23 - OpenSpoutLogs() - allow for getenv if not Microsoft compile (PR #105)
		22-11-23 - Remove unused buffer length argument from _dupenv_s
		01.12.24 - Update Version to "2.007.013"
		07.12.23 - Remove SPOUT_DLLEXP from private MessageTaskDialog
				   Use _access and string find in place of shlwapi path functions
				   Add GetExePath, GetExeName, RemovePath
				   Revise : _getLogPath(), _getLogFilePath, _logtofile,
				   EnableSpoutLog, EnableSpoutLogFile, ShowSpoutLogs,
				   OpenSpoutConsole, GetNVIDIAmode, SetNVIDIAmode
		08.12.23 - #ifdef _MSC_VER for linker manifestdependency pragma comment
				   MessageTaskDialog - correct topmost if a second dialog is opened
		15.12.23 - Change to #ifdef _WINDOWS for linker manifestdependency pragma comment
				   Conditional compile of TaskDialogIndirect for _WINDOWS.
				   MessageBoxTimeoutA for other compilers.
		16.12.23 - Replace #define _WINDOWS with _MSC_VER for conditional compile for Visual Studio
				 - MessageTaskDialog - first argumnent HWND instead of HINSTANCE
				 - SpoutMessageBox - pass in hwnd to MessageTaskDialog
				 - Add TDF_POSITION_RELATIVE_TO_WINDOW to TaskDialogIndirect config flags
				 - TaskDialogIndirect centers on the window if hwnd passed in or the monitor if NULL
		20.12.23 - Remove GetNVIDIAmode, SetNVIDIAmode
				 - ExecuteProcess - use ShellExecuteEx instead of CreateProcess
				 - Add SpoutMessageBoxModeless
				 - Restore modeless SpoutMessageBox functionality using SpoutPanel, > v2.72 required.
				 - Call MessageTaskDialog directly in all SpoutMessageBox functions
				 - Use a custom icon if set for SpoutMessageBox functions that do not specify an icon
				 - Clear custom icon handle after TaskDialogIndirect exit
				 - Add SpoutMessageBoxWindow
		21.12.23 - Add std::string GetExeVersion()
				 - Revise SpoutMessageBoxModeless to test version of SpoutPanel > 2.072
		27.12.23 - Send OK button message to close taskdialog instead of DestroyWindow for URL click
				 - Test for custom icon and multiple buttons in MessageTaskDialog
		Version 2.007.013
		28.12.23 - SpoutMessageBox - add MB_RIGHT for right aligned text
		11.03.24 - Add MessageBox dialog with an edit control for text input
				   Add MessageBox dialog with a combo box control for item selection
				   Update Taskdialog callback to create the controls and return input
		19.03.24 - Add icon/button option for variable arguments
		29.03.24 - Correct ReadPathFromRegistry definition for default size argument
				   Correct EndTiming definition for microseconds argument
		Version 2.007.014
		14.06.24 - SpoutUtils.h - PR #114
				   Correct conditional definition of EndTiming in header file
				   Allow mingw to define USE_CHRONO if available
				   Include <math.h> to fix mingw build
		01.07.24 - Increase SpoutMessageBox combo width for NDI sender names
				 - Add "SpoutMessageBoxModeless" to warning caption if SpoutPanel not found
		02.07.24 - Add SpoutMessageBoxPosition
		09.07.24 - TDcallbackProc TDN_CREATED : common rect and coordinates
		15.07.24 - Update Spout SDK version
		24.07.24 - SpoutMessageBoxModeless - add code comments for SpoutPanel version
		06.08.24 - SpoutMessageBox - show initial content in edit box control
		08.08.24 - SpoutMessageBox - removed unused WS_HSCROLL in edit box control
		10.08.24 - SpoutMessageBox - select all text in the combobox edit field
		11.08.24 - Add CBS_HASSTRINGS style to combobox and detect CB_ERR.
		16.08.22 - ExecuteProcess, SpoutMessageBoxIcon return conditional value
				   to avoid warning C4800: 'BOOL': forcing value to bool 'true' or 'false'
				 - GetSpoutLog - remove null argument check for use of existing log path
		20.08.24 - GetSpoutLog - add check for empty filepath
		10.09.24 - ReadPathFromRegistry -
				   "valuename" argument can be null for the(Default) key string
		06.10.24 - OpenSpoutConsole, EnableSpoutLog - add optional title argument
		22.12.24 - Remove MB_USERBUTTON. Use TDbuttonID.size() instead.
				   SpoutMessageBoxModeless bool instead of void
		07.01.25 - GetExePath - add option for full path with executable name
		13.01.25 - Add #standalone define in SpoutUtils.h
		18.01.25 - Rename "#standalone" to "#standaloneUtils" to avoid naming conflicts
		01.02.25 - Add GetSpoutVersion
				   OpenSpoutLogs - use _getLogPath
				   GetSpoutLog - use _getLogFilePath
		02.02.25 - GetSDKversion - optional return integer version number
				   GetSpoutVersion - get the user Spout version string from the registry
				   optional return integer version number
		09.02.25 - Remove debug comments for MB_USERBUTTON (no longer used)
		17.02.25 - Adjust combo box width to the longest item string
				   Use CBS_DROPDOWNLIST style for list only
		04.03.25 - Add #include <algorithm> to SpoutUtils.h (PR #120)
		07.03.25 - MessageTaskDialog -
				   Add global "hwndTask" to return if TaskDialog is open.
				   Move modeless check to first.
				   Disable modeless mode after SpoutPanel open so it is one-off.
				   Window handle is HWND passed in or specified by SpoutMessageBoxWindow.
		08.03.25 - Add missing SPOUT_DLLEXP to SpoutMessageBoxIcon and SpoutMessageBoxButton
		09.03.25 - Change function names from RemoveName and RemovePath to
				   GetPath and GetName to modify the path argument and return a string
		27.03.25 - Remove messagebox from GetName
		20.04.25 - Correct GetExeName to return GetName
				   Correct GetExePath to return GetPath
		13.05.25 - Use a local file pointer for freopen_s with AllocConsole
				   if "standaloneutils" is defined to avoid crash - unknown cause
		25.05.25 - Add print option to EndTiming
		22.05.25 - Update SDKversion to 2.007.016
		09.08.25 - Change all "={}" initializations back to "{}"
		01.09.25 - Correct GetFileVersionInfoA dwHandle arg from NULL to 0
				   Correct RegOpenKeyExA options arg from NULL to 0
				   Correct RegCreateKeyExA reserved arg from NULL to 0
				   MessageBoxTimeoutA - add return value for else
		06.09.25 - Add executable name to log file
		16.09.25 - Update version to 2.007.017

*/

#include "SpoutUtils.h"

//
// Namespace: spoututils
//
// Namespace for utility functions.
//
// - Version
// - Console
// - Logs
// - MessageBox dialog
// - Registry utilities
// - Computer information
// - Timing utilities
//
// Refer to source code for documentation.
//


namespace spoututils {

	// Local variables
	bool bEnableLog = false;
	bool bEnableLogFile = false;
	bool bDoLogs = true;
	SpoutLogLevel CurrentLogLevel = SPOUT_LOG_NOTICE;
	FILE* pCout = nullptr; // for log to console
	std::ofstream logFile; // for log to file
	std::string logPath; // folder path for the logfile
	char logChars[1024]{}; // The current log string
	bool bConsole = false;
#ifdef USE_CHRONO
	std::chrono::steady_clock::time_point start;
	std::chrono::steady_clock::time_point end;
#endif
	// PC timer
	double PCFreq = 0.0;
	__int64 CounterStart = 0;
	double startcount = 0.0;
	double endcount = 0.0;
	double m_FrameStart = 0.0;

	// Spout SDK version number string
	// Major, minor, release
	std::string SDKversion = "2.007.017";

	//
	// Group: Information
	//

	// ---------------------------------------------------------
	// Function: GetSDKversion
	//
	// Get SDK version number string e.g. "2.007.000"
	// Optional - return as a single number
	// e.g. 2.006 = 2006, 2.007 = 2007, 2.007.009 = 2007009
	std::string GetSDKversion(int* pNumber) {
		// Version number string e.g. "2.007.009"
		std::string str = SDKversion;
		if (!str.empty() && pNumber) {
			// Remove all "." chars
			std::string nstr = str;
			nstr.erase(std::remove(nstr.begin(), nstr.end(), '.'), nstr.end());
			// integer from string e.g. 2007009
			*pNumber = std::stoi(nstr.c_str());
		}
		// Return the version string
		return str;
	}

	// ---------------------------------------------------------
	// Function: GetSpoutVersion
	//
	// Get the user Spout version number from the registry
	// Optional - return as a single number
	std::string GetSpoutVersion(int* number) {
		std::string vstr;
		DWORD dwVers = 0;
		if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "Version", &dwVers)) {
			// Create version string e.g. 2.006, 2.007, 2.007.009
			std::string str = std::to_string(dwVers);
			// 2006, 2007, 2007009
			vstr = str.substr(0, 1); // first digit
			vstr += "."; // decimal place
			vstr += str.substr(1, 3); // Major
			if (str.size() > 4) {
				vstr += ".";
				vstr += str.substr(4, 3); // Minor
			}
			if (number) {
				*number = (int)dwVers;
			}
		}
		return vstr;
	}


	// ---------------------------------------------------------
	// Function: IsLaptop
	// Return whether the system is a laptop.
	//
	// Queries power status. Most likely a laptop if battery power is available. 
	bool IsLaptop()
	{
		SYSTEM_POWER_STATUS status;
		if (GetSystemPowerStatus(&status)) {
			// SpoutLog("    ACLineStatus         %d", status.ACLineStatus);
			// SpoutLog("    BatteryFlag          %d", status.BatteryFlag);
			// SpoutLog("    BatteryLifePercent   %d", status.BatteryLifePercent);
			// SpoutLog("    SystemStatusFlag     %d", status.SystemStatusFlag);
			// SpoutLog("    BatteryLifeTime      %d", status.BatteryLifeTime);
			// SpoutLog("    BatteryFullLifeTime  %d", status.BatteryFullLifeTime);
			// BatteryFlag, battery charge status (128 - No system battery)
			// BatteryLifePercent,  % of full battery charge remaining (255 - Status unknown)
			if (status.BatteryFlag != 128 && status.BatteryLifePercent != 255) {
				return true;
			}
		}
		return false;
	}


	// ---------------------------------------------------------
	// Function: GetCurrentModule
	// Get the module handle of a process.
	//
	// This method is necessary if the process is a dll
	//
	// https://gist.github.com/davidruhmann/8008844
	HMODULE GetCurrentModule()
	{
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		HMODULE hModule = NULL;
		// hModule is NULL if GetModuleHandleEx fails.
		GetModuleHandleExA(flags, (LPCSTR)GetCurrentModule, &hModule);
		return hModule;
	}

	//---------------------------------------------------------
	// Function: GetExeVersion
	// Get executable version string
	//
	std::string GetExeVersion(const char* path)
	{
		// Get product version number
		std::string productversion;
		DWORD dwSize = GetFileVersionInfoSizeA(path, nullptr);
		if (dwSize > 0) {
			std::vector<BYTE> data(dwSize);
			if (GetFileVersionInfoA(path, 0, dwSize, &data[0])) {
				LPVOID pvProductVersion = NULL;
				unsigned int iProductVersionLen = 0;
				if (VerQueryValueA(&data[0], ("\\StringFileInfo\\080904E4\\ProductVersion"), &pvProductVersion, &iProductVersionLen)) {
					productversion = (char*)pvProductVersion;
				}
			}
		}
		return productversion;
	}

	// ---------------------------------------------------------
	// Function: GetExePath
	// Get executable or dll path
	//    bFull
	//	    true  - full path with executable name
	//	    false - path without executable name (default)
	//
	std::string GetExePath(bool bFull)
	{
		char path[MAX_PATH]{};
		// exe or dll
		GetModuleFileNameA(GetCurrentModule(), path, MAX_PATH);
		std::string exepath = path;

		if(!bFull)
			return GetPath(exepath); // Remove name and get path
		else
			return exepath;// Return the full path
		
	}

	// ---------------------------------------------------------
	// Function: GetExeName
	// Get executable or dll name
	//
	std::string GetExeName()
	{
		char path[MAX_PATH]{};
		GetModuleFileNameA(GetCurrentModule(), path, MAX_PATH);
		std::string exepath = path;
		// Remove extension
		size_t pos = exepath.rfind(".");
		exepath = exepath.substr(0, pos);
		// Remove path for name only
		return GetName(exepath);
	}

	// ---------------------------------------------------------
	// Function: GetPath
	// Remove file name and return the path
	//
	std::string GetPath(std::string fullpath) {
		std::string path;
		size_t pos = fullpath.rfind("\\");
		if (pos == std::string::npos)
			pos = fullpath.rfind("/");
		if (pos != std::string::npos) {
			path = fullpath.substr(0, pos + 1); // leave trailing backslash
		}
		return path;
	}

	// ---------------------------------------------------------
	// Function: GetName
	// Remove path and return the file name
	//
	std::string GetName(std::string fullpath) {
		std::string name;
		size_t pos = fullpath.rfind("\\");
		if (pos == std::string::npos)
			pos = fullpath.rfind("/");
		if (pos != std::string::npos) {
			name = fullpath.substr(pos + 1, fullpath.size() - pos);
		}
		return name;
	}

	//
	// Group: Console management
	//

	// ---------------------------------------------------------
	// Function: OpenSpoutConsole
	// Open console window.
	//
	// A console window opens without logs.
	// Useful for debugging with console output.
	//
	void OpenSpoutConsole(const char* title)
	{
		if (!GetConsoleWindow()) {

			//
			// Application console window mot found
			//

			// Get calling process window
			HWND hwndFgnd = GetForegroundWindow();
			if (AllocConsole()) {
				#ifndef standaloneUtils
					FILE* fp = nullptr;
					const errno_t err = freopen_s(&fp, "CONOUT$", "w", stdout);
				#else
					const errno_t err = freopen_s(&pCout, "CONOUT$", "w", stdout);
				#endif
				if (err == 0) {
					std::string name = GetExeName();
					if (title != nullptr) name = title;
					SetConsoleTitleA(name.c_str());
					bConsole = true;
					// Optional - disable close button
					// HMENU hmenu = GetSystemMenu(GetConsoleWindow(), FALSE);
					// EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
					// Bring the main window to the top again
					SetWindowPos(hwndFgnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
				else {
					pCout = nullptr;
					bConsole = false;
				}
				
			}
			else {
				// If the calling process is already attached to a console,
				// the error code returned is ERROR_ACCESS_DENIED(5).
				if (GetLastError() == 5) {
					bConsole = true;
				}
			}

		}
	}

	// ---------------------------------------------------------
	// Function: CloseSpoutConsole
	// Close console window.
	//
	// The optional warning displays a MessageBox if user notification is required.
	void CloseSpoutConsole(bool bWarning)
	{
		if (bWarning) {
			if (MessageBoxA(NULL, "Console close - are you sure?", "CloseSpoutConsole", MB_YESNO) == IDNO)
				return;
		}
		if(pCout) fclose(pCout);
		if (GetConsoleWindow()) FreeConsole();
		pCout = nullptr;
		bConsole = false;
	}


	//
	// Group: Logs
	//
	//
	// Spout logs are used thoughout the SDK and are printed to a console
	// with EnableLogs or saved to a file with EnableLogFIle.
	//
	// You can set the level above which the logs are shown
	// SPOUT_LOG_SILENT  : SPOUT_LOG_VERBOSE : SPOUT_LOG_NOTICE (default)
	// SPOUT_LOG_WARNING : SPOUT_LOG_ERROR   : SPOUT_LOG_FATAL
	// For example, to show only warnings and errors (you shouldn't see any)
	// or leave set to default Notice to see more information.
	//
	//    SetSpoutLogLevel(SPOUT_LOG_WARNING);
	//
	// You can instead, or additionally, output to a text log file
	// with the name and extension of your choice.
	//    EnableSpoutLogFile("OF Spout Graphics sender.log");
	//
	// The log file is re-created every time the application starts
	// unless you specify to append to the existing one :
	//    EnableSpoutLogFile("OF Spout Graphics sender.log", true);
	//
	// The file is saved in the %AppData% folder unless you specify the full path.
	//    C:>Users>username>AppData>Roaming>Spout
	//
	// If there is no file specified, the executable or dll name is used.
	// 
	// After the application has run you can find and examine the log file
	//
	// This folder can also be shown in Windows Explorer directly from the application.
	//    ShowSpoutLogs();
	//
	// Or the entire log file can be returned as a string
	//    std::string logstring = GetSpoutLog();
	//
	// You can also create your own logs
	// For example :
	//    SpoutLog("SpoutLog test");
	//
	// Or specify the logging level :
	// For example :
	//    SpoutLogNotice("Important notice");
	// or :
	//    SpoutLogFatal("This should not happen");
	// or :
	//    SetSpoutLogLevel(SPOUT_LOG_VERBOSE);
	//    SpoutLogVerbose("Message");
	//
	// See SpoutUtils.h for mre information
	//

	// ---------------------------------------------------------
	// Enum: Log level definitions
	// The level above which the logs are shown.
	// 
	//   SPOUT_LOG_SILENT - Disable all messages.
	//   SPOUT_LOG_VERBOSE - Show all messages.
	//   SPOUT_LOG_NOTICE - Show information messages (default).
	//   SPOUT_LOG_WARNING - Show warning, errors and fatal.
	//   SPOUT_LOG_ERROR - Show errors and fatal.
	//   SPOUT_LOG_FATAL - Show only fatal errors.
	//   SPOUT_LOG_NONE - Ignore log levels.
	// 
	// For example, to show only warnings and errors (you shouldn't see any):
	// 
	// 	SetSpoutLogLevel(SPOUT_LOG_WARNING);
	//

	// ---------------------------------------------------------
	// Function: EnableSpoutLog
	// Enable logging to the console.
	//
	// Logs are displayed in a console window.  
	// Useful for program development.
	//
	void EnableSpoutLog(const char* title)
	{

		std::string name = GetExeName();
		name += ".log";
		if (title != nullptr) name = title;

		if (!GetConsoleWindow())
			OpenSpoutConsole(name.c_str());

		bConsole = true;
		bEnableLog = true;

		// Initialize current log string
		logChars[0] = 0;
	}

	// ---------------------------------------------------------
	// Function: EnableSpoutLogFile
	// Enable logging to a file with optional append.
	//
	// As well as a console window, you can output logs to a text file. 
	// Default extension is ".log" unless the full path is used.
	// For no file name or path the executable name is used.
	//
	//     Example : EnableSpoutLogFile("Sender.log");
	//
	// The log file is re-created every time the application starts
	// unless you specify to append to the existing one.  
	//
	//    Example : EnableSpoutLogFile("Sender.log", true);
	//
	// The file is saved in the %AppData% folder unless you specify the full path : 
	//
	//    C:>Users>username>AppData>Roaming>Spout   
	//
	// You can find and examine the log file after the application has run.
	void EnableSpoutLogFile(const char* filename, bool bAppend)
	{
		bEnableLogFile = true;
		if (!logPath.empty()) {
			if (logFile.is_open())
				logFile.close();
			logPath.clear();
		}
		logChars[0] = 0;

		// Create the log file path given the filename passed in
		logPath = _getLogFilePath(filename);

		_logtofile(bAppend);

	}

	// ---------------------------------------------------------
	// Function: DisableSpoutLogFile
	// Disable logging to file
	void DisableSpoutLogFile() {
		if (!logPath.empty()) {
			if (logFile.is_open())
				logFile.close();
			logPath.clear();
		}
	}

	// ---------------------------------------------------------
	// Function: RemoveSpoutLogFile
	// Remove a log file
	void RemoveSpoutLogFile(const char* filename)
	{
		// The path derived from the name or path passed in
		// could be different to the current log file
		std::string path = _getLogFilePath(filename);

		if (!path.empty()) {
			// If it's the same as the current log file
			if (!logPath.empty() && path == logPath) {
				// Stop file logging and clear the log file path
				DisableSpoutLogFile();
			}
			// Remove the file if it exists
			if (_access(path.c_str(), 0) != -1)
				remove(path.c_str());
		}
	}

	// ---------------------------------------------------------
	// Function: DisableSpoutLog
	// Disable logging to console and file
	void DisableSpoutLog()
	{
		CloseSpoutConsole();
		if (!logPath.empty()) {
			if (logFile.is_open())
				logFile.close();
			logPath.clear();
		}
		bEnableLog = false;
		bEnableLogFile = false;
	}

	// ---------------------------------------------------------
	// Function: DisableLogs
	// Disable logs temporarily
	void DisableLogs() {
		bDoLogs = false;
	}

	// ---------------------------------------------------------
	// Function: EnableLogs
	// Enable logging again
	void EnableLogs() {
		bDoLogs = true;
	}

	// ---------------------------------------------------------
	// Function: LogsEnabled
	// Are console logs enabled
	bool LogsEnabled()
	{
		return bEnableLog;
	}

	// ---------------------------------------------------------
	// Function: LogFileEnabled
	// Is file logging enabled
	bool LogFileEnabled()
	{
		return bEnableLogFile;
	}

	// ---------------------------------------------------------
	// Function: GetSpoutLogPath
	// Return the full log file path
	std::string GetSpoutLogPath()
	{
		return logPath;
	}

	// ---------------------------------------------------------
	// Function: GetSpoutLog
	// Return the Spout log file as a string
	// If not a full path, prepend appdata\Spout
	// If a file path is not specified, return the current log file
	std::string GetSpoutLog(const char* filepath)
	{
		std::string logstr = "";
		std::string path;

		// Check for specified log file path
		if (filepath && *filepath != 0) {
			path = _getLogFilePath(filepath);
		}
		else {
			path = logPath;
		}
		
		 // does the log file exist
		if (_access(path.c_str(), 0) != -1) {
			// Open the log file
			std::ifstream logstream(path);
			// Source file loaded OK ?
			if (logstream.is_open()) {
				// Get the file text as a single string
				logstr.assign((std::istreambuf_iterator<char>(logstream)), std::istreambuf_iterator<char>());
				logstr += ""; // ensure a NULL terminator
				logstream.close();
			}
		}

		return logstr;
	}

	// ---------------------------------------------------------
	// Function: ShowSpoutLogs
	// Show the Spout log file folder in Windows Explorer
	void ShowSpoutLogs()
	{
		char directory[MAX_PATH]{};
		std::string logfilefolder;

		if (logPath.empty() || _access(logPath.c_str(), 0) == -1) {
			logfilefolder = _getLogPath();
		}
		else {
			logfilefolder = logPath.c_str();
			// Remove file spec
			size_t pos = logfilefolder.rfind("\\");
			if (pos == std::string::npos)
				pos = logfilefolder.rfind("/");
			if (pos != std::string::npos)
				logfilefolder = logfilefolder.substr(0, pos);
		}

		// Current log file path
		strcpy_s(directory, MAX_PATH, logfilefolder.c_str());

		SHELLEXECUTEINFOA ShExecInfo;
		memset(&ShExecInfo, 0, sizeof(ShExecInfo));
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.lpFile = (LPCSTR)directory;
		ShExecInfo.nShow = SW_SHOW;
		ShellExecuteExA(&ShExecInfo);
	}

	// ---------------------------------------------------------
	// Function: SetSpoutLogLevel
	// Set the current log level
	void SetSpoutLogLevel(SpoutLogLevel level)
	{
		CurrentLogLevel = level;
	}


	// ---------------------------------------------------------
	// Function: SpoutLog
	// General purpose log - ignore log levels and no log level shown
	void SpoutLog(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_NONE, format, args);
		va_end(args);
	}

	// ---------------------------------------------------------
	// Function: SpoutLogVerbose
	// Verbose - show log for SPOUT_LOG_VERBOSE or above
	void SpoutLogVerbose(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_VERBOSE, format, args);
		va_end(args);
	}

	// ---------------------------------------------------------
	// Function: SpoutLogNotice
	// Notice - show log for SPOUT_LOG_NOTICE or above
	void SpoutLogNotice(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_NOTICE, format, args);
		va_end(args);
	}

	// ---------------------------------------------------------
	// Function: SpoutLogWarning
	// Warning - show log for SPOUT_LOG_WARNING or above
	void SpoutLogWarning(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		// Show warning text bright yellow
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		_doLog(SPOUT_LOG_WARNING, format, args);
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		va_end(args);
	}

	// ---------------------------------------------------------
	// Function: SpoutLogError
	// Error - show log for SPOUT_LOG_ERROR or above
	void SpoutLogError(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_ERROR, format, args);
		va_end(args);
	}

	// ---------------------------------------------------------
	// Function: SpoutLogFatal
	// Fatal - always show log
	void SpoutLogFatal(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_FATAL, format, args);
		va_end(args);
	}

	// ---------------------------------------------------------
	// Function: _doLog
	// Perform the log
	//
	// Used internally to perform logging. Can also be used externally.
	// The function code can be changed to produce logs as required
	void _doLog(SpoutLogLevel level, const char* format, va_list args)
	{
		if (!format)
			return;

		char currentLog[1024]{}; // allow more than the name length

		// Construct the current log
		vsprintf_s(currentLog, 1024, format, args);

		// Return if logging is paused
		if (!bDoLogs)
			return;

		if (level != SPOUT_LOG_SILENT
			&& CurrentLogLevel != SPOUT_LOG_SILENT
			&& level >= CurrentLogLevel
			&& format != nullptr) {

			// Prevent multiple logs by comparing with the last
			if (strcmp(currentLog, logChars) == 0) {
				// Save the current log as the last
				strcpy_s(logChars, 1024, currentLog);
				return;
			}

			// Save the current log as the last
			strcpy_s(logChars, 1024, currentLog);

			// Console logging
			if (bConsole && bEnableLog) {
				// Yellow text for warnings and errors
				HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
				FILE* out = stdout; // Console output
				if (level == SPOUT_LOG_WARNING || level == SPOUT_LOG_ERROR)
					SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				if (level != SPOUT_LOG_NONE) {
					// Show log level
					fprintf(out, "[%s] ", _levelName(level).c_str());
				}
				// The log
				vfprintf(out, format, args);
				// Newline
				fprintf(out, "\n");
				// Reset white text
				SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
			} // end console log

			// File logging
			if (bEnableLogFile && !logPath.empty()) {
				// Log file output
				// Append to the the current log file so it remains closed
				// No verbose logs for log to file
				if (level != SPOUT_LOG_VERBOSE) {
					logFile.open(logPath, logFile.app);
					if (logFile.is_open()) {
						if (level != SPOUT_LOG_NONE) {
							// Show log level
							logFile << "[" << _levelName(level).c_str()  << "] ";
						}
						// The log and newline
						logFile << currentLog << std::endl;
					}
					logFile.close();
				}
			} // end file log
		}
	}

	// ---------------------------------------------------------
	// Function: _conprint
	// Print to console - (printf replacement).  
	//
	int _conprint(const char* format, ...)
	{

		// Construct the message
		va_list args;
		va_start(args, format);
		vsprintf_s(logChars, 1024, format, args);
		va_end(args);

		//
		// Write to the console without line feed
		//
		// cout and printf do not write if another console is opened by the application.
		// WriteFile writes to either of them.
		//
		DWORD nBytesWritten = 0;
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), logChars, (DWORD)strlen(logChars), &nBytesWritten, NULL);

		logChars[0]=0;
		return (int)nBytesWritten;
	}



	//
	// Group: MessageBox
	//

	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// MessageBox dialog with optional timeout.
	// The dialog closes itself if a timeout is specified.
	int SpoutMessageBox(const char* message, DWORD dwMilliseconds)
	{
		if (!message)
			return 0;
		
		return MessageTaskDialog(NULL, message, "Message", MB_OK, dwMilliseconds);

	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// MessageBox with variable arguments
	int SpoutMessageBox(const char* caption, const char* format, ...)
	{
		std::string strmessage;
		std::string strcaption;

		// Construct the message
		va_list args;
		va_start(args, format);
		vsprintf_s(logChars, 1024, format, args);
		strmessage = logChars;
		va_end(args);

		if (caption && *caption)
			strcaption = caption;
		else
			strcaption = "Message";

		return MessageTaskDialog(NULL, strmessage.c_str(), strcaption.c_str(), MB_OK, 0);
	
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// MessageBox with variable arguments and icon, buttons
	int SPOUT_DLLEXP SpoutMessageBox(const char* caption, UINT uType, const char* format, ...)
	{
		std::string strmessage;
		std::string strcaption;

		// Construct the message
		va_list args;
		va_start(args, format);
		vsprintf_s(logChars, 1024, format, args);
		strmessage = logChars;
		va_end(args);

		if (caption && *caption)
			strcaption = caption;
		else
			strcaption = "Message";

		return MessageTaskDialog(NULL, strmessage.c_str(), strcaption.c_str(), uType, 0);
	}


	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// Messagebox with standard arguments and optional timeout
	// Replaces an existing MessageBox call.
	int SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, DWORD dwMilliseconds)
	{
		return MessageTaskDialog(hwnd, message, caption, uType, dwMilliseconds);
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// MessageBox dialog with standard arguments
	// including taskdialog main instruction large text
	int SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, const char* instruction, DWORD dwMilliseconds)
	{
		// Set global main instruction
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, instruction, (int)strlen(instruction), NULL, 0);
		wstrInstruction.resize(size_needed);
		MultiByteToWideChar(CP_UTF8, 0, instruction, (int)strlen(instruction), &wstrInstruction[0], size_needed);

		return MessageTaskDialog(hwnd, message, caption, uType, dwMilliseconds);

	}
	
	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// MessageBox dialog with edit control for text input
	// Can be used in place of a specific application resource dialog
	//   o For message content, the control is in the footer area
	//   o If no message, the control is in the main content area
	//   o All SpoutMessageBox functions such as user icon and buttons are available
	int SPOUT_DLLEXP SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, std::string& text)
	{
		// For edit control creation
		bEdit = true;

		// A timeout value of 1000000 signals the Taskdialog callback of message content.
		// The dialog times out after 1000 seconds but is effectively modal.
		DWORD dwTimeout = 0;
		std::string content = "";
		if (message && *message) {
			dwTimeout = 1000000;
			content = message;
		}

		// Set initial text for edit control
		stredit = text;
		int iret = MessageTaskDialog(hwnd, content.c_str(), caption, uType, dwTimeout);
		// Get text from global edit control string
		if (iret == IDOK) text = stredit;
		stredit.clear();
		bEdit = false;
		return iret;
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBox
	// MessageBox dialog with a combobox control for item selection
	// Can be used in place of a specific application resource dialog
	// Properties the same as the edit control
	int SPOUT_DLLEXP SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType,
		std::vector<std::string> items, int& index)
	{
		// For combobox creation
		bCombo = true;

		// Timeout value to signal the Taskdialog callback of message content.
		DWORD dwTimeout = 0;
		std::string content = "";
		if (message && *message) {
			dwTimeout = 1000000;
			content = message;
		}

		// Set taskdialog combo box items vector and selected index
		comboitems = items;
		comboindex = index;
		int iret = MessageTaskDialog(hwnd, content.c_str(), caption, uType, dwTimeout);
		if (iret == IDOK) index = comboindex;
		comboitems.clear();
		comboindex = 0;
		bCombo = false;
		return iret;

	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBoxIcon
	// Custom icon for SpoutMessageBox from resources
	// Use together with MB_USERICON
	void SPOUT_DLLEXP SpoutMessageBoxIcon(HICON hIcon)
	{
		hTaskIcon = hIcon;
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBoxIcon
	// Custom icon for SpoutMessageBox from file
	// Use together with MB_USERICON
	bool SPOUT_DLLEXP SpoutMessageBoxIcon(std::string iconfile)
	{
		hTaskIcon = reinterpret_cast<HICON>(LoadImageA(nullptr, iconfile.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
		return (hTaskIcon != nullptr);
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBoxButton
	// Custom button for SpoutMessageBox
	void SPOUT_DLLEXP SpoutMessageBoxButton(int ID, std::wstring title)
	{
		TDbuttonID.push_back(ID);
		TDbuttonTitle.push_back(title);
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBoxModeless
	// Enable modeless functionality using SpoutPanel.exe
	// Used where a Windows MessageBox would interfere with the application GUI.
	// Depends on SpoutPanel.exe version 2.072 or greater distributed with Spout release.
	bool SPOUT_DLLEXP SpoutMessageBoxModeless(bool bMode)
	{
		// If setting modeless, find the path for SpoutPanel.exe
		if (bMode) {
			std::string errmsg;
			char path[MAX_PATH]{};
			if (ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "InstallPath", path)) {
				// Does SpoutPanel.exe exist in this path ?
				if (_access(path, 0) != -1) {
					// Get the version of SpoutPanel
					std::string version = GetExeVersion(path);
					// Check that the version supports Modeless mode
					// (> 2.072 - for example 2.076)
					double fvers = atof(version.c_str());
					if (fvers >= 2.072) {
						// Set modeless
						bModeless = bMode;
						return true;
					}
					sprintf_s(path, MAX_PATH, "SpoutPanel version %s\n2.070 or greater required for modeless\n\n", version.c_str());
					errmsg = path;
				}
				else {
					SpoutLogWarning("SpoutMessageBoxModeless - SpoutPanel.exe not found");
					errmsg = "SpoutPanel.exe not found\n\n";
				}
			}
			else {
				SpoutLogWarning("SpoutMessageBoxModeless - SpoutPanel path not found");
				errmsg = "SpoutPanel path not found\n\n";
			}
			// Show a modal SpoutMessageBox and direct to the Spout releases page
			errmsg += "Download the <a href=\"https://github.com/leadedge/Spout2/releases\">latest Spout release</a>.\n";
			errmsg += "Run either 'SpoutSettings' or 'SpoutPanel' once.\n";
			errmsg += "This will establish the path to SpoutPanel.exe\n";
			errmsg += "to enable modeless function for SpoutMessageBox.\n\n\n";
			bool oldmode = bModeless;
			bModeless = false;
			SpoutMessageBox(NULL, errmsg.c_str(), "SpoutMessageBoxModeless - Warning", MB_ICONWARNING | MB_OK);
			bModeless = oldmode;
			return false;
		}

		// Disable modeless
		bModeless = bMode;

		return true;
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBoxWindow
	// Window handle for SpoutMessageBox where not specified
	void SPOUT_DLLEXP SpoutMessageBoxWindow(HWND hWnd)
	{
		hwndMain = hWnd;
	}

	// ---------------------------------------------------------
	// Function: SpoutMessageBoxPosition
	// Position to centre SpoutMessageBox
	void SPOUT_DLLEXP SpoutMessageBoxPosition(POINT pt)
	{
		TDcentre = pt;
	}

	// ---------------------------------------------------------
	// Function: CopyToClipBoard
	// Copy text to the clipboard
	bool CopyToClipBoard(HWND hwnd, const char* text)
	{
		HGLOBAL clipbuffer = NULL;
		char* buffer = nullptr;
		bool bret = false;

		if (text[0] && strlen(text) > 16) {
			if (OpenClipboard(hwnd)) {
				EmptyClipboard();
				size_t len = (strlen(text) + 1) * sizeof(char);
				// Use GMEM_MOVEABLE instead of GMEM_DDESHARE to avoid crash on repeat
				// GlobalUnlock then returns false but ignore
				clipbuffer = GlobalAlloc(GMEM_MOVEABLE, len); // No crash but GlobalUnlock fails
				if (clipbuffer) {
					buffer = (char*)GlobalLock(clipbuffer);
					if (buffer) {
						memcpy((void *)buffer, (void *)text, len);
						SetClipboardData(CF_TEXT, clipbuffer);
						bret = true;
					}
					GlobalUnlock(clipbuffer);
					GlobalFree(clipbuffer);
					clipbuffer = nullptr;
					buffer = nullptr;
				}
				CloseClipboard();
			}
		}
		return bret;
	}

	// ---------------------------------------------------------
	// Function: OpenSpoutLogs
	// Open Spout log folder in Windows explorer
	bool OpenSpoutLogs()
	{
		// Retrieve Spout log path
		std::string logfolder = _getLogPath();
		if (!logfolder.empty()) {
			if (_access(logfolder.c_str(), 0) != -1) {
				// Open log folder in explorer
				ShellExecuteA(NULL, "open", logfolder.c_str(), NULL, NULL, SW_SHOWNORMAL);
				do { } while (!FindWindowA("CabinetWClass", NULL));
			}
		}
		else {
			SpoutMessageBox(NULL, "Could not create AppData path", "OpenSpoutLogs", MB_OK | MB_TOPMOST | MB_ICONWARNING);
			return false;
		}
		return true;
	}


	//
	// Group: Registry utilities
	//

	// Registry functions new for 2.007 including hKey and changed argument order

	// ---------------------------------------------------------
	// Function: ReadDwordFromRegistry
	// Read subkey DWORD value
	bool ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* pValue)
	{
		if (!subkey || !*subkey || !valuename || !*valuename || !pValue)
			return false;
	
		DWORD dwKey = 0;
		DWORD dwSize = sizeof(DWORD);
		const LONG regres = RegGetValueA(hKey, subkey, valuename, RRF_RT_REG_DWORD, &dwKey, pValue, &dwSize);
		
		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("ReadDwordFromRegistry - could not read [%s] from registry", valuename);
			return false;
		}

		return true;

	}

	// ---------------------------------------------------------
	// Function: WriteDwordToRegistry
	// Write subkey DWORD value
	bool WriteDwordToRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD dwValue)
	{
		if (!subkey || !*subkey || !valuename || !*valuename)
			return false;

		HKEY hRegKey = NULL;
		// Does the key already exist ?
		LONG regres = RegOpenKeyExA(hKey, subkey, 0, KEY_ALL_ACCESS, &hRegKey);
		if (regres != ERROR_SUCCESS) {
			// Create a new key
			regres = RegCreateKeyExA(hKey, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
		}

		if (regres == ERROR_SUCCESS && hRegKey != NULL) {
			// Write the DWORD value
			regres = RegSetValueExA(hRegKey, valuename, 0, REG_DWORD, (BYTE *)&dwValue, 4);
			// For immediate read after write - necessary here because the app might set the values 
			// and read the registry straight away and it might not be available yet.
			// The key must have been opened with the KEY_QUERY_VALUE access right 
			// (included in KEY_ALL_ACCESS)
			RegFlushKey(hRegKey); // needs an open key
			RegCloseKey(hRegKey); // Done with the key
		}

		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("WriteDwordToRegistry - could not write [%s] to registry", valuename);
			return false;
		}

		return true;
	}

	// ---------------------------------------------------------
	// Function: ReadPathFromRegistry
	// Read subkey character string
	bool ReadPathFromRegistry(HKEY hKey, const char* subkey, const char* valuename, char* filepath, DWORD dwSize)
	{
		// Valuename can be null for the (Default) key string
		if (!subkey || !*subkey || !filepath)
			return false;

		HKEY  hRegKey = NULL;
		LONG  regres = 0;
		DWORD dwKey = 0;
		DWORD dwSizePath = dwSize;

		// Does the key exist
		regres = RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &hRegKey);
		if (regres == ERROR_SUCCESS) {
			// Read the key Filepath value
			regres = RegQueryValueExA(hRegKey, valuename, NULL, &dwKey, (BYTE*)filepath, &dwSizePath);
			RegCloseKey(hRegKey);
			if (regres == ERROR_SUCCESS) {
				return true;
			}
			if (regres == ERROR_MORE_DATA) {
				SpoutLogWarning("ReadPathFromRegistry -  buffer size (%d) not large enough (%d)", dwSize, dwSizePath);
			}
			else {
				SpoutLogWarning("ReadPathFromRegistry - could not read [%s] from registry", valuename);
			}
		}

		// Quit if the key does not exist
		return false;

	}
	
	// ---------------------------------------------------------
	// Function: WritePathToRegistry
	// Write subkey character string
	bool WritePathToRegistry(HKEY hKey, const char* subkey, const char* valuename, const char* filepath)
	{
		if (!subkey || !*subkey || !valuename || !*valuename || !filepath)
			return false;

		HKEY  hRegKey = NULL;
		const DWORD dwSize = (DWORD)strlen(filepath)+1;

		// Does the key already exist ?
		LONG regres = RegOpenKeyExA(hKey, subkey, 0, KEY_ALL_ACCESS, &hRegKey);
		if (regres != ERROR_SUCCESS) {
			// Create a new key
			regres = RegCreateKeyExA(hKey, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
		}

		if (regres == ERROR_SUCCESS && hRegKey != NULL) {
			// Write the path
			regres = RegSetValueExA(hRegKey, valuename, 0, REG_SZ, (BYTE *)filepath, dwSize);
			RegCloseKey(hRegKey);
		}

		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("WritePathToRegistry - could not write [%s] to registry", valuename);
			return false;
		}

		return true;

	}

	// ---------------------------------------------------------
	// Function: WriteBinaryToRegistry
	// Write subkey binary hex data string
	bool WriteBinaryToRegistry(HKEY hKey, const char* subkey, const char* valuename, const unsigned char* hexdata, DWORD nChars)
	{
		if (!subkey || !*subkey || !valuename || !*valuename || !hexdata)
			return false;

		HKEY  hRegKey = NULL;
		// Does the key already exist ?
		LONG regres = RegOpenKeyExA(hKey, subkey, 0, KEY_ALL_ACCESS, &hRegKey);
		if (regres != ERROR_SUCCESS) {
			// Create a new key
			regres = RegCreateKeyExA(hKey, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
		}

		if (regres == ERROR_SUCCESS && hRegKey != NULL) {
			regres = RegSetValueExA(hRegKey, valuename, 0, REG_BINARY, hexdata, nChars);
			RegCloseKey(hRegKey);
		}

		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("WriteBinaryToRegistry - could not write to registry");
			return false;
		}

		return true;

	}


	// ---------------------------------------------------------
	// Function: RemovePathFromRegistry
	// Remove subkey value name
	bool RemovePathFromRegistry(HKEY hKey, const char* subkey, const char* valuename)
	{
		if (!subkey || !*subkey || !valuename) {
			SpoutLogWarning("RemovePathFromRegistry - no subkey specified");
			return false;
		}

		HKEY hRegKey = NULL;
		LONG regres = RegOpenKeyExA(hKey, subkey, 0, KEY_ALL_ACCESS, &hRegKey);
		if (regres == ERROR_SUCCESS) {
			regres = RegDeleteValueA(hRegKey, valuename);
			RegCloseKey(hRegKey);
		}

		if (regres == ERROR_SUCCESS)
			return true;

		// Quit if the key does not exist
		SpoutLogWarning("RemovePathFromRegistry - could not open key [%s]", subkey);
		return false;
	}

	// ---------------------------------------------------------
	// Function: RemoveSubKey
	// Delete a subkey and its values.
	//
	// The "subkey" argument must be a subkey of the key that hKey identifies,
	// but it cannot have subkeys.
	//
	// Note that key names are not case sensitive.
	//
	bool RemoveSubKey(HKEY hKey, const char* subkey)
	{
		if (!subkey || !*subkey)
			return false;

		const LONG lStatus = RegDeleteKeyA(hKey, subkey);
		if (lStatus == ERROR_SUCCESS)
			return true;

		SpoutLogWarning("RemoveSubkey - error #%ld", lStatus);
		return false;
	}

	// ---------------------------------------------------------
	// Function: FindSubKey
	// Find subkey
	bool FindSubKey(HKEY hKey, const char* subkey)
	{
		if (!subkey || !*subkey)
			return false;

		HKEY hRegKey = NULL;
		const LONG lStatus = RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &hRegKey);
		if(lStatus == ERROR_SUCCESS) {
			RegCloseKey(hRegKey);
			return true;
		}

		SpoutLogWarning("FindSubkey - error #%ld", lStatus);
		return false;

	}

	//
	// Group: Timing
	//
	// Compiler dependent
	//

	// ---------------------------------------------------------
	// Function: 
	// Start timing period
	// void StartTiming()

	// ---------------------------------------------------------
	// Function: 
	// Stop timing and return microseconds elapsed.
	//
	// Code console output can be enabled for quick timing tests.
	// double EndTiming()

	// ---------------------------------------------------------
	// Function: ElapsedMicroseconds
	// Microseconds elapsed since epoch
	// Requires std::chrono
	// double ElapsedMicroseconds()

	// ---------------------------------------------------------
	// Function: GetRefreshRate
	// Get system refresh rate
	double GetRefreshRate()
	{
		double frequency = 60.0; // default
		DEVMODE DevMode = {};
		BOOL bResult = true;
		DWORD dwCurrentSettings = 0;
		DevMode.dmSize = sizeof(DEVMODE);
		// Test all the graphics modes
		// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-enumdisplaysettingsa
		while (bResult) {
			bResult = EnumDisplaySettings(NULL, dwCurrentSettings, &DevMode);
			if (bResult)
				frequency = static_cast<double>(DevMode.dmDisplayFrequency);
			dwCurrentSettings++;
		}
		return frequency;
	}

#ifdef USE_CHRONO

	// Start timing period
	void StartTiming() {
		start = std::chrono::steady_clock::now();
	}

	// Stop timing and return milliseconds or microseconds elapsed
	// (microseconds default).
	// Console output can be enabled for quick timing tests.
	double EndTiming(bool microseconds, bool bPrint) {
		end = std::chrono::steady_clock::now();
		double elapsed = 0;
		if (microseconds) {
			elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
			if (bPrint) printf("%.3f microsec\n", elapsed);
		} else {
			elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0);
			if (bPrint) printf("%.3f millisec\n", elapsed);
		}
		return elapsed;
	}

	// Microseconds elapsed since epoch
	double ElapsedMicroseconds()
	{
		const std::chrono::system_clock::time_point timenow = std::chrono::system_clock::now();
		const std::chrono::system_clock::duration duration = timenow.time_since_epoch();
		double timestamp = static_cast<double>(duration.count()); // nsec/100 - duration period is 100 nanoseconds
		return round(timestamp / 10.0); // microseconds
	}
#else
	// Start timing period
	void StartTiming() {
		// startcount = GetCounter();
		StartCounter();
	}

	// Stop timing and return microseconds elapsed.
	// Console output can be enabled for quick timing tests.
	double EndTiming() {
		endcount = GetCounter();
		return (endcount-startcount);
	}
#endif

	// -----------------------------------------------
	// Set counter start
	// Used instead of std::chrono for Visual Studio before VS2015
	//
	// Information on using QueryPerformanceFrequency for timing
	// https://docs.microsoft.com/en-us/windows/desktop/SysInfo/acquiring-high-resolution-time-stamps
	//
	void StartCounter()
	{
		LARGE_INTEGER li;
		if (QueryPerformanceFrequency(&li)) {
			// Find the PC frequency if not done yet
			if (PCFreq < 0.0001)
				PCFreq = static_cast<double>(li.QuadPart) / 1000.0;
			// Get the counter start
			QueryPerformanceCounter(&li);
			CounterStart = li.QuadPart;
		}
	}

	// -----------------------------------------------
	// Return msec elapsed since counter start
	double GetCounter()
	{
		LARGE_INTEGER li;
		if (QueryPerformanceCounter(&li)) {
			return static_cast<double>(li.QuadPart - CounterStart) / PCFreq;
		}
		else {
			return 0.0;
		}
	}

	//
	// Private functions
	//
	namespace
	{
		// Log to file with optional append 
		void _logtofile(bool append)
		{
			bool bNewFile = true;

			// Set default log file if not specified
			// C:\Users\username\AppData\Roaming\Spout\SpoutLog.log
			if (logPath.empty()) {
				logPath = _getLogPath();
				logPath += "\\SpoutLog.log";
			}

			// Check for existence before file open
			if (_access(logPath.c_str(), 0) != -1) {
				bNewFile = false; // File exists already
			}

			// File is created if it does not exist
			if (append) {
				logFile.open(logPath, logFile.app);
			}
			else {
				logFile.open(logPath);
			}

			if (logFile.is_open()) {
				// Date and time to identify the log
				char tmp[128]{};
				time_t datime;
				struct tm tmbuff;
				time(&datime);
				localtime_s(&tmbuff, &datime);
				sprintf_s(tmp, 128, "%4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
					tmbuff.tm_year+1900, tmbuff.tm_mon+1, tmbuff.tm_mday,
					tmbuff.tm_hour, tmbuff.tm_min, tmbuff.tm_sec);

				if (append && !bNewFile) {
					logFile << "   " << tmp << std::endl;
				}
				else {
					logFile << "========================================" << std::endl;
					logFile << " Spout log file ";
					size_t bs = logPath.find_last_of("\\");
					if(bs == std::string::npos)
						bs = logPath.find_last_of("/");
					if (bs != std::string::npos) {
						std::string logName = "\"" + logPath.substr(bs+1) + "\"";
						logFile << logName.c_str();
					}
					logFile << std::endl;
					logFile << "========================================" << std::endl;
					logFile << " " << tmp << std::endl;
				}
				logFile.close();
			}
			else {
				// disable file writes and use a console instead
				logPath.clear();
			}
		}

		// Get the default log file path
		std::string _getLogPath()
		{
			char logpath[MAX_PATH]{};
			logpath[0] = 0;

			// Retrieve user %appdata% environment variable
			char* appdatapath = nullptr;
			bool bSuccess = true;
			errno_t err = 0;
			#if defined(_MSC_VER)
				err = _dupenv_s(&appdatapath, NULL, "APPDATA");
			#else
				appdatapath = getenv("APPDATA");
			#endif
			if (err == 0 && appdatapath) {
				strcpy_s(logpath, MAX_PATH, appdatapath);
				strcat_s(logpath, MAX_PATH, "\\Spout");
				if (_access(logpath, 0) == -1) {
					if (!CreateDirectoryA(logpath, NULL)) {
						bSuccess = false;
					}
				}
			}
			else {
				bSuccess = false;
			}

			if (!bSuccess) {
				// _dupenv_s or CreateDirectory failed
				// Find the path of the executable
				sprintf_s(logpath, MAX_PATH, GetExePath().c_str());
			}

			return logpath;
		}

		// Create a full log file path given the name passed in
		std::string _getLogFilePath(const char* filename)
		{
			std::string logFilePath; // full path of the log file
			std::string path;
			size_t pos;

			//
			// Default log 
			//
			if (!filename) {
				// Use the executable name if no filename was passed in
				// C:\Users\username\AppData\Roaming\Spout\exename.log
				logFilePath = _getLogPath();
				logFilePath += "\\";
				logFilePath += GetExeName();
				logFilePath += ".log";
				return logFilePath;
			}
			else {
				path = filename;
				//
				// Path without a filename
				//
				pos = path.rfind("\\");
				if (pos == std::string::npos)
					pos = path.rfind("/");
				if (pos != std::string::npos && pos == path.size() - 1) {
					// Path terminates with a backslash
					pos = path.find(":");
					if (pos != std::string::npos) {
						// Path has a drive letter
						// Add the executable name
						path += GetExeName();
						path += ".log";
						return path;
					}
				}
				//
				// Filename without a path
				//
				pos = path.rfind("\\");
				if (pos == std::string::npos)
					pos = path.rfind("/");
				if (pos == std::string::npos) {
					// Does not have any backslash
					pos = path.find(":");
					if (pos == std::string::npos) {
						// Does not have a drive letter
						// Add an extension if not supplied
						pos = path.rfind(".");
						if (pos == std::string::npos)
							path += ".log";
						logFilePath = _getLogPath();
						logFilePath += "\\";
						logFilePath += path;
						return logFilePath;
					}
				}
				//
				// Path with a filename
				//
				pos = path.rfind("\\");
				if (pos == std::string::npos)
					pos = path.rfind("/");
				if (pos != std::string::npos) {
					// Has any backslash
					pos = path.find(":");
					if (pos != std::string::npos) {
						// Has a drive letter
						// Add an extension if none supplied
						pos = path.rfind(".");
						if (pos == std::string::npos)
							path += ".log";
						return path;
					}
				}
			}
			// If all options fail "SpoutLog.log" is used
			return "SpoutLog.log";
		}

		// Get the name for the current log level
		std::string _levelName(SpoutLogLevel level) {

			std::string name = "";

			switch (level) {
				case SPOUT_LOG_SILENT:
					name = "silent";
					break;
				case SPOUT_LOG_VERBOSE:
					name = "verbose";
					break;
				case SPOUT_LOG_NOTICE:
					name = "notice";
					break;
				case SPOUT_LOG_WARNING:
					name = "warning";
					break;
				case SPOUT_LOG_ERROR:
					name = "error";
					break;
				case SPOUT_LOG_FATAL:
					name = "fatal";
					break;
				default:
					break;
			}

			return name;
		}


		//
		// MessageBox replacement
		// 
		// https://learn.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-taskdialogconfig
		//
		int MessageTaskDialog(HWND hWnd, const char* content, const char* caption, DWORD dwButtons, DWORD dwMilliseconds)
		{
			//
			// TaskDialogIndirect is modal and stops the application.
			// When used within a plugin or similar this can freeze the host application.
			// To avoid this, the message can be passed on to "SpoutPanel" which then calls 
			// SpoutMessageBox and because the dialog is part of a separate application
			// it is effectively modeless and does not affect the calling application
			//
			// Activate or de-activate modeless using :
			//    SpoutMessageBoxModeless(bool bMode);
			//   
			// This option is also suitable for compilers other than Visual Studio
			// that do not support TaskDialogIndirect or do not have control over
			// using the required Version 6 of Commctrl32.dll.
			//
			// Bypass modeless if :
			//   1) The application has not called SoutMessageBoxModeless
			//   2) Any dialog is requested that requires user input
			//
			if (bModeless
				&& TDbuttonID.size() == 0
				&& (dwButtons & MB_OKCANCEL)    != MB_OKCANCEL
				&& (dwButtons & MB_YESNO)       != MB_YESNO
				&& (dwButtons & MB_YESNOCANCEL) != MB_YESNOCANCEL) {
				// Construct command line for SpoutPanel
				std::string str = std::to_string(PtrToUint(hwndMain));
				str += ",";                                        // HWND
				str += content; str += ",";                        // content
				str += caption; str += ",";                        // caption
				str += std::to_string(dwButtons); str += ",";      // buttons
				str += std::to_string(dwMilliseconds); str += ","; // timeout
				if (hTaskIcon) {
					str += std::to_string(PtrToUint(hTaskIcon));   // user icon
					str += ",";
				}

				// Pass on to SpoutPanel
				bool bRet = OpenSpoutPanel(str.c_str());

				// Disable modeless mode so it is one-off
				bModeless = false;
				return bRet;

			}

			// Return if TaskDialog is already open
			if (hwndTask) {
				return 0;
			}

			// Window handle is HWND passed in or specified by SpoutMessageBoxWindow
			if (hwndMain)
				hWnd = hwndMain;

			// hinstance of the window
			HINSTANCE hInst = nullptr;
			if (hWnd) hInst = (HINSTANCE)GetWindowLongPtrA(hWnd, GWLP_HINSTANCE);

			// Use a custom icon if set
			if (hTaskIcon) dwButtons |= MB_USERICON;

			//
			// Drop through for modal TaskDialogIndirect
			// or MessageBox for compilers other than Visual Studio
			//

#ifdef _MSC_VER

			//
			// Visual Studio TaskDialogIndirect
			//

			// User buttons
			TASKDIALOG_BUTTON buttons[10]{};

			// Use a wide string to avoid a pre-sized buffer
			std::wstring wstrTemp;
			if (content) {
				int size_needed = MultiByteToWideChar(CP_UTF8, 0, content, (int)strlen(content), NULL, 0);
				wstrTemp.resize(size_needed);
				MultiByteToWideChar(CP_UTF8, 0, content, (int)strlen(content), &wstrTemp[0], size_needed);
			}


			// Caption (default caption is the executable name)
			std::wstring wstrCaption;
			if (caption) {
				int size_needed = MultiByteToWideChar(CP_UTF8, 0, caption, (int)strlen(caption), NULL, 0);
				wstrCaption.resize(size_needed);
				MultiByteToWideChar(CP_UTF8, 0, caption, (int)strlen(caption), &wstrCaption[0], size_needed);
			}

			// Hyperlinks can be included in the content using HTML format.
			// For example : 
			// <a href=\"https://spout.zeal.co/\">Spout home page</a>
			// Only double quotes are supported and must be escaped.

			// Topmost global flag
			bTopMost = ((dwButtons & MB_TOPMOST) != 0);
			LONG dwl = (LONG)dwButtons;
			if (bTopMost)
				dwl = dwl ^ MB_TOPMOST;

			//
			// Buttons
			//
			DWORD dwb = dwl & 0x0F; // buttons code
			DWORD dwCommonButtons = MB_OK;
			//
			// User buttons
			//
			if (TDbuttonID.size() > 0) {
				int i = 0;
				for (i=0; i<(int)TDbuttonID.size(); i++) {
					buttons[i].nButtonID = TDbuttonID[i];
					buttons[i].pszButtonText = TDbuttonTitle[i].c_str();
				}
				// Final button is OK
				// CANCEL/YES/NO etc have to be added as buttons
				buttons[i].nButtonID = IDOK;
				buttons[i].pszButtonText = L"OK";
			}
			// }
			else {
				//
				// Common buttons
				//
				// https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-taskdialog
				// TDCBF_OK_BUTTON     1
				// TDCBF_YES_BUTTON    2
				// TDCBF_NO_BUTTON     4
				// TDCBF_CANCEL_BUTTON 8
				// MB_OK          0x00
				// MB_OKCANCEL    0x01
				// MB_YESNOCANCEL 0x03
				// MB_YESNO       0x04
				if (dwb == MB_YESNO) { // 4
					dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
				}
				else if (dwb == MB_YESNOCANCEL) { // 3
					dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
				}
				else if (dwb == MB_OKCANCEL) { // 1
					dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
				}
				else {
					dwCommonButtons = MB_OK;
				}
			}

			//
			// Icons
			//
			// Icons available
			// TD_WARNING_ICON, TD_ERROR_ICON, TD_INFORMATION_ICON, TD_SHIELD_ICON
			//
			// Icons to allow for
			// MB_ICONSTOP         0x10
			// MB_ICONERROR        0x10
			// MB_ICONHAND         0x10
			// MB_ICONQUESTION     0x20
			// MB_ICONEXCLAMATION  0x30
			// MB_ICONWARNING      0x30
			// MB_ICONINFORMATION  0x40
			// MB_ICONASTERISK     0x40
			// MB_USERICON         0x80
			//
			HICON hMainIcon = NULL; // No user icon
			WCHAR* wMainIcon = nullptr; // No resource icon
			dwl = dwl & 0xF0; // remove buttons for icons
			if (dwl == MB_USERICON && hTaskIcon) {
				// Private SpoutUtils icon handle set by SpoutMessageBoxIcon
				hMainIcon = hTaskIcon;
				wMainIcon = nullptr;
			}
			else {
				switch (dwl) {
				case MB_ICONINFORMATION: // 0x40
					wMainIcon = TD_INFORMATION_ICON;
					break;
				case MB_ICONWARNING: // 0x30
					wMainIcon = TD_WARNING_ICON;
					break;
				case MB_ICONQUESTION: // 0x20
					wMainIcon = TD_INFORMATION_ICON;
					break;
				case MB_ICONERROR: // 0x10
					wMainIcon = TD_ERROR_ICON;
					break;
				default:
					// No icon specified
					wMainIcon = nullptr;
					break;
				}
			}

			int nButtonPressed        = 0;
			int nRadioButton          = 0;
			TASKDIALOGCONFIG config   = {0};
			config.cbSize             = sizeof(config);
			config.hwndParent         = hWnd;
			config.hInstance          = hInst;
			config.pszWindowTitle     = wstrCaption.c_str();
			config.hMainIcon          = hMainIcon;
			if (!hMainIcon)
				config.pszMainIcon    = wMainIcon; // Important to remove this
			config.pszMainInstruction = wstrInstruction.c_str();
			if (content) {
				config.pszContent         = wstrTemp.c_str();
			}

			// User buttons in TASKDIALOG_BUTTON buttons
			// Otherwise use common buttons
			config.nDefaultButton = IDOK;

			if (TDbuttonID.size() > 0) {
				config.pButtons = buttons;
				config.cButtons = (UINT)TDbuttonID.size()+1; // Includes OK button
			}
			else {
				config.dwCommonButtons = dwCommonButtons;
			}

			config.cxWidth            = 0; // auto width - requires TDF_SIZE_TO_CONTENT
			// TDF_POSITION_RELATIVE_TO_WINDOW Indicates that the task dialog is
			// centered relative to the window specified by hwndParent.
			// If hwndParent is NULL, the dialog is centered on the monitor.
			config.dwFlags  = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_CALLBACK_TIMER | TDF_ENABLE_HYPERLINKS;
			if ((dwButtons & MB_RIGHT) == MB_RIGHT)
				config.dwFlags |= TDF_RTL_LAYOUT;
			
			if (hMainIcon)
				config.dwFlags        |= TDF_USE_HICON_MAIN; // User icon
			config.pfCallback         = reinterpret_cast<PFTASKDIALOGCALLBACK>(TDcallbackProc);
			config.lpCallbackData     = reinterpret_cast<LONG_PTR>(&dwMilliseconds);
	
			if (bTopMost) {
				// Get the first visible window in the Z order
				hwndTop = GetForegroundWindow();
				HWND hwndParent = GetParent(hwndTop); // Is it a dialog
				if (hwndParent) hwndTop = hwndParent;
				// Is it topmost ?
				if ((GetWindowLong(hwndTop, GWL_EXSTYLE) & WS_EX_TOPMOST) > 0) {
					// Move it down
					SetWindowPos(hwndTop, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
				else {
					hwndTop = NULL;
				}
			}

			TaskDialogIndirect(&config, &nButtonPressed, &nRadioButton, NULL);

			if (bTopMost && hwndTop) {
				// Reset the window that was topmost before
				SetWindowPos(hwndTop, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				hwndTop = NULL;
			}

			// Clear global main instruction
			wstrInstruction.clear();

			// Clear custom buttons
			TDbuttonID.clear();
			TDbuttonTitle.clear();

			// Clear custom icon handle set by SpoutMessageBoxIcon and activated by MB_USERICON
			// Use before calling any of the SpoutMessagebox functions
			hTaskIcon = nullptr;

			// Clear dialog user position
			TDcentre.x = 0;
			TDcentre.y = 0;

			// Return button pressed
			// IDCANCEL, IDNO, IDOK, IDRETRY, IDYES
			// or custom button ID
			return nButtonPressed;
#else
			UNREFERENCED_PARAMETER(hInst);
			if(MessageBoxTimeoutA(NULL, content, caption, dwButtons, 0, dwMilliseconds) == 0)
				return MessageBoxA(NULL, content, caption, dwButtons);
			else
				return IDRETRY;
#endif
		}

		HRESULT TDcallbackProc(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
		{
#ifdef _MSC_VER
			
			if (uNotification == TDN_CREATED) {

				// Taskdialog window open
				hwndTask = hwnd;

				// For general use
				RECT rect{};
				int x, y, w, h = 0;

				// hInstance of task dialog
				HINSTANCE hInstTD = (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE);

				// Timeout
				DWORD* pTimeout = reinterpret_cast<DWORD*>(dwRefData); // = tc.lpCallbackData

				// Remove icons from the caption
				// An icon appears in the caption when using MB_OKCANCEL
				// or when an icon is set for the taskdialog content
				SendMessage(hwnd, WM_SETICON, ICON_BIG, NULL);
				SendMessage(hwnd, WM_SETICON, ICON_SMALL, NULL);

				// Dialog Window size and position
				GetWindowRect(hwnd, &rect);
				x = rect.left;
				y = rect.top;
				w = rect.right-rect.left;
				h = rect.bottom - rect.top;

				// Centre the taskdialog window on the point
				// if SpoutMessageBoxPosition has been used
				if (TDcentre.x > 0 || TDcentre.y > 0) {
					// Offset to the centre of the window
					x = TDcentre.x - (w/2);
					y = TDcentre.y - (h/2);
				}

				if (bTopMost)
					SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOSIZE);
				else
					SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, w, h, SWP_NOSIZE);

				// Edit text control
				if (bEdit) {

					// Position in the main content by default
					GetClientRect(hwnd, &rect);

					// Taskdialog client size is larger with an icon
					h = rect.bottom-rect.top;
					x = rect.left+70;
					y = rect.top + 3;
					// Allow for increased height with an icon
					if (h > 90) y += 20;
					w = 320;
					h = 24;

					// Look for a timeout of 1000000 as a signal for message content
					// and position in the the footer area if so.
					if (*pTimeout && *pTimeout == 1000000) {
						x = rect.left+10;
						y = rect.bottom-38;
						w = 220;
					}

					hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
						WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
						x, y, w, h, hwnd, (HMENU)IDC_TASK_EDIT, hInstTD, NULL);

					// Set an initial entry in the edit box
					if (!stredit.empty()) {
						SetWindowTextA(hEdit, (LPCSTR)stredit.c_str());
						// Select all text in the edit field
						SendMessage(hEdit, EM_SETSEL, 0, 0x7FFF0000L);
					}

					// Position on top of content
					BringWindowToTop(hEdit);

					// Set keyboard focus to allow user entry
					SetFocus(hEdit);

				}

				// Combo box control
				if (bCombo) {

					// Position in the main content by default
					GetClientRect(hwnd, &rect);

					// Taskdialog client size
					// h = rect.bottom-rect.top;
					h = rect.bottom - rect.top;
					x = rect.left+20;
					y = rect.top;
					w = 395;

					// Find combo box width from the longest item string
					LONG maxw = 0L;
					if (comboitems.size() > 0) {
						HDC hdc = GetDC(hwnd);
						HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
						SelectObject(hdc, hFont);
						for (int i = 0; i < (int)comboitems.size(); i++) {
							// Text width for the current string
							SIZE size;
							GetTextExtentPoint32A(hdc, (LPCSTR)comboitems[i].c_str(), (int)comboitems[i].size(), &size);
							if (size.cx > maxw)
								maxw = size.cx;
        				}
						ReleaseDC(hwnd, hdc);
						// Add padding for the combo box button
						maxw += 35;
					}

					// Adjust to the maximum width required
					w = 0;
					if(maxw > 0)
						w = (int)maxw;
					if(w < 200) w = 200; // Minimum combo width
					int dw = rect.right-rect.left;
					if (w < dw) {
						// If the width is less than the dialog adjust the x position 
						x = (dw-w)/2;
						if (*pTimeout && *pTimeout == 1000000) {
							// Position in the footer area if there is message content
							// Less width due to buttons
							x = rect.left+10;
							y = rect.bottom-40;
							w = 220; // Fixed width
						}
						else if (h > 90) { // Increased client size for icon
							// Allow for increased height and position further right
							y += 20;
							if (x < 20) {
								x += 40;
								w -= 40;
							}
						}
					}
					else {
						// If the width is larger, reduce to fit the dialog
						w = dw-4;
						x = 2;
					}

					// Combo box inital height. Changed by content.
					h = 100;

					// Use CBS_DROPDOWNLIST style for list only
					hCombo = CreateWindowExA(WS_EX_CLIENTEDGE, "COMBOBOX", "",
						CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
						x, y, w, h, hwnd, (HMENU)IDC_TASK_COMBO, hInstTD, NULL);

					// Add combo box items
					if (comboitems.size() > 0) {
						for (int i = 0; i<(int)comboitems.size(); i++) {
							SendMessageA(hCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)comboitems[i].c_str());
						}
						// Display an initial item in the selection field
						SendMessageA(hCombo, CB_SETCURSEL, (WPARAM)comboindex, (LPARAM)0);
					}

					// Remove icons from the caption
					SendMessage(hwnd, WM_SETICON, ICON_BIG, NULL);
					SendMessage(hwnd, WM_SETICON, ICON_SMALL, NULL);

					// Position on top of content
					BringWindowToTop(hCombo);

				}

			}

			if (uNotification == TDN_DESTROYED) {

				// Taskdialog window closed
				hwndTask = nullptr;

				if (bEdit) {
					// Get text from edit control
					char text[MAX_PATH]{};
					GetWindowTextA(hEdit, text, MAX_PATH);
					// Move to global string for return
					stredit = text;
				}

				if (bCombo) {
					// Get currently selected index
					// Allow for error if the user edits the list item
					int index = (int)SendMessageA(hCombo, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					if (index != CB_ERR) {
						comboindex = index;
					}
				}
			}

			// Timeout
			if (uNotification == TDN_TIMER)
			{
				DWORD* pTimeout = reinterpret_cast<DWORD*>(dwRefData);  // = tc.lpCallbackData
				DWORD timeElapsed = static_cast<DWORD>(wParam);
				if (*pTimeout && timeElapsed >= *pTimeout) {
					*pTimeout = 0; // Make sure we don't send the button message multiple times.
					SendMessage(hwnd, TDM_CLICK_BUTTON, IDOK, 0);
				}
			}

			// Hyperlink
			//   TDN_HYPERLINK_CLICKED indicates that a hyperlink has been selected.
			//   lParam - Pointer to a wide-character string containing the URL of the hyperlink.
			if (uNotification == TDN_HYPERLINK_CLICKED) {
				SHELLEXECUTEINFOW sei{};
				sei.cbSize = sizeof(sei);
				sei.hwnd = NULL;
				sei.lpVerb = L"open";
				sei.lpFile = (LPCWSTR)lParam;
				sei.nShow = SW_SHOWNORMAL;
				if (!ShellExecuteExW(&sei)) {
					return S_FALSE;
				}
				SendMessage(hwnd, TDM_CLICK_BUTTON, IDOK, 0);
			}
#endif
			return S_OK;
		}

#ifndef _MSC_VER

		// TimeoutMessageBox replacement for TaskDialogIndirect
		// https://www.codeproject.com/Articles/7914/MessageBoxTimeout-API
		int MessageBoxTimeoutA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption,
			UINT uType, WORD wLanguageId, DWORD dwMilliseconds)
		{
			typedef int(__stdcall* MSGBOXAAPI)(IN HWND hWnd,
				IN LPCSTR lpText, IN LPCSTR lpCaption, IN UINT uType,
				IN WORD wLanguageId, IN DWORD dwMilliseconds);

			static MSGBOXAAPI MsgBoxTOA = NULL;

			if (!MsgBoxTOA) {
				HMODULE hUser32 = GetModuleHandleA("user32.dll");
				if (hUser32) {
					MsgBoxTOA = (MSGBOXAAPI)GetProcAddress(hUser32, "MessageBoxTimeoutA");
				}
				else {
					// Return to call MessageBox())
					return 0;
				}
			}
			if (MsgBoxTOA)
				return MsgBoxTOA(hWnd, lpText, lpCaption, uType, wLanguageId, dwMilliseconds);
			return 0;
		}
#endif

		//---------------------------------------------------------
		// Function: ExecuteProcess
		// Open process using ShellExecuteEx
		bool ExecuteProcess(const char* path, const char* commandline)
		{
			if (!path)
				return false;

			SHELLEXECUTEINFOA ShExecInfo{};
			ZeroMemory(&ShExecInfo, sizeof(ShExecInfo));
			ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.lpFile = (LPCSTR)path;
			if (commandline)
				ShExecInfo.lpParameters = commandline;
			ShExecInfo.nShow = SW_SHOW;

			return (ShellExecuteExA(&ShExecInfo) != FALSE);

		}

		//---------------------------------------------------------
		// Function: OpenSpoutPanel
		//  Open SpoutPanel.exe with command line for
		//  SpoutMessageBox as a modeless dialog
		//  "SpoutMessageBoxModeless" must have been previously called
		bool OpenSpoutPanel(const char* message)
		{
			if (!bModeless || !message || !*message)
				return false;

			// SpoutPanel.exe has already been found by SpoutMessageBoxModeless
			// Get the path again for ExecuteProcess
			char path[MAX_PATH]{};
			if (!ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "InstallPath", path))
				return false;

			bool bRet = false;
			// Check whether SpoutPanel is already running
			HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");
			if (!hMutex) {
				// No mutex, not running so can open it
				std::string cmdline= " ";
				cmdline += message;
				bRet = ExecuteProcess(path, message);
			}

			// Close the mutex now or it is never released
			if (hMutex) CloseHandle(hMutex);

			return bRet;

		} // end OpenSpoutPanel

	} // end private namespace

} // end namespace spoututils
