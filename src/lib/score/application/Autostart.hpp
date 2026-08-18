#pragma once
#include <QString>
#include <QStringList>

#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief Starting score when the machine does, and keeping it started.
 *
 * An appliance is a computer somebody switched on. Nobody logs into it, nobody
 * opens a file, and when it stops nobody is there to start it again. What that
 * needs from score is small -- register with whatever the system uses to launch
 * things -- but the systems disagree about what can be asked of them, and the
 * ways they disagree are the whole design.
 */
struct SCORE_LIB_BASE_EXPORT AutostartSettings
{
  bool enabled{};

  //! Seconds to wait before starting. A machine that has just booted may not
  //! have its network, its audio device or its displays yet.
  int delaySeconds{};

  //! Document to open, or empty for none.
  QString document;

  bool autoplay{};

  //! Come up without an editor. On a board with no window manager this is also
  //! what lets the render output own the screen.
  bool hideEditor{};

  //! Finish loading before starting to play.
  bool waitLoad{};

  bool restartOnCrash{};

  /**
   * Start with the machine rather than with a login.
   *
   * A box in a rack has no session for a per-user registration to hang from.
   * Costs elevation to install, everywhere.
   */
  bool systemScope{};

  bool operator==(const AutostartSettings&) const noexcept = default;
};

/**
 * @brief What the mechanism available here can actually be asked to do.
 *
 * Asked by the settings page so it can refuse to offer what nothing behind it
 * will honour. A control that silently does nothing is worse than an absent
 * one: it is believed.
 */
struct SCORE_LIB_BASE_EXPORT AutostartCapabilities
{
  bool available{};
  bool canDelay{};
  bool canRestartOnCrash{};
  //! Whether starting without a login session is possible at all.
  bool canRunWithoutSession{};
  //! Whether the settings as given would need administrator rights.
  bool needsElevation{};
  //! What will actually be written, in words, for the page to show.
  QString mechanism;
};

//! Whether something is registered, and whether it is still us.
enum class AutostartState
{
  NotInstalled,
  Installed,
  //! Registered, but pointing at another binary -- score was moved, or a second
  //! copy registered itself.
  InstalledElsewhere
};

/**
 * @brief The command line the registration will run.
 *
 * The registration carries the whole invocation rather than score reading a
 * second configuration at startup: the unit, task or entry then says what it
 * does, and can be pasted into a terminal when it does not do it.
 */
SCORE_LIB_BASE_EXPORT QStringList
autostartCommandLine(const AutostartSettings& s, const QString& executable);

/**
 * @brief A systemd unit for these settings.
 *
 * Restart is bounded on purpose. Unbounded restart plus a score that fails at
 * startup is a boot loop, and on a machine with no keyboard that is not
 * recoverable.
 */
SCORE_LIB_BASE_EXPORT QString
systemdUnit(const AutostartSettings& s, const QString& executable);

//! A desktop entry, for a session with no systemd. It can express neither a
//! restart nor a start without a login.
SCORE_LIB_BASE_EXPORT QString
xdgDesktopEntry(const AutostartSettings& s, const QString& executable);

/**
 * @brief A Task Scheduler task for these settings.
 *
 * Several of its defaults are wrong for something that must keep running: a
 * task refuses to start on battery, and is stopped after three days. Both are
 * turned off here.
 */
SCORE_LIB_BASE_EXPORT QString
windowsTaskXml(const AutostartSettings& s, const QString& executable);

//! The value for the Run key, which is a command line and nothing else.
SCORE_LIB_BASE_EXPORT QString
windowsRunCommand(const AutostartSettings& s, const QString& executable);

//! A launchd agent property list.
SCORE_LIB_BASE_EXPORT QString
launchdPlist(const AutostartSettings& s, const QString& executable);

//! The name score registers itself under, on every system.
SCORE_LIB_BASE_EXPORT QString autostartIdentifier();

/**
 * @brief Whether something else is already responsible for restarting score.
 *
 * A supervisor and score both restarting it gives two of them, one of which
 * nothing is watching. Anything that wants to restart score has to ask this
 * first and simply exit instead.
 */
SCORE_LIB_BASE_EXPORT bool runningUnderSupervisor();
}
