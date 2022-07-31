/*

						SpoutUtils.h

					General utility functions

		- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		Copyright (c) 2017-2022, Lynn Jarvis. All rights reserved.

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

#include <windows.h>
#include <stdio.h> // for console
#include <iostream> // std::cout, std::end
#include <fstream> // for log file
#include <time.h> // for time and date
#include <io.h> // for _access
#include <vector>
#include <string>
#include <Shellapi.h> // for shellexecute
#include <shlwapi.h> // for path functions

//
// C++11 timer is only available for MS Visual Studio 2015 and above.
//
// Note that _MSC_VER may not correspond correctly if an earlier platform toolset
// is selected for a later compiler e.g. Visual Studio 2010 platform toolset for
// a Visual studio 2017 compiler. "#include <chrono>" will then fail.
// If this is a problem, remove _MSC_VER_ and manually enable/disable the USE_CHRONO define.
//
#if _MSC_VER >= 1900 || defined (__clang__) || (__cplusplus >= 201703L)
#define USE_CHRONO
#endif

#ifdef USE_CHRONO
#include <chrono> // c++11 timer
#include <thread>
#endif

#pragma comment(lib, "Shell32.lib") // for shellexecute
#pragma comment(lib, "shlwapi.lib") // for path functions
#pragma comment(lib, "Advapi32.lib") // for registry functions
#pragma comment(lib, "Version.lib") // for version resources where necessary

// SpoutUtils
namespace spoututils {

	//
	// Log level definitions.
	// The level above which the logs are shown.  
	// For example, to show only warnings and errors (you shouldn't see any):  
	//    SetSpoutLogLevel(SPOUT_LOG_WARNING);
	//
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
		SPOUT_LOG_NONE,
	};

	//
	// Console management
	//

	// Open console window.
	// A console window opens without logs.
	// Useful for debugging with console output.
	void OpenSpoutConsole();
	
	// Close console window.
	// The optional warning displays a MessageBox if user notification is required.
	void CloseSpoutConsole(bool bWarning = false);
	
	// Enable logging to the console.
	// Logs are displayed in a console window.  
	// Useful for program development.
	void EnableSpoutLog();
	
	// Enable logging to a file with optional append.
	// You can instead, or additionally to a console window,  
	// specify output to a text file with the extension of your choice  
	// Example : EnableSpoutLogFile("Sender.log");
	// The log file is re-created every time the application starts unless you specify to append to the existing one :  
	// Example : EnableSpoutLogFile("Sender.log", true);
	// The file is saved in the %AppData% folder unless you specify the full path :  
	//    C:>Users>username>AppData>Roaming>Spout   
	// You can find and examine the log file after the application has run.
	void EnableSpoutLogFile(const char* filename, bool append = false);

	// Disable logging to file
	void DisableSpoutLogFile();
	
	// Disable logging to console and file
	void DisableSpoutLog();

	// Disable logging temporarily
	void DisableLogs();
	
	// Enable logging again
	void EnableLogs();

	// Return the log file as a string
	std::string GetSpoutLog();
	
	// Show the log file folder in Windows Explorer
	void ShowSpoutLogs();
	
	// Set the current log level
	void SetSpoutLogLevel(SpoutLogLevel level);
	
	// General purpose log
	void SpoutLog(const char* format, ...);
	
	// Verbose - show log for SPOUT_LOG_VERBOSE or above
	void SpoutLogVerbose(const char* format, ...);
	
	// Notice - show log for SPOUT_LOG_NOTICE or above
	void SpoutLogNotice(const char* format, ...);
	
	// Warning - show log for SPOUT_LOG_WARNING or above
	void SpoutLogWarning(const char* format, ...);
	
	// Error - show log for SPOUT_LOG_ERROR or above
	void SpoutLogError(const char* format, ...);
	
	// Fatal - always show log
	void SpoutLogFatal(const char* format, ...);
	
	//
	// MessageBox dialog
	//

	// MessageBox dialog with optional timeout.
	// Used where a Windows MessageBox would interfere with the application GUI.  
	// The dialog closes itself if a timeout is specified.
	int SpoutMessageBox(const char * message, DWORD dwMilliseconds = 0);
	
	// MessageBox dialog with standard arguments.
	// Replaces an existing MessageBox call.
	int SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, DWORD dwMilliseconds = 0);
	
	//
	// Registry utilities
	//

	// Read subkey DWORD value
	bool ReadDwordFromRegistry(HKEY hKey, const char *subkey, const char *valuename, DWORD *pValue);
	
	// Write subkey DWORD value
	bool WriteDwordToRegistry(HKEY hKey, const char *subkey, const char *valuename, DWORD dwValue);
	
	// Read subkey character string
	bool ReadPathFromRegistry(HKEY hKey, const char *subkey, const char *valuename, char *filepath);
	
	// Write subkey character string
	bool WritePathToRegistry(HKEY hKey, const char *subkey, const char *valuename, const char *filepath);
	
	// Write subkey binary hex data string
	bool WriteBinaryToRegistry(HKEY hKey, const char *subkey, const char *valuename, const unsigned char *hexdata, DWORD nchars);

	// Remove subkey value name
	bool RemovePathFromRegistry(HKEY hKey, const char *subkey, const char *valuename);
	
	// Delete a subkey and its values.
	//   It must be a subkey of the key that hKey identifies, but it cannot have subkeys.  
	//   Note that key names are not case sensitive.  
	bool RemoveSubKey(HKEY hKey, const char *subkey);
	
	// Find subkey
	bool FindSubKey(HKEY hKey, const char *subkey);

	//
	// Timing functions
	//
#ifdef USE_CHRONO
	// Start timing period
	void StartTiming();
	// Stop timing and return microseconds elapsed.
	// Code console output can be enabled for quick timing tests.
	double EndTiming();
#endif

	// Get SDK version number string e.g. "2.007.000"
	std::string GetSDKversion();

	// Logging function.
	// Used internally to perform logging.  
	// The function code can be changed to produce logs as required
	void _doLog(SpoutLogLevel level, const char* format, va_list args);

	//
	// Private functions
	//
	namespace
	{

		// Local functions
		std::string _getLogPath();
		std::string _levelName(SpoutLogLevel level);
		void _logtofile(bool append = false);

		// Used internally for NVIDIA profile functions
		bool GetNVIDIAmode(const char *command, int * mode);
		bool SetNVIDIAmode(const char *command, int mode);
		bool ExecuteProcess(char *path);
	}


}

#endif
