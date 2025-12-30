#define WIN32_LEAN_AND_MEAN
#include "launcher-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
int main(int argc, char* argv[])
{
  char exe_path[MAX_PATH];
  char exe_dir[MAX_PATH];
  char qml_path[MAX_PATH];
  char qml_import_path[MAX_PATH];
  char score_path[MAX_PATH];
  char command_line[32768]; // Windows max command line length

  // Get the directory where this executable is located
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  // Extract directory path
  char* last_slash = strrchr(exe_path, '\\');
  if(last_slash)
  {
    size_t dir_len = last_slash - exe_path;
    strncpy(exe_dir, exe_path, dir_len);
    exe_dir[dir_len] = '\0';
  }
  else
  {
    strcpy(exe_dir, ".");
  }

  // Build paths
  snprintf(qml_path, MAX_PATH, "%s\\qml\\%s", exe_dir, MAIN_QML);
  snprintf(qml_import_path, MAX_PATH, "%s\\qml", exe_dir);

  // Build command line
  snprintf(
      command_line, sizeof(command_line), "\"%s\\app-bin.exe\" --ui \"%s\"", exe_dir,
      qml_path);

  if(HAS_AUTOPLAY)
  {
    strncat(
        command_line, " --autoplay ", sizeof(command_line) - strlen(command_line) - 1);
  }
  // Add score file if present
  if(HAS_SCORE)
  {
    snprintf(score_path, MAX_PATH, "%s\\%s", exe_dir, SCORE_FILE);
    strncat(command_line, " \"", sizeof(command_line) - strlen(command_line) - 1);
    strncat(command_line, score_path, sizeof(command_line) - strlen(command_line) - 1);
    strncat(command_line, "\"", sizeof(command_line) - strlen(command_line) - 1);
  }

  // Add any additional command line arguments
  for(int i = 1; i < argc; i++)
  {
    strncat(command_line, " \"", sizeof(command_line) - strlen(command_line) - 1);
    strncat(command_line, argv[i], sizeof(command_line) - strlen(command_line) - 1);
    strncat(command_line, "\"", sizeof(command_line) - strlen(command_line) - 1);
  }

  // Setup environment for new process
  SetEnvironmentVariableA("QML2_IMPORT_PATH", qml_import_path);
  SetEnvironmentVariableA(
      "SCORE_CUSTOM_APP_ORGANIZATION_NAME", SCORE_CUSTOM_APP_ORGANIZATION_NAME);
  SetEnvironmentVariableA(
      "SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN", SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN);
  SetEnvironmentVariableA(
      "SCORE_CUSTOM_APP_APPLICATION_NAME", SCORE_CUSTOM_APP_APPLICATION_NAME);
  SetEnvironmentVariableA(
      "SCORE_CUSTOM_APP_APPLICATION_VERSION", SCORE_CUSTOM_APP_APPLICATION_VERSION);

  // Create process
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  // Launch the process
  if(!CreateProcessA(
         NULL,         // Application name (NULL = use command line)
         command_line, // Command line
         NULL,         // Process security attributes
         NULL,         // Thread security attributes
         TRUE,         // Inherit handles
         0,            // Creation flags
         NULL,         // Environment
         exe_dir,      // Current directory
         &si,          // Startup info
         &pi           // Process info
         ))
  {
    MessageBoxA(
        NULL, "Failed to launch " SCORE_CUSTOM_APP_APPLICATION_NAME, "Error",
        MB_OK | MB_ICONERROR);
    return 1;
  }

  // Wait for the process to complete
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Get exit code
  DWORD exit_code = 0;
  GetExitCodeProcess(pi.hProcess, &exit_code);

  // Close handles
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return (int)exit_code;
}
