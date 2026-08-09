// Starting score when the machine starts.
//
// What is worth testing here is what gets written: a unit, a task or an entry
// is read by something else entirely, and a mistake in it shows up as a
// machine that does not come up -- often one with no keyboard attached. So the
// generation is pure and pinned here, including the parts whose absence would
// be silent.

#include <score/application/Autostart.hpp>

#include <QRegularExpression>

#include <catch2/catch_test_macros.hpp>

using namespace score;

namespace
{
AutostartSettings appliance()
{
  AutostartSettings s;
  s.enabled = true;
  s.document = "/home/user/show.score";
  s.autoplay = true;
  s.hideEditor = true;
  s.waitLoad = true;
  s.restartOnCrash = true;
  s.delaySeconds = 30;
  return s;
}
}

TEST_CASE("The command line carries the whole intent", "[autostart]")
{
  const auto cmd = autostartCommandLine(appliance(), "/usr/bin/ossia-score");

  REQUIRE(cmd.size() == 5);
  CHECK(cmd[0] == "/usr/bin/ossia-score");
  CHECK(cmd.contains("--no-gui"));
  CHECK(cmd.contains("--autoplay"));
  CHECK(cmd.contains("--wait-load"));

  // The document goes last: an option after it would be read as a file, and a
  // file named like an option would be read as one.
  CHECK(cmd.last() == "/home/user/show.score");
}

TEST_CASE("Nothing asked for, nothing passed", "[autostart]")
{
  AutostartSettings s;
  const auto cmd = autostartCommandLine(s, "/usr/bin/ossia-score");

  REQUIRE(cmd.size() == 1);
  CHECK(cmd[0] == "/usr/bin/ossia-score");
}

TEST_CASE("A systemd unit that cannot loop for ever", "[autostart]")
{
  const auto u = systemdUnit(appliance(), "/usr/bin/ossia-score");

  CHECK(u.contains("ExecStart="));
  CHECK(u.contains("--no-gui"));
  CHECK(u.contains("/bin/sleep 30"));

  CHECK(u.contains("Restart=always"));
  // The guard. Without it a score that fails at startup is restarted for ever
  // and the machine cannot be logged into to fix it -- which on an appliance
  // means a trip to the hardware. This assertion is the whole reason the test
  // exists.
  CHECK(u.contains("StartLimitBurst="));
  CHECK(u.contains("StartLimitIntervalSec="));
}

TEST_CASE("No restart asked, no restart written", "[autostart]")
{
  AutostartSettings s;
  s.enabled = true;
  const auto u = systemdUnit(s, "/usr/bin/ossia-score");

  CHECK(!u.contains("Restart=always"));
  CHECK(!u.contains("StartLimitBurst="));
  CHECK(!u.contains("sleep"));
}

TEST_CASE("Scope decides what the unit is wanted by", "[autostart]")
{
  AutostartSettings user;
  CHECK(systemdUnit(user, "/x").contains("WantedBy=default.target"));

  AutostartSettings system;
  system.systemScope = true;
  CHECK(systemdUnit(system, "/x").contains("WantedBy=multi-user.target"));
}

TEST_CASE("A desktop entry spends the delay itself", "[autostart]")
{
  const auto d = xdgDesktopEntry(appliance(), "/usr/bin/ossia-score");

  CHECK(d.startsWith("[Desktop Entry]"));
  CHECK(d.contains("Type=Application"));
  // It has nowhere to put a delay, so the command waits instead.
  CHECK(d.contains("sleep 30"));
  CHECK(d.contains("--no-gui"));
}

TEST_CASE("A Task Scheduler task that keeps running", "[autostart]")
{
  const auto x = windowsTaskXml(appliance(), "C:\\Program Files\\score\\ossia-score.exe");

  CHECK(x.contains("<LogonTrigger>"));
  CHECK(x.contains("<Delay>PT30S</Delay>"));

  // Three defaults that are wrong for something meant to stay up. A task will
  // not start on battery, will be stopped after three days, and a second
  // trigger would start a second copy to fight the first for the audio device.
  CHECK(x.contains("<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"));
  CHECK(x.contains("<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"));
  CHECK(x.contains("<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"));
  CHECK(x.contains("<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"));

  // Bounded, exactly as the systemd unit is.
  CHECK(x.contains("<RestartOnFailure>"));
  CHECK(x.contains("<Count>5</Count>"));

  CHECK(x.contains("<Command>C:\\Program Files\\score\\ossia-score.exe</Command>"));
  CHECK(x.contains("--no-gui"));
}

TEST_CASE("A task that starts with the machine runs as the system", "[autostart]")
{
  auto s = appliance();
  s.systemScope = true;
  const auto x = windowsTaskXml(s, "C:\\score.exe");

  CHECK(x.contains("<BootTrigger>"));
  CHECK(!x.contains("<LogonTrigger>"));
  // S-1-5-18 is LocalSystem: there is no session to borrow a token from.
  CHECK(x.contains("<UserId>S-1-5-18</UserId>"));
  CHECK(x.contains("HighestAvailable"));
}

TEST_CASE("XML that would not parse is XML that does not run", "[autostart]")
{
  AutostartSettings s;
  s.document = "C:\\shows\\rock & roll <final>.score";
  const auto x = windowsTaskXml(s, "C:\\score.exe");

  CHECK(x.contains("&amp;"));
  CHECK(x.contains("&lt;final&gt;"));
  // The raw forms must not survive anywhere in the document.
  CHECK(!x.contains("roll & roll"));
}

TEST_CASE("The Run key takes one line", "[autostart]")
{
  auto s = appliance();
  s.delaySeconds = 0;
  s.restartOnCrash = false;
  const auto c = windowsRunCommand(s, "C:\\Program Files\\score\\ossia-score.exe");

  // The registry hands the whole value to the shell, which splits on spaces:
  // an unquoted Program Files becomes two arguments and nothing starts.
  CHECK(c.startsWith("\"C:\\Program Files\\score\\ossia-score.exe\""));
  CHECK(c.contains("\"--no-gui\""));
  CHECK(!c.contains("\n"));
}

TEST_CASE("A launchd agent", "[autostart]")
{
  const auto p = launchdPlist(appliance(), "/Applications/score.app/Contents/MacOS/score");

  CHECK(p.contains("<key>Label</key>"));
  CHECK(p.contains(autostartIdentifier()));
  CHECK(p.contains("<key>RunAtLoad</key>"));
  CHECK(p.contains("<key>KeepAlive</key>"));
  // launchd ignores anything under ten seconds, so asking for less would be a
  // setting that silently does nothing.
  CHECK(p.contains("<key>ThrottleInterval</key>"));

  // One argument per string: launchd does no splitting of its own.
  CHECK(p.contains("<string>--no-gui</string>"));
  CHECK(p.contains("<string>/home/user/show.score</string>"));
}

TEST_CASE("A document whose path has spaces survives every mechanism", "[autostart]")
{
  AutostartSettings s;
  s.document = "/home/user/My Shows/opening night.score";

  CHECK(systemdUnit(s, "/usr/bin/ossia-score")
            .contains("\"/home/user/My Shows/opening night.score\""));
  CHECK(windowsRunCommand(s, "C:\\score.exe")
            .contains("\"/home/user/My Shows/opening night.score\""));
  CHECK(launchdPlist(s, "/x").contains(
      "<string>/home/user/My Shows/opening night.score</string>"));
}

TEST_CASE("Whether something else is already restarting score", "[autostart]")
{
  // The tests do not run under a supervisor, and score must not decide it is
  // supervised when it is not: that would turn its own restart into a plain
  // exit and the way back to an editor would stop working.
  CHECK(!runningUnderSupervisor());
}
