#pragma once

#if defined(_MSC_VER)
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
#endif
