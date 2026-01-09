#define WIN32_LEAN_AND_MEAN
#include "launcher-defines.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include <string>
#include <string_view>
#include <iostream>
#include <cctype>

static std::string_view trim_sv(std::string_view s) 
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static void parse_and_set_env(std::string_view env_text) {
    size_t pos = 0;
    while (pos < env_text.size()) 
    {
        size_t nl = env_text.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
                                    ? env_text.substr(pos)
                                    : env_text.substr(pos, nl - pos);

        // Strip trailing CR if present (CRLF)
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        auto trimmed = trim_sv(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            pos = (nl == std::string_view::npos) ? env_text.size() : nl + 1;
            continue;
        }

        // Handle optional "export" prefix: must be followed by whitespace
        if (trimmed.size() >= 7 && trimmed.substr(0, 6) == "export"
            && std::isspace(static_cast<unsigned char>(trimmed[6]))) {
            trimmed.remove_prefix(6);
            trimmed = trim_sv(trimmed);
        }

        // Find '='
        auto eq = trimmed.find('=');
        if (eq == std::string_view::npos) {
            pos = (nl == std::string_view::npos) ? env_text.size() : nl + 1;
            continue;
        }

        auto name_sv = trim_sv(trimmed.substr(0, eq));
        auto value_sv = trim_sv(trimmed.substr(eq + 1));

        // Strip surrounding matching quotes (single or double)
        if (value_sv.size() >= 2) {
            char first = value_sv.front();
            char last = value_sv.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                value_sv.remove_prefix(1);
                value_sv.remove_suffix(1);
            }
        }

        if (!name_sv.empty()) {
            // Convert to std::string for SetEnvironmentVariableA
            std::string name{name_sv};
            std::string value{value_sv}; // empty string is allowed

            if (!SetEnvironmentVariableA(name.c_str(), value.c_str())) {
                DWORD err = GetLastError();
                std::cerr << "Failed to set env '" << name << "' -> '" << value
                          << "' (GetLastError=" << err << ")\n";
            }
        }

        pos = (nl == std::string_view::npos) ? env_text.size() : nl + 1;
    }
}

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
 
  if(strlen(SCORE_ENVIRONMENT) > 0)
    parse_and_set_env(SCORE_ENVIRONMENT);

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

#include <ShlObj.h>
#include <shellapi.h>
#include <stdio.h>
#include <windows.h>
static inline char* wideToMulti(int codePage, const wchar_t* aw)
{
  const int required = WideCharToMultiByte(codePage, 0, aw, -1, NULL, 0, NULL, NULL);
  char* result = new char[required];
  WideCharToMultiByte(codePage, 0, aw, -1, result, required, NULL, NULL);
  return result;
}

extern "C" int APIENTRY
WinMain(HINSTANCE, HINSTANCE, LPSTR /*cmdParamarg*/, int /* cmdShow */)
{
  int argc;
  wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if(!argvW)
    return -1;
  char** argv = new char*[argc + 1];
  for(int i = 0; i < argc; ++i)
    argv[i] = wideToMulti(CP_ACP, argvW[i]);
  argv[argc] = nullptr;
  LocalFree(argvW);
  const int exitCode = main(argc, argv);
  for(int i = 0; i < argc && argv[i]; ++i)
    delete[] argv[i];
  delete[] argv;
  return exitCode;
}
