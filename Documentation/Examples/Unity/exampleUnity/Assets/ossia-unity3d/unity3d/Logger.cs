using System.Collections.Generic;
using System.Runtime;
using System.Runtime.InteropServices;
using System;
using System.Diagnostics;
using System.ComponentModel;
using UnityEngine;

namespace Ossia
{
  class Logger : MonoBehaviour
  {
    IntPtr ossia_logger = IntPtr.Zero;

    public string host = "ws://127.0.0.1:1337";
    public string cmdLine = "/path/to/Unity";
    void Awake ()
    {
      if (ossia_logger == IntPtr.Zero) {
        ossia_logger = Network.ossia_logger_create (host, Controller.Get().appName);
        InitHeartbeat (Process.GetCurrentProcess ().Id, cmdLine);
      }
    }

    Logger()
    {
    }

    Logger(string host, string appname)
    {
      ossia_logger = Network.ossia_logger_create (host, appname);
    }

    ~Logger()
    {
      Network.ossia_logger_free (ossia_logger);      
    }

    void InitHeartbeat(int pid, string cmdline)
    {
      Network.ossia_logger_init_heartbeat (ossia_logger, pid, cmdline);
    }

    void SetLevel(log_level level)
    {
      Network.ossia_logger_set_level (ossia_logger, level);
    }

    void Trace(String s)
    { Network.ossia_log(ossia_logger, Ossia.log_level.trace, s); }
    void Info(String s)
    { Network.ossia_log(ossia_logger, Ossia.log_level.info, s); }
    void Debug(String s)
    { Network.ossia_log(ossia_logger, Ossia.log_level.debug, s); }
    void Warn(String s)
    { Network.ossia_log(ossia_logger, Ossia.log_level.warn, s); }
    void Error(String s)
    { Network.ossia_log(ossia_logger, Ossia.log_level.error, s); }
    void Critical(String s)
    { Network.ossia_log(ossia_logger, Ossia.log_level.critical, s); }
  }
}