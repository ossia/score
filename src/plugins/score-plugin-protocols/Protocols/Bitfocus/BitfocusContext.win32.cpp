#include "BitfocusContext.hpp"

#include <QDebug>
#include <QLocalSocket>

#include <stdio.h>
#include <windows.h>

#include <iostream>

namespace bitfocus
{
struct win32_handles
{
  char PIPENAME[512] = {};
  HANDLE pipe_server{INVALID_HANDLE_VALUE};
  HANDLE pipe_client{INVALID_HANDLE_VALUE};
  PROCESS_INFORMATION process{
      .hProcess = INVALID_HANDLE_VALUE,
      .hThread = INVALID_HANDLE_VALUE,
      .dwProcessId = {},
      .dwThreadId = {}};

  QLocalSocket socket;

  bool ready{false};
  win32_handles()
  {
    static int current_socket_index = 0;

    snprintf(
        PIPENAME, sizeof(PIPENAME), "\\\\?\\pipe\\ossia.companion-ipc.%llu.%d",
        (std::uintptr_t)GetProcessId(GetCurrentProcess()), current_socket_index++);

    if(!initServer())
      return;
    if(!initClient())
      return;
    initSocket();
    ready = true;
  }

  ~win32_handles()
  {
    socket.close();

    if(process.hProcess)
    {
      TerminateProcess(process.hProcess, 0);

      const DWORD result = WaitForSingleObject(process.hProcess, 500);

      CloseHandle(process.hProcess);
      CloseHandle(process.hThread);
    }
    if(pipe_client)
    {
      CloseHandle(pipe_client);
    }
    if(pipe_server)
    {
      CloseHandle(pipe_server);
    }
  }

  bool initServer()
  {
    OVERLAPPED overlapped = {0};
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // clang-format off
    pipe_server = CreateNamedPipeA(PIPENAME,

                                   PIPE_ACCESS_DUPLEX
                                       | FILE_FLAG_OVERLAPPED
                                       | WRITE_DAC
                                       | FILE_FLAG_FIRST_PIPE_INSTANCE,

                                   PIPE_TYPE_BYTE
                                       | PIPE_READMODE_BYTE
                                       | PIPE_WAIT,

                                   1,
                                   65536,
                                   65536,
                                   0,
                                   &sa);
    // clang-format on

    if(pipe_server == INVALID_HANDLE_VALUE)
    {
      qDebug() << "CreateNamedPipe failed, GLE=" << GetLastError();
      return false;
    }
    return true;
  }

  bool initClient()
  {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    pipe_client = CreateFileA(
        PIPENAME, GENERIC_READ | GENERIC_WRITE | WRITE_DAC, 0, &sa, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, NULL);

    if(pipe_client == INVALID_HANDLE_VALUE)
    {
      qDebug() << "CreateFileA failed, GLE=" << GetLastError();
      return false;
    }

    if(!ConnectNamedPipe(pipe_server, NULL))
    {
      if(GetLastError() != ERROR_PIPE_CONNECTED)
      {
        qDebug() << "ConnectNamedPipe failed, GLE=" << GetLastError();
        return false;
      }
    }
    return true;
  }

  void initSocket() { socket.setSocketDescriptor((intptr_t)pipe_server); }

  static std::vector<char> environment(const QProcessEnvironment& genv)
  {
    std::vector<char> env_buf;
    auto env = genv.toStringList();
    int nbytes = 16;
    for(auto& e : env)
      nbytes += e.size() + 1;

    env_buf.reserve(nbytes);

    for(auto& e : env)
    {
      auto str = e.toStdString();
      env_buf.insert(env_buf.end(), str.begin(), str.end());
      env_buf.push_back(0);
    }
    env_buf.push_back(0);
    return env_buf;
  }

  bool startProcess(
      std::string_view command, std::string_view wdir, const QProcessEnvironment& genv)
  {
    STARTUPINFOA si = {0};
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(si);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags
        |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW | CREATE_UNICODE_ENVIRONMENT;

#define FOPEN 0x01
#define FEOFLAG 0x02
#define FCRLF 0x04
#define FPIPE 0x08
#define FNOINHERIT 0x10
#define FAPPEND 0x20
#define FDEV 0x40
#define FTEXT 0x80

    struct
    {
      int count = 4;
      unsigned char flags[4];
      HANDLE handles[4];
    } fd_buf;
    fd_buf.count = 4;
    fd_buf.handles[0] = si.hStdInput;
    fd_buf.flags[0] = FOPEN | FDEV;
    fd_buf.handles[1] = si.hStdOutput;
    fd_buf.flags[1] = FOPEN | FDEV;
    fd_buf.handles[2] = si.hStdError;
    fd_buf.flags[2] = FOPEN | FDEV;
    fd_buf.handles[3] = pipe_client;
    fd_buf.flags[3] = FOPEN | FPIPE;

    si.lpReserved2 = (unsigned char*)&fd_buf;
    si.cbReserved2 = sizeof(fd_buf);

    ZeroMemory(&process, sizeof(PROCESS_INFORMATION));

    HANDLE allowed_handles[] = {si.hStdOutput, pipe_server};
    char app[512] = {0};

    memcpy(app, command.data(), command.size());

    auto env_buf = environment(genv);
    BOOL result = CreateProcessA(
        NULL, app, NULL, NULL, TRUE, 0, env_buf.data(), wdir.data(), &si, &process);
    if(!result)
    {
      qDebug() << "CreateProcess failed, GLE=" << GetLastError();
      return false;
    }
    return true;
  }

  struct message
  {
    uint64_t header;
    uint64_t sz;
    char data[];
  };

  int write(std::string_view data)
  {
    auto buf = std::vector<char>(8 + 8 + data.size());
    auto msg = reinterpret_cast<message*>(buf.data());
    msg->header = 1;
    msg->sz = data.size();
    std::memcpy(msg->data, data.data(), data.size());

    DWORD bytesWritten;
    OVERLAPPED overlappedWrite = {0};
    overlappedWrite.Offset = 0;
    overlappedWrite.OffsetHigh = 0;
    BOOL writeResult = WriteFile(
        pipe_server, buf.data(), buf.size(), &bytesWritten, &overlappedWrite);

    if(!writeResult)
    {
      DWORD writeError = GetLastError();
      if(writeError == ERROR_IO_PENDING)
      {
        writeResult
            = GetOverlappedResult(pipe_server, &overlappedWrite, &bytesWritten, TRUE);
        if(!writeResult)
        {
          qDebug() << "GetOverlappedResult failed:" << writeResult;
          return 1;
        }
      }
      else
      {
        qDebug() << "GetOverlappedResult failed: GLE=" << writeError;
        return 1;
      }
    }
    return 0;
  }

  QByteArray readbuf;
};
module_handler_base::module_handler_base(QString module_path, QString entrypoint)
{
  handles = std::make_unique<win32_handles>();
  if(!handles->ready)
    return;

  connect(&handles->socket, &QLocalSocket::readyRead, this, [&] {
    auto& rb = handles->readbuf;
    rb.append(handles->socket.readAll());

    for(;;)
    {
      if(rb.size() < 16)
        break;

      auto msg = reinterpret_cast<win32_handles::message*>(rb.data());
      if(msg->header != 1)
        qDebug() << "Invalid pipe message?";

      if(msg->sz + 16 > rb.size())
        break;
      std::string json{msg->data, msg->data + msg->sz - 1};

      rb.remove(0, 16 + msg->sz);
      this->processMessage(json);
    }
  });

  auto genv = QProcessEnvironment::systemEnvironment();
  genv.insert("CONNECTION_ID", "connectionId");
  genv.insert("VERIFICATION_TOKEN", "foobar");
  genv.insert("MODULE_MANIFEST", module_path + "/companion/manifest.json");
  genv.insert("NODE_CHANNEL_SERIALIZATION_MODE", "json");
  genv.insert("NODE_CHANNEL_FD", "3");

  std::string cmdline;
  cmdline = "node.exe ";
  cmdline += entrypoint.toStdString();
  handles->startProcess(cmdline, module_path.toStdString(), genv);
}

module_handler_base::~module_handler_base() { }

void module_handler_base::do_write(std::string_view res)
{
  handles->write(res);
}
}
