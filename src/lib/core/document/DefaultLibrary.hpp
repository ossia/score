#pragma once
#include <QObject>
#include <QString>
#include <QUuid>

#include <score_lib_base_export.h>

#include <functional>
#include <verdigris>

namespace score
{
/**
 * @brief Whether the default library package is installed, and up to date.
 *
 * The default library is the package installed in
 * `<library>/packages/default`: it holds the templates, and the presets and
 * media that many examples use. Its installed version comes from its
 * package.json.
 *
 * The package manager lives in a plug-in: it tells this object which version
 * is available online once it has fetched the package list, gives it the
 * function that installs that version, and reports the progress of every
 * install of the package, wherever it was started from. The start screen
 * shows the state and calls install().
 */
class SCORE_LIB_BASE_EXPORT DefaultLibrary final : public QObject
{
  W_OBJECT(DefaultLibrary)

public:
  enum class State
  {
    Current,         //!< Installed, and no newer version is known
    Missing,         //!< Not installed
    UpdateAvailable, //!< Installed, and a newer version is available online
    Installing,      //!< A download or an extraction is under way
    Failed           //!< The last install failed; error() says why
  };

  static DefaultLibrary& instance();

  //! The key of the package, as its package.json and the package list have it.
  static QUuid key();
  //! Where the package is installed, empty if there is no user library.
  static QString folder();
  //! The version in its package.json; -1 when it is not installed.
  static int installedVersion();

  State state() const;
  //! -1 until the package list has been fetched.
  int availableVersion() const noexcept { return m_available; }
  //! In bytes, 0 when unknown.
  qint64 downloadSize() const noexcept { return m_size; }
  //! Between 0 and 1 while installing, negative when the total is unknown.
  double progress() const noexcept { return m_progress; }
  const QString& error() const noexcept { return m_error; }

  //! From the package manager: the version of the package list.
  void setAvailable(int version, qint64 bytes);
  //! From the package manager: what install() calls.
  void setInstaller(std::function<void()> installer);

  bool canInstall() const noexcept { return bool(m_installer); }
  //! Installs or updates the package with the package manager's installer.
  void install();

  //! From the package manager, for any install of the package.
  void setProgress(qint64 received, qint64 total);
  void setFinished(bool ok, const QString& error = {});

  //! True while a view offers the install itself: the package manager then
  //! does not ask with a dialog of its own.
  bool offeredInView() const noexcept { return m_views > 0; }
  void addView() noexcept { m_views++; }
  void removeView() noexcept { m_views--; }

  //! The install under way was started from a view which still shows: it
  //! reports the result, the package manager does not with a dialog of its own.
  bool reportedInView() const noexcept { return m_installing && m_fromView && m_views > 0; }

  void changed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, changed)

private:
  DefaultLibrary();

  std::function<void()> m_installer;
  QString m_error;
  qint64 m_size{};
  double m_progress{-1.};
  int m_available{-1};
  int m_views{};
  bool m_installing{};
  bool m_fromView{};
  bool m_failed{};
};
}
