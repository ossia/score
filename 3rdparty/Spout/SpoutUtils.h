/*

						SpoutUtils.h

					General utility functions

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

*/

#pragma once
#ifndef __spoutUtils__ // standard way as well
#define __spoutUtils__

// Enable this define to use independently of Spout source files
// See also the stand alone define in SpoutGLextensions
// #define standaloneUtils

#ifdef standaloneUtils
#define SPOUT_DLLEXP
#else
// For use together with Spout source files
#include "SpoutCommon.h" // for legacyOpenGL define and Utils
#include <stdint.h> // for _uint32 etc
#endif

#include <windows.h>
#include <stdio.h> // for console
#include <iostream> // std::cout, std::end
#include <fstream> // for log file
#include <time.h> // for time and date
#include <io.h> // for _access
#include <direct.h> // for _getcwd
#include <vector>
#include <string>
#include <shellapi.h> // for shellexecute
#include <commctrl.h> // For TaskDialogIndirect
#include <math.h> // for round
#include <algorithm> // for string character remove

//
// C++11 timer is only available for MS Visual Studio 2015 and above.
//
// Note that _MSC_VER may not correspond correctly if an earlier platform toolset
// is selected for a later compiler e.g. Visual Studio 2010 platform toolset for
// a Visual studio 2017 compiler. "#include <chrono>" will then fail.
// If this is a problem, remove _MSC_VER_ and manually enable/disable the USE_CHRONO define.
//
// PR #84  Fixes for clang
// PR #114  Fixes for MingW
#if (defined(_MSC_VER) && (_MSC_VER >= 1900)) || (defined(__cplusplus) && (__cplusplus >= 201103L))

#define USE_CHRONO
#endif

#ifdef USE_CHRONO
#include <chrono> // c++11 timer
#include <thread>
#endif

#pragma comment(lib, "shell32.lib") // for shellexecute
#pragma comment(lib, "advapi32.lib") // for registry functions
#pragma comment(lib, "version.lib") // for version resources where necessary
#pragma comment(lib, "comctl32.lib") // For taskdialog

// TaskDialog requires comctl32.dll version 6
#ifdef _MSC_VER
// https://learn.microsoft.com/en-us/windows/win32/controls/cookbook-overview
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// SpoutUtils
namespace spoututils {

	enum SpoutLogLevel {
		// Disable all messages
		SPOUT_LOG_SILENT,
		// Show all messages
		SPOUT_LOG_VERBOSE,
		// Show information messages - default
		SPOUT_LOG_NOTICE,
		// Show warning, errors and fatal
		SPOUT_LOG_WARNING,
		// Show errors and fatal
		SPOUT_LOG_ERROR,
		// Show only fatal errors
		SPOUT_LOG_FATAL,
		// Ignore log levels
		SPOUT_LOG_NONE
	};

	//
	// Information
	//

	// Get SDK version number string e.g. "2.007.000"
	// Optional - return as a single number
	// e.g. 2.006 = 2006, 2.007 = 2007, 2.007.009 = 2007009
	std::string SPOUT_DLLEXP GetSDKversion(int* number = nullptr);

	// Get the user Spout version from the registry
	// Optional - return as a single number
	std::string SPOUT_DLLEXP GetSpoutVersion(int* number = nullptr);

	// Computer type
	bool SPOUT_DLLEXP IsLaptop();

	// Get the module handle of an executable or dll
	HMODULE SPOUT_DLLEXP GetCurrentModule();

	// Get executable or dll version
	std::string SPOUT_DLLEXP GetExeVersion(const char* path);

	// Get executable or dll path
	std::string SPOUT_DLLEXP GetExePath(bool bFull = false);

	// Get executable or dll name
	std::string SPOUT_DLLEXP GetExeName();

	// Remove file name and return the path
	std::string SPOUT_DLLEXP GetPath(std::string fullpath);

	// Remove path and return the file name
	std::string SPOUT_DLLEXP GetName(std::string fullpath);

	//
	// Console management
	//

	// Open console window.
	// A console window opens without logs.
	// Useful for debugging with console output.
	void SPOUT_DLLEXP OpenSpoutConsole(const char* title = nullptr);
	
	// Close console window.
	// The optional warning displays a MessageBox if user notification is required.
	void SPOUT_DLLEXP CloseSpoutConsole(bool bWarning = false);

	// Enable logging to the console.
	// Logs are displayed in a console window.  
	// Useful for program development.
	void SPOUT_DLLEXP EnableSpoutLog(const char* title = nullptr);

	// Enable logging to a file with optional append.
	// As well as a console window, you can output logs to a text file. 
	// Default extension is ".log" unless the full path is used.
	// For no file name or path the executable name is used.
	//     Example : EnableSpoutLogFile("Sender.log");
	// The log file is re-created every time the application starts
	// unless you specify to append to the existing one.  
	//    Example : EnableSpoutLogFile("Sender.log", true);
	// The file is saved in the %AppData% folder unless you specify the full path :  
	//    C:>Users>username>AppData>Roaming>Spout   
	// You can find and examine the log file after the application has run.
	void SPOUT_DLLEXP EnableSpoutLogFile(const char* filename = nullptr, bool bAppend = false);

	// Disable logging to file
	void SPOUT_DLLEXP DisableSpoutLogFile();

	// Remove a log file
	void SPOUT_DLLEXP RemoveSpoutLogFile(const char* filename = nullptr);

	// Disable logging to console and file
	void SPOUT_DLLEXP DisableSpoutLog();

	// Disable logging temporarily
	void SPOUT_DLLEXP DisableLogs();
	
	// Enable logging again
	void SPOUT_DLLEXP EnableLogs();

	// Are console logs enabled
	bool SPOUT_DLLEXP LogsEnabled();

	// Is file logging enabled
	bool SPOUT_DLLEXP LogFileEnabled();

	// Return the full log file path
	std::string SPOUT_DLLEXP GetSpoutLogPath();

	// Return the log file as a string
	std::string SPOUT_DLLEXP GetSpoutLog(const char* filepath = nullptr);

	// Show the log file folder in Windows Explorer
	void SPOUT_DLLEXP ShowSpoutLogs();
	
	// Set the current log level
	void SPOUT_DLLEXP SetSpoutLogLevel(SpoutLogLevel level);
	
	// General purpose log
	void SPOUT_DLLEXP SpoutLog(const char* format, ...);
	
	// Verbose - show log for SPOUT_LOG_VERBOSE or above
	void SPOUT_DLLEXP SpoutLogVerbose(const char* format, ...);
	
	// Notice - show log for SPOUT_LOG_NOTICE or above
	void SPOUT_DLLEXP SpoutLogNotice(const char* format, ...);
	
	// Warning - show log for SPOUT_LOG_WARNING or above
	void SPOUT_DLLEXP SpoutLogWarning(const char* format, ...);
	
	// Error - show log for SPOUT_LOG_ERROR or above
	void SPOUT_DLLEXP SpoutLogError(const char* format, ...);
	
	// Fatal - always show log
	void SPOUT_DLLEXP SpoutLogFatal(const char* format, ...);

	// Logging function.
	void SPOUT_DLLEXP _doLog(SpoutLogLevel level, const char* format, va_list args);

	// Print to console (printf replacement)
	int SPOUT_DLLEXP _conprint(const char* format, ...);

	//
	// MessageBox dialog
	//

	// MessageBox dialog with optional timeout.
	// The dialog closes itself if a timeout is specified.
	int SPOUT_DLLEXP SpoutMessageBox(const char* message, DWORD dwMilliseconds = 0);

	// MessageBox with variable arguments
	int SPOUT_DLLEXP SpoutMessageBox(const char* caption, const char* format, ...);
	
	// MessageBox with variable arguments and icon, buttons
	int SPOUT_DLLEXP SpoutMessageBox(const char* caption, UINT uType, const char* format, ...);

	// MessageBox dialog with standard arguments.
	// Replaces an existing MessageBox call.
	// uType options : standard MessageBox buttons and icons
	// MB_USERICON - use together with SpoutMessageBoxIcon
	// Hyperlinks can be included in the content using HTML format.
	// For example : <a href=\"https://spout.zeal.co/\">Spout home page</a>
	// Only double quotes are supported and must be escaped.
	int SPOUT_DLLEXP SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, DWORD dwMilliseconds = 0);

	// MessageBox dialog with standard arguments
	// including taskdialog main instruction large text
	int SPOUT_DLLEXP SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption,  UINT uType, const char* instruction, DWORD dwMilliseconds = 0);

	// MessageBox dialog with an edit control for text input
	// Can be used in place of a specific application resource dialog
	//   o For message content, the control is in the footer area
	//   o If no message, the control is in the main content area
	//   o All SpoutMessageBox functions such as user icon and buttons are available
	int SPOUT_DLLEXP SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, std::string& text);

	// MessageBox dialog with a combobox control for item selection
	// Can be used in place of a specific application resource dialog
	// Properties the same as the edit control
	int SPOUT_DLLEXP SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType,
		std::vector<std::string> items, int &selected);

	// Custom icon for SpoutMessageBox from resources
	void SPOUT_DLLEXP SpoutMessageBoxIcon(HICON hIcon);

	// Custom icon for SpoutMessageBox from file
	bool SPOUT_DLLEXP SpoutMessageBoxIcon(std::string iconfile);

	// Custom button for SpoutMessageBox
	void SPOUT_DLLEXP SpoutMessageBoxButton(int ID, std::wstring title);

	// Activate modeless mode using SpoutPanel.exe
	bool SPOUT_DLLEXP SpoutMessageBoxModeless(bool bMode = true);

	// Window handle for SpoutMessageBox where not specified
	void SPOUT_DLLEXP SpoutMessageBoxWindow(HWND hWnd);

	// Position to centre SpoutMessageBox
	void SPOUT_DLLEXP SpoutMessageBoxPosition(POINT pt);

	// Copy text to the clipboard
	bool SPOUT_DLLEXP CopyToClipBoard(HWND hwnd, const char* text);

	// Open logs folder
	bool SPOUT_DLLEXP OpenSpoutLogs();

	//
	// Registry utilities
	//

	// Read subkey DWORD value
	bool SPOUT_DLLEXP ReadDwordFromRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD* pValue);
	
	// Write subkey DWORD value
	bool SPOUT_DLLEXP WriteDwordToRegistry(HKEY hKey, const char* subkey, const char* valuename, DWORD dwValue);
	
	// Read subkey character string
	bool SPOUT_DLLEXP ReadPathFromRegistry(HKEY hKey, const char* subkey, const char* valuename, char* filepath, DWORD dwSize = MAX_PATH);
	
	// Write subkey character string
	bool SPOUT_DLLEXP WritePathToRegistry(HKEY hKey, const char* subkey, const char* valuename, const char* filepath);
	
	// Write subkey binary hex data string
	bool SPOUT_DLLEXP WriteBinaryToRegistry(HKEY hKey, const char* subkey, const char* valuename, const unsigned char* hexdata, DWORD nchars);

	// Remove subkey value name
	bool SPOUT_DLLEXP RemovePathFromRegistry(HKEY hKey, const char* subkey, const char* valuename);
	
	// Delete a subkey and its values.
	//   It must be a subkey of the key that hKey identifies, but it cannot have subkeys.  
	//   Note that key names are not case sensitive.  
	bool SPOUT_DLLEXP RemoveSubKey(HKEY hKey, const char* subkey);
	
	// Find subkey
	bool SPOUT_DLLEXP FindSubKey(HKEY hKey, const char* subkey);

	//
	// Timing functions
	//

	// Monitor refresh rate
	double SPOUT_DLLEXP GetRefreshRate();

	// Start timing period
	void SPOUT_DLLEXP StartTiming();

#ifdef USE_CHRONO
	// Stop timing and return milliseconds or microseconds elapsed.
	// (microseconds default).
	// Code console output can be enabled for quick timing tests.
	double SPOUT_DLLEXP EndTiming(bool microseconds = false, bool bPrint = false);
	// Microseconds elapsed since epoch
	double SPOUT_DLLEXP ElapsedMicroseconds();
#else
	double SPOUT_DLLEXP EndTiming();
#endif

	void SPOUT_DLLEXP StartCounter();
	double SPOUT_DLLEXP GetCounter();

	//
	// Private functions
	//
	namespace
	{
		// Local functions
		void _logtofile(bool append = false);
		std::string _getLogPath();
		std::string _getLogFilePath(const char* filename);
		std::string _levelName(SpoutLogLevel level);
		// Taskdialog for SpoutMessageBox
		int MessageTaskDialog(HWND hWnd, const char* content, const char* caption, DWORD dwButtons, DWORD dwMilliseconds);
		// TaskDialogIndirect callback to handle timer, topmost and hyperlinks
		HRESULT TDcallbackProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData);
#ifndef _MSC_VER
		// Timeout MessageBox for other compilers
		int MessageBoxTimeoutA(IN HWND hWnd,
			IN LPCSTR lpText, IN LPCSTR lpCaption, IN UINT uType,
			IN WORD wLanguageId, IN DWORD dwMilliseconds);
#endif

		// Use ShellExecutEx to open a program
		bool ExecuteProcess(const char* path, const char* command = nullptr);
		// Open SpoutPanel with command line for modeless SpoutMessageBox
		bool OpenSpoutPanel(const char* message);
		// Application window
		HWND hwndMain = NULL;
		// Taskdialog window to prevent multiple open
		HWND hwndTask = NULL;
		// Position for TaskDialog window centre
		POINT TDcentre = {};
		// For topmost
		HWND hwndTop = NULL;
		bool bTopMost = false;
		// Modeless TaskDialog by way of OpenSpoutPanel
		bool bModeless = false; // Default use local TaskDialogIndirect
		// For custom icon
		HICON hTaskIcon = NULL;

		// For custom buttons
		std::vector<int>TDbuttonID;
		std::vector<std::wstring>TDbuttonTitle;

		// Main instruction text
		std::wstring wstrInstruction;

		// For edit text control
		bool bEdit = false;
		HWND hEdit = NULL;
		std::string stredit;
		#define IDC_TASK_EDIT 101

		// For combo box control
		bool bCombo = false;
		HWND hCombo = NULL;
		std::vector<std::string> comboitems;
		int comboindex = 0;
		#define IDC_TASK_COMBO 102

	}

}

#endif
