#include <score/application/Autostart.hpp>

#include <QDir>
#include <QtGlobal>
#include <QFileInfo>

namespace score
{
namespace
{
//! Quoting for a place that takes one string and splits it itself.
QString quoted(const QString& s)
{
  return QStringLiteral("\"") + QString{s}.replace('"', QStringLiteral("\\\""))
         + QStringLiteral("\"");
}

QString xmlEscaped(const QString& s)
{
  QString r = s;
  r.replace('&', QStringLiteral("&amp;"));
  r.replace('<', QStringLiteral("&lt;"));
  r.replace('>', QStringLiteral("&gt;"));
  r.replace('"', QStringLiteral("&quot;"));
  return r;
}

//! ISO 8601, which is how both Task Scheduler and launchd spell a duration.
QString isoSeconds(int seconds)
{
  return QStringLiteral("PT%1S").arg(seconds);
}
}

QString autostartIdentifier()
{
  return QStringLiteral("ossia-score");
}

QStringList autostartCommandLine(const AutostartSettings& s, const QString& executable)
{
  QStringList args;
  if(s.hideEditor)
    args << QStringLiteral("--no-gui");
  if(s.waitLoad)
    args << QStringLiteral("--wait-load");
  if(s.autoplay)
    args << QStringLiteral("--autoplay");

  // Last: everything before it is an option, and a document whose name starts
  // with a dash would otherwise be read as one.
  if(!s.document.isEmpty())
    args << s.document;

  return QStringList{executable} << args;
}

QString systemdUnit(const AutostartSettings& s, const QString& executable)
{
  const auto cmd = autostartCommandLine(s, executable);

  QString exec;
  for(const auto& part : cmd)
    exec += (exec.isEmpty() ? QString{} : QStringLiteral(" ")) + quoted(part);

  QString u;
  u += QStringLiteral("[Unit]\n");
  u += QStringLiteral("Description=ossia score\n");
  // Without this the score starts before there is a display or a network to
  // use, which on a machine that boots straight into a show is the same as not
  // starting.
  u += QStringLiteral("After=network-online.target sound.target\n");
  u += QStringLiteral("Wants=network-online.target\n\n");

  u += QStringLiteral("[Service]\n");
  u += QStringLiteral("Type=simple\n");
  if(s.delaySeconds > 0)
    u += QStringLiteral("ExecStartPre=/bin/sleep %1\n").arg(s.delaySeconds);
  u += QStringLiteral("ExecStart=%1\n").arg(exec);

  if(s.restartOnCrash)
  {
    u += QStringLiteral("Restart=always\n");
    u += QStringLiteral("RestartSec=5\n");
    // The guard: five failures in two minutes and systemd stops trying. A
    // score that cannot start would otherwise be restarted forever, and a
    // machine in that state cannot be logged into to fix it.
    u += QStringLiteral("StartLimitIntervalSec=120\n");
    u += QStringLiteral("StartLimitBurst=5\n");
  }
  u += QStringLiteral("\n[Install]\n");
  u += s.systemScope ? QStringLiteral("WantedBy=multi-user.target\n")
                     : QStringLiteral("WantedBy=default.target\n");
  return u;
}

QString xdgDesktopEntry(const AutostartSettings& s, const QString& executable)
{
  auto cmd = autostartCommandLine(s, executable);

  // Nothing here can wait, so the delay is spent by the command itself.
  if(s.delaySeconds > 0)
  {
    QStringList wrapped{
        QStringLiteral("/bin/sh"), QStringLiteral("-c"),
        QStringLiteral("sleep %1; exec ").arg(s.delaySeconds) + cmd.join(' ')};
    cmd = wrapped;
  }

  QString exec;
  for(const auto& part : cmd)
    exec += (exec.isEmpty() ? QString{} : QStringLiteral(" ")) + part;

  QString d;
  d += QStringLiteral("[Desktop Entry]\n");
  d += QStringLiteral("Type=Application\n");
  d += QStringLiteral("Name=ossia score\n");
  d += QStringLiteral("Exec=%1\n").arg(exec);
  d += QStringLiteral("X-GNOME-Autostart-enabled=true\n");
  d += QStringLiteral("Terminal=false\n");
  return d;
}

QString windowsTaskXml(const AutostartSettings& s, const QString& executable)
{
  QStringList args = autostartCommandLine(s, executable);
  args.removeFirst();

  QString arguments;
  for(const auto& a : args)
    arguments += (arguments.isEmpty() ? QString{} : QStringLiteral(" ")) + quoted(a);

  const auto trigger = s.systemScope ? QStringLiteral("BootTrigger")
                                     : QStringLiteral("LogonTrigger");

  QString x;
  x += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n");
  x += QStringLiteral("<Task version=\"1.2\" "
                      "xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/"
                      "task\">\n");
  x += QStringLiteral("  <RegistrationInfo>\n    <Description>ossia "
                      "score</Description>\n  </RegistrationInfo>\n");

  x += QStringLiteral("  <Triggers>\n    <%1>\n      "
                      "<Enabled>true</Enabled>\n")
           .arg(trigger);
  if(s.delaySeconds > 0)
    x += QStringLiteral("      <Delay>%1</Delay>\n").arg(isoSeconds(s.delaySeconds));
  x += QStringLiteral("    </%1>\n  </Triggers>\n").arg(trigger);

  x += QStringLiteral("  <Principals>\n    <Principal id=\"Author\">\n");
  x += s.systemScope
           ? QStringLiteral("      <UserId>S-1-5-18</UserId>\n      "
                            "<RunLevel>HighestAvailable</RunLevel>\n")
           : QStringLiteral("      <LogonType>InteractiveToken</LogonType>\n      "
                            "<RunLevel>LeastPrivilege</RunLevel>\n");
  x += QStringLiteral("    </Principal>\n  </Principals>\n");

  x += QStringLiteral("  <Settings>\n");
  // Both of these default the wrong way for something that has to stay up: a
  // task will not start on battery, and is stopped after three days.
  x += QStringLiteral("    <DisallowStartIfOnBatteries>false</"
                      "DisallowStartIfOnBatteries>\n");
  x += QStringLiteral("    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n");
  x += QStringLiteral("    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n");
  // A second copy started by a later trigger would fight the first over the
  // audio device and the screen.
  x += QStringLiteral("    <MultipleInstancesPolicy>IgnoreNew</"
                      "MultipleInstancesPolicy>\n");
  if(s.restartOnCrash)
  {
    // Bounded, for the same reason as StartLimitBurst.
    x += QStringLiteral("    <RestartOnFailure>\n      <Interval>PT1M</Interval>\n     "
                        " <Count>5</Count>\n    </RestartOnFailure>\n");
  }
  x += QStringLiteral("  </Settings>\n");

  x += QStringLiteral("  <Actions Context=\"Author\">\n    <Exec>\n");
  x += QStringLiteral("      <Command>%1</Command>\n").arg(xmlEscaped(executable));
  if(!arguments.isEmpty())
    x += QStringLiteral("      <Arguments>%1</Arguments>\n").arg(xmlEscaped(arguments));
  x += QStringLiteral("      <WorkingDirectory>%1</WorkingDirectory>\n")
           .arg(xmlEscaped(QFileInfo{executable}.absolutePath()));
  x += QStringLiteral("    </Exec>\n  </Actions>\n</Task>\n");
  return x;
}

QString windowsRunCommand(const AutostartSettings& s, const QString& executable)
{
  const auto cmd = autostartCommandLine(s, executable);
  QString line;
  for(const auto& part : cmd)
    line += (line.isEmpty() ? QString{} : QStringLiteral(" ")) + quoted(part);
  return line;
}

QString launchdPlist(const AutostartSettings& s, const QString& executable)
{
  const auto cmd = autostartCommandLine(s, executable);

  QString p;
  p += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  p += QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
  p += QStringLiteral("<plist version=\"1.0\">\n<dict>\n");
  p += QStringLiteral("  <key>Label</key>\n  <string>%1</string>\n")
           .arg(autostartIdentifier());

  p += QStringLiteral("  <key>ProgramArguments</key>\n  <array>\n");
  for(const auto& part : cmd)
    p += QStringLiteral("    <string>%1</string>\n").arg(xmlEscaped(part));
  p += QStringLiteral("  </array>\n");

  p += QStringLiteral("  <key>RunAtLoad</key>\n  <true/>\n");
  if(s.restartOnCrash)
  {
    p += QStringLiteral("  <key>KeepAlive</key>\n  <true/>\n");
    // launchd throttles a job that exits repeatedly; asking for less than ten
    // seconds is ignored, so this is the floor rather than a choice.
    p += QStringLiteral("  <key>ThrottleInterval</key>\n  <integer>10</integer>\n");
  }
  if(s.delaySeconds > 0)
    p += QStringLiteral("  <key>StartInterval</key>\n  <integer>%1</integer>\n")
             .arg(s.delaySeconds);
  p += QStringLiteral("</dict>\n</plist>\n");
  return p;
}

bool runningUnderSupervisor()
{
#if defined(__linux__)
  // systemd stamps every service it starts with this; nothing else sets it.
  return qEnvironmentVariableIsSet("INVOCATION_ID");
#elif defined(__APPLE__)
  // launchd names the job it is running in. A shell has it unset or "0".
  const auto xpc = qgetenv("XPC_SERVICE_NAME");
  return !xpc.isEmpty() && xpc != "0";
#else
  // Task Scheduler leaves nothing in the environment to recognise it by, so
  // this answers no and score restarts itself as it would unsupervised. The
  // task's MultipleInstancesPolicy is what keeps that from doubling up.
  return false;
#endif
}
}
