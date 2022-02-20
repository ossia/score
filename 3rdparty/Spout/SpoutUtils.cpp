/*

			SpoutUtils

			Utility functions

	CREDIT - logging based on Openframeworks ofLog
	https://github.com/openframeworks/openFrameworks/tree/master/libs/openFrameworks/utils

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	Copyright (c) 2017-2021, Lynn Jarvis. All rights reserved.

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

*/
#include "SpoutUtils.h"

//
// Namespace: spoututils
//
// Namespace for utility functions.
//
// - Console
// - Logs
// - MessageBox dialog
// - Registry utilities
//
// Refer to source code for documentation.
//

namespace spoututils {

	// Local variables
	bool bEnableLog = false;
	bool bEnableLogFile = false;
	bool bDoLogs = true;
	SpoutLogLevel CurrentLogLevel = SPOUT_LOG_NOTICE;
	FILE* pCout = NULL; // for log to console
	std::ofstream logFile; // for log to file
	std::string logPath = ""; // path for the logfile
	std::string logFileName = ""; // file name for the logfile
	std::string LastSpoutLog = "";
	bool bConsole = false;
#ifdef USE_CHRONO
	std::chrono::steady_clock::time_point start;
	std::chrono::steady_clock::time_point end;
#endif
	std::string SDKversion = "2.007.004"; // Spout SDK version number string

	//
	// Console management
	//

	void OpenSpoutConsole()
	{
		// AllocConsole fails if the process already has a console
		// Is a console associated with the calling process?
		if (GetConsoleWindow()) {
			bConsole = true;
		}
		else {
			// Get calling process window
			HWND hwndFgnd = GetForegroundWindow();
			if (AllocConsole()) {
				errno_t err = freopen_s(&pCout, "CONOUT$", "w", stdout);
				if (err == 0) {
					SetConsoleTitleA("Spout Log");
					bConsole = true;
					// Disable close button
					// HMENU hmenu = GetSystemMenu(GetConsoleWindow(), FALSE);
					// EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
					// Bring the main window to the top again
					SetWindowPos(hwndFgnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
				else {
					pCout = NULL;
					bConsole = false;
				}
			}
		}
	}
	
	void CloseSpoutConsole(bool bWarning)
	{
		if(bWarning) {
			if(MessageBoxA(NULL, "Console close - are you sure?", "Spout", MB_YESNO) == IDNO)
				return;
		}

		if (pCout) {
			fclose(pCout);
			FreeConsole();
			pCout = NULL;
			bConsole = false;
		}
	}
		
	//
	// Logs
	//

	// Enable log to console
	void EnableSpoutLog()
	{
		bEnableLog = true;

		// Console output
		if(!bConsole)
			OpenSpoutConsole();

	}

	// Enable log to a user file with optional append
	void EnableSpoutLogFile(const char* filename, bool append)
	{
		bEnableLogFile = true;
		if (!logPath.empty()) {
			if (logFile.is_open())
				logFile.close();
			logPath = "";
		}

		// Set the log file name or path
		if (filename[0]) {

			char fname[MAX_PATH];
			strcpy_s(fname, MAX_PATH, filename);
			PathRemoveBackslashA(fname);
			logPath.clear();

			if (PathIsDirectoryA(fname)) {
				// Path without a filename
				logPath = fname;
				logPath += "\\SpoutLog.log";
			}
			else if (PathIsFileSpecA(fname)) {

				// Filename without a path
				// Add an extension if none supplied
				if (!PathFindExtensionA(fname)[0])
					strcat_s(fname, MAX_PATH, ".log");
				logPath = _getLogPath();
				logPath += "\\";
				logPath += fname;

			}
			else if (PathFindFileNameA(fname)) {
				// Full path with a filename
				// Add an extension if none supplied
				if (!PathFindExtensionA(fname)[0])
					strcat_s(fname, MAX_PATH, ".log");
				logPath = fname;
			}
			// logPath is empty if all options fail
		}
		_logtofile(append);
	}

	// Disable logging to file
	void DisableSpoutLogFile() {
		if (!logPath.empty()) {
			if (logFile.is_open())
				logFile.close();
			logPath = "";
		}
	}

	// Disable logging to console and file
	void DisableSpoutLog()
	{
		CloseSpoutConsole();
		if (!logPath.empty()) {
			if (logFile.is_open())
				logFile.close();
			logPath = "";
		}
		bEnableLog = false;
		bEnableLogFile = false;
	}

	// Disable logs
	void DisableLogs() {
		bDoLogs = false;
	}

	// Enable logs
	void EnableLogs() {
		bDoLogs = true;
	}
	
	// Return the Spout log file as a string
	std::string GetSpoutLog()
	{
		std::string logString = "";

		if (!logPath.empty()) {
			logFile.open(logPath);
			if (logFile.is_open()) {
				if (_access(logPath.c_str(), 0) != -1) { // does the file exist
					// Open the log file
					std::ifstream logstream(logPath);
					// Source file loaded OK ?
					if (logstream.is_open()) {
						// Get the file text as a single string
						logString.assign((std::istreambuf_iterator< char >(logstream)), std::istreambuf_iterator< char >());
						logString += ""; // ensure a NULL terminator
						logstream.close();
					}
				}
			}
		}

		return logString;
	}
	
	// Show the Spout log file folder in Windows Explorer
	void ShowSpoutLogs()
	{
		SHELLEXECUTEINFOA ShExecInfo;
		char directory[MAX_PATH];

		if (logPath.empty() || _access(logPath.c_str(), 0) == -1) {
			std::string logfilefolder = _getLogPath();
			strcpy_s(directory, MAX_PATH, logfilefolder.c_str());
		}
		else {
			strcpy_s(directory, MAX_PATH, logPath.c_str());
			PathRemoveFileSpecA(directory); // Current log file path
		}

		memset(&ShExecInfo, 0, sizeof(ShExecInfo));
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.lpFile = (LPCSTR)directory;
		ShExecInfo.nShow = SW_SHOW;
		ShellExecuteExA(&ShExecInfo);
	}
	
	// Set the current log level
	void SetSpoutLogLevel(SpoutLogLevel level)
	{
		CurrentLogLevel = level;
	}


	void SpoutLog(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_NONE, format, args);
		va_end(args);
	}

	void SpoutLogVerbose(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_VERBOSE, format, args);
		va_end(args);
	}

	void SpoutLogNotice(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_NOTICE, format, args);
		va_end(args);
	}

	void SpoutLogWarning(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_WARNING, format, args);
		va_end(args);
	}

	void SpoutLogError(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_ERROR, format, args);
		va_end(args);
	}

	void SpoutLogFatal(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		_doLog(SPOUT_LOG_FATAL, format, args);
		va_end(args);
	}
	
	//
	// MessageBox
	//

	// SpoutPanel Messagebox with optional timeout
	int SpoutMessageBox(const char * message, DWORD dwMilliseconds)
	{
		return SpoutMessageBox(NULL, message, "spout", MB_OK, dwMilliseconds);
	}

	// SpoutPanel Messagebox with standard arguments and optional timeout
	int SpoutMessageBox(HWND hwnd, LPCSTR message, LPCSTR caption, UINT uType, DWORD dwMilliseconds)
	{
		int iRet = 0;
		SHELLEXECUTEINFOA ShExecInfo;
		char path[MAX_PATH];

		std::string spoutmessage = message;

		// Find if there has been a Spout installation with an install path for SpoutPanel.exe
		if (ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "InstallPath", path)) {
			// Does the file exist ?
			if (_access(path, 0) != -1) {
				// Open SpoutPanel text message
				// If a timeout has been specified, add the timeout option and value
				// SpoutPanel handles the timeout delay
				if (dwMilliseconds > 0) {
					spoutmessage += " /TIMEOUT ";
					spoutmessage += std::to_string((unsigned long long)dwMilliseconds);
				}

				ZeroMemory(&ShExecInfo, sizeof(ShExecInfo));
				ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				ShExecInfo.hwnd = NULL;
				ShExecInfo.lpVerb = NULL;
				ShExecInfo.lpFile = (LPCSTR)path;
				ShExecInfo.lpParameters = (LPCSTR)spoutmessage.c_str();
				ShExecInfo.lpDirectory = NULL;
				ShExecInfo.nShow = SW_SHOW;
				ShExecInfo.hInstApp = NULL;
				ShellExecuteExA(&ShExecInfo);
				// Returns straight away here but multiple instances of SpoutPanel
				// are prevented in it's WinMain procedure by the mutex.
			}
			else {
				// Registry path OK but no SpoutPanel.exe
				// Use a standard untimed topmost messagebox
				iRet = MessageBoxA(hwnd, spoutmessage.c_str(), caption, (uType | MB_TOPMOST));
			}
		}
		else {
			// No SpoutPanel path registered
			// Use a standard untimed topmost messagebox
			iRet = MessageBoxA(hwnd, spoutmessage.c_str(), caption, (uType | MB_TOPMOST));
		}

		return iRet;
	}

	//
	// Registry utilities
	//

	//
	// New registry functions for 2.007 including hKey and changed argument order
	//
	bool ReadDwordFromRegistry(HKEY hKey, const char *subkey, const char *valuename, DWORD *pValue)
	{
		HKEY  hRegKey = NULL;
		LONG  regres = 0;
		DWORD dwKey = 0;
		DWORD dwSize = MAX_PATH;

		// 01.01.18
		if (!subkey[0])	return false;

		// Does the key exist
		regres = RegOpenKeyExA(hKey, subkey, NULL, KEY_READ, &hRegKey);
		if (regres == ERROR_SUCCESS) {
			// Read the key DWORD value
			regres = RegQueryValueExA(hRegKey, valuename, NULL, &dwKey, (BYTE*)pValue, &dwSize);
			RegCloseKey(hRegKey);
			if (regres == ERROR_SUCCESS)
				return true;
		}
	
		// Just quit if the key does not exist
		return false;

	}

	bool WriteDwordToRegistry(HKEY hKey, const char *subkey, const char *valuename, DWORD dwValue)
	{
		HKEY  hRegKey = NULL;
		LONG  regres = 0;
		char  mySubKey[512];

		if (!subkey[0])
			return false;

		// The required key
		strcpy_s(mySubKey, 512, subkey);

		// Does the key already exist ?
		regres = RegOpenKeyExA(hKey, mySubKey, NULL, KEY_ALL_ACCESS, &hRegKey);
		if (regres != ERROR_SUCCESS) {
			// Create a new key
			regres = RegCreateKeyExA(hKey, mySubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
		}

		if (regres == ERROR_SUCCESS && hRegKey != NULL) {
			// Write the DWORD value
			regres = RegSetValueExA(hRegKey, valuename, 0, REG_DWORD, (BYTE*)&dwValue, 4);
			// For immediate read after write - necessary here because the app might set the values 
			// and read the registry straight away and it might not be available yet
			// The key must have been opened with the KEY_QUERY_VALUE access right 
			// (included in KEY_ALL_ACCESS)
			RegFlushKey(hRegKey); // needs an open key
			RegCloseKey(hRegKey); // Done with the key
		}

		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("WriteDwordToRegistry - could not write to registry");
			return false;
		}

		return true;

	}

	bool ReadPathFromRegistry(HKEY hKey, const char *subkey, const char *valuename, char *filepath)
	{
		HKEY  hRegKey = NULL;
		LONG  regres = 0;
		DWORD dwKey = 0;
		DWORD dwSize = MAX_PATH;

		if (!subkey[0])
			return false;

		// Does the key exist
		regres = RegOpenKeyExA(hKey, subkey, NULL, KEY_READ, &hRegKey);
		if (regres == ERROR_SUCCESS) {
			// Read the key Filepath value
			regres = RegQueryValueExA(hRegKey, valuename, NULL, &dwKey, (BYTE*)filepath, &dwSize);
			RegCloseKey(hRegKey);
			if (regres == ERROR_SUCCESS)
				return true;
		}
		// Quit if the key does not exist
		return false;
	}
	
	bool WritePathToRegistry(HKEY hKey, const char *subkey, const char *valuename, const char *filepath)
	{
		HKEY  hRegKey = NULL;
		LONG  regres = 0;
		char  mySubKey[512];

		if (!subkey[0]) {
			SpoutLogWarning("WritePathToRegistry - no subkey specified");
			return false;
		}

		// The required key
		strcpy_s(mySubKey, 512, subkey);

		// Does the key already exist ?
		regres = RegOpenKeyExA(hKey, mySubKey, NULL, KEY_ALL_ACCESS, &hRegKey);
		if (regres != ERROR_SUCCESS) {
			// Create a new key
			regres = RegCreateKeyExA(hKey, mySubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
		}

		if (regres == ERROR_SUCCESS && hRegKey != NULL) {
			// Write the path
			regres = RegSetValueExA(hRegKey, valuename, 0, REG_SZ, (BYTE*)filepath, ((DWORD)strlen(filepath) + 1) * sizeof(unsigned char));
			RegCloseKey(hRegKey);
		}

		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("WritePathToRegistry - could not write to registry");
			return false;
		}

		return true;

	}

	bool WriteBinaryToRegistry(HKEY hKey, const char *subkey, const char *valuename, const unsigned char *hexdata, DWORD nChars)
	{
		HKEY  hRegKey = NULL;
		LONG  regres = 0;
		char  mySubKey[512];

		if (!subkey[0]) {
			SpoutLogWarning("WriteBinaryToRegistry - no subkey specified");
			return false;
		}

		// The required key
		strcpy_s(mySubKey, 512, subkey);

		// Does the key already exist ?
		regres = RegOpenKeyExA(hKey, mySubKey, NULL, KEY_ALL_ACCESS, &hRegKey);
		if (regres != ERROR_SUCCESS) {
			// Create a new key
			regres = RegCreateKeyExA(hKey, mySubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
		}

		if (regres == ERROR_SUCCESS && hRegKey != NULL) {
			regres = RegSetValueExA(hRegKey, valuename, 0, REG_BINARY, (BYTE *)hexdata, nChars);
			RegCloseKey(hRegKey);
		}

		if (regres != ERROR_SUCCESS) {
			SpoutLogWarning("WriteBinaryToRegistry - could not write to registry");
			return false;
		}

		return true;

	}


	bool RemovePathFromRegistry(HKEY hKey, const char *subkey, const char *valuename)
	{
		HKEY  hRegKey = NULL;
		LONG  regres = 0;

		// 01.01.18
		if (!subkey[0]) {
			SpoutLogWarning("RemovePathFromRegistry - no subkey specified");
			return false;
		}

		regres = RegOpenKeyExA(hKey, subkey, NULL, KEY_ALL_ACCESS, &hRegKey);
		if (regres == ERROR_SUCCESS) {
			regres = RegDeleteValueA(hRegKey, valuename);
			RegCloseKey(hRegKey);
			return true;
		}

		// Quit if the key does not exist
		SpoutLogWarning("RemovePathFromRegistry - could not open key [%s]", subkey);
		return false;
	}

	// Delete a subkey and its values.
	// Note that key names are not case sensitive.
	// It must be a subkey of the key that hKey identifies, but it cannot have subkeys.
	bool RemoveSubKey(HKEY hKey, const char *subkey)
	{
		LONG lStatus;

		lStatus = RegDeleteKeyA(hKey, subkey);
		if (lStatus == ERROR_SUCCESS)
			return true;

		SpoutLogWarning("RemoveSubkey - error #%ld", lStatus);
		return false;
	}

	bool FindSubKey(HKEY hKey, const char *subkey)
	{
		HKEY hRegKey;
		LONG lStatus = RegOpenKeyExA(hKey, subkey, NULL, KEY_READ, &hRegKey);
		if(lStatus == ERROR_SUCCESS) {
			RegCloseKey(hRegKey);
			return true;
		}

		SpoutLogWarning("FindSubkey - error #%ld", lStatus);
		return false;

	}

	// Timing utility functions
	void StartTiming() {
#ifdef USE_CHRONO
		start = std::chrono::steady_clock::now();
#endif
	}

	double EndTiming() {
#ifdef USE_CHRONO
		end = std::chrono::steady_clock::now();
		double elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
		// printf("    elapsed [%.4f] msec\n", elapsed / 1000.0);
		// printf("elapsed [%.3f] u/sec\n", elapsed);
		return elapsed;
#else
		return 0.0;
#endif
	}

	// Get SDK version number string e.g. "2.007.000"
	std::string GetSDKversion()
	{
		return SDKversion;
	}

	// Perform the log
	void _doLog(SpoutLogLevel level, const char* format, va_list args)
	{
		char currentLog[512]; // allow more than the name length
		std::string logString;

		// Return if logging is paused
		if (!bDoLogs)
			return;

		if (level != SPOUT_LOG_SILENT
			&& CurrentLogLevel != SPOUT_LOG_SILENT
			&& level >= CurrentLogLevel
			&& format != nullptr) {

			// Construct the current log
			// Problem with vsprintf_s here
			// does not seem to use the maximum buffer length
			vsprintf_s(currentLog, 512, format, args);
			logString = currentLog;

			// Prevent multiple logs by comparing with the last
			if (logString == LastSpoutLog)
				return;

			LastSpoutLog = logString; // update the last log

			// Console logging
			if (bEnableLog && bConsole) {

				// For console output, allow multiple warnings
				FILE* out = stdout;
				if (level != SPOUT_LOG_NONE && level != SPOUT_LOG_VERBOSE) {
					fprintf(out, "[%s] ", _levelName(level).c_str());
				}

				vfprintf(out, format, args);
				fprintf(out, "\n");

			}

			// File logging
			if (bEnableLogFile && !logPath.empty()) {
				// Log file output - append the current log
				logFile.open(logPath, logFile.app);
				if (logFile.is_open()) {
					char name[256];
					name[0] = 0;
					if (level != SPOUT_LOG_NONE && level != SPOUT_LOG_VERBOSE) {
						sprintf_s(name, 256, "[%s] ", _levelName(level).c_str());
					}
					logFile << name << currentLog << std::endl;
					logFile.close();
				}
			}
		}
	}

	//
	// Private functions
	//
	namespace
	{
			
		// Get the default log file path
		std::string _getLogPath()
		{
			char logpath[MAX_PATH];
			logpath[0] = 0;

			// Retrieve user %appdata% environment variable
			char *appdatapath = nullptr;
			size_t len;
			bool bSuccess = true;
			errno_t err = _dupenv_s(&appdatapath, &len, "APPDATA");
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
				GetModuleFileNameA(NULL, (LPSTR)logpath, sizeof(logpath));
				PathRemoveFileSpecA((LPSTR)logpath);
			}

			return logpath;
		}

		// Get the name for the current log level
		std::string _levelName(SpoutLogLevel level) {
			switch (level) {
			case SPOUT_LOG_SILENT:
				return "silent";
			case SPOUT_LOG_VERBOSE:
				return "verbose";
			case SPOUT_LOG_NOTICE:
				return "notice";
			case SPOUT_LOG_WARNING:
				return "warning";
			case SPOUT_LOG_ERROR:
				return "error";
			case SPOUT_LOG_FATAL:
				return "fatal";
			default:
				return "";
			}
		}

		// Log to file with optional append 
		void _logtofile(bool append)
		{
			bool bNewFile = true;

			// Set default log file if not specified
			// C:\Users\username\AppData\Roaming\Spout\SpoutLog.txt
			if (logPath.empty()) {
				logPath = _getLogPath();
				logPath += "\\SpoutLog.txt";
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
				char tmp[128];
				time_t datime;
				struct tm tmbuff;
				time(&datime);
				localtime_s(&tmbuff, &datime);
				int year = tmbuff.tm_year + 1900;
				int month = tmbuff.tm_mon + 1;
				int day = tmbuff.tm_mday;
				int hour = tmbuff.tm_hour;
				int min = tmbuff.tm_min;
				int sec = tmbuff.tm_sec;
				sprintf_s(tmp, 128, "%4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", year, month, day, hour, min, sec);

				if (append && !bNewFile) {
					logFile << "   " << tmp << std::endl;
				}
				else {
					logFile << "========================" << std::endl;
					logFile << "    Spout log file" << std::endl;
					logFile << "========================" << std::endl;
					logFile << " " << tmp << std::endl;
				}
				logFile.close();
			}
			else {
				// disable file writes and use a console instead
				logPath = "";
			}
		}

		//
		// Used internally for NVIDIA profile functions
		//

		// Get the current mode from the NVIDIA base profile
		// will just fail for unsupported hardware
		// Starts SpoutSettings.exe with a command line
		// which writes the mode value to the registry
		// Reads back the registry value for the required mode
		bool GetNVIDIAmode(const char *command, int * mode)
		{
			char exePath[MAX_PATH];
			if (!ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "SpoutSettings", exePath)) {
				SpoutLogError("Spout::GetNVIDIAmode - SpoutSettings path not found");
				return false;
			}

			if (!PathFileExistsA(exePath)) {
				SpoutLogError("Spout::GetNVIDIAmode - SpoutSettings.exe not found");
				return false;
			}

			// SpoutSettings -getCommand
			// Returns mode in registry
			char path[MAX_PATH];
			sprintf_s(path, MAX_PATH, "%s -get%s", exePath, command);
			if (ExecuteProcess(path)) {
				DWORD dwMode = 0xffff;
				if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", command, &dwMode)) {
					*mode = (int)dwMode;
					return true;
				}
				else {
					SpoutLogError("Spout::GetNVIDIAmode -  could not read setting from registry");
				}
			}
			else {
				SpoutLogError("Spout::GetNVIDIAmode -  could not start SpoutSettings");
			}
			return false;
		}

		// Set the current mode to the NVIDIA base profile
		// Starts SpoutSettings.exe with a command line
		// which writes the mode value to the registry
		bool SetNVIDIAmode(const char *command, int mode)
		{
			// Find SpoutSettings path
			char exePath[MAX_PATH];
			if (!ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "SpoutSettings", exePath)) {
				SpoutLogError("Spout::SetNVIDIAmode - SpoutSettings path not found");
				return false;
			}

			if (!PathFileExistsA(exePath)) {
				SpoutLogError("Spout::SetNVIDIAmode - SpoutSettings.exe not found");
				return false;
			}

			// SpoutSettings -setCommand mode
			// Sets the required mode and writes it to the registry
			char path[MAX_PATH];
			sprintf_s(path, MAX_PATH, "%s -set%s %d", exePath, command, mode);
			if (ExecuteProcess(path))
				return true;

			return false;
		}


		// Open process and wait for completion
		bool ExecuteProcess(char *path)
		{
			HANDLE hProcess = NULL; // Handle from CreateProcess
			DWORD dwExitCode = 0; // Exit code when process terminates
			STARTUPINFOA si = { sizeof(STARTUPINFO) };
			bool bRet = false;

			ZeroMemory((void *)&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION pi;
			SetCursor(LoadCursor(NULL, IDC_WAIT));
			if (CreateProcessA(NULL, (LPSTR)path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
				hProcess = pi.hProcess;
				// Wait for CreateProcess to finish
				double elapsed = 0.0;
				if (hProcess) {
					StartTiming(); // for 1 second timeout
					do {
						if (!GetExitCodeProcess(hProcess, &dwExitCode)) {
							bRet = false;
							break;
						}
						elapsed = EndTiming() / 1000.0; // msec
					} while (dwExitCode == STILL_ACTIVE && elapsed < 1000.0);
					hProcess = NULL;
					bRet = true;
				}
			}
			else {
				SpoutLogError("Spout::ExecuteProcess - CreateProcess failed\n    %s", path);
				bRet = false;
			}
			SetCursor(LoadCursor(NULL, IDC_ARROW));

			return bRet;
		}
	} // end private namespace

} // end namespace spoututils
