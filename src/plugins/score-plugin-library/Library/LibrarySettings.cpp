#include <Library/LibrarySettings.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QApplication>
#include <QDir>
#include <QFormLayout>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QMessageBox>

#include <wobjectimpl.h>
#include <zipdownloader.hpp>

W_OBJECT_IMPL(Library::Settings::View)
W_OBJECT_IMPL(Library::Settings::Model)

namespace Library::Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(RootPath){QStringLiteral("Library/RootPath"), []() -> QString {
                                auto paths = QStandardPaths::standardLocations(
                                    QStandardPaths::DocumentsLocation);
                                return paths[0] + "/ossia/score";
                              }()};

static auto list()
{
  return std::tie(RootPath);
}
}

static void initUserLibrary(QDir userlib)
{
  userlib.mkpath("./medias");
  userlib.mkpath("./presets");
  userlib.mkpath("./devices");
  userlib.mkpath("./cues");
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);

  for(auto& path : {getPackagesPath(), getUserLibraryPath()})
  {
    QDir{path}.mkpath(".");
  }
  initUserLibrary(getUserLibraryPath());

#if !defined(__EMSCRIPTEN__)
  if(ctx.applicationSettings.gui)
  {
    QTimer::singleShot(3000, this, &Model::firstTimeLibraryDownload);
  }
#endif
}

QString Model::getPackagesPath() const noexcept
{
  return m_RootPath + "/packages";
}

QString Model::getDefaultLibraryPath() const noexcept
{
  return m_RootPath + "/packages/default";
}

QString Model::getUserLibraryPath() const noexcept
{
  return m_RootPath + "/packages/user";
}

QString Model::getUserPresetsPath() const noexcept
{
  return m_RootPath + "/packages/user/presets";
}

QString Model::getSDKPath() const noexcept
{
  return m_RootPath + "/sdk";
}

void Model::firstTimeLibraryDownload()
{
  QString lib_folder = getPackagesPath() + "/default";
  QString lib_info = lib_folder + "/package.json";
  if (QFile file{lib_info}; !file.exists())
  {
    auto dl = score::question(
        qApp->activeWindow(),
        tr("Download the user library ?"),
        tr("The user library has not been found. \n"
           "Do you want to download it from the internet ? \n\n"
           "Note: you can always download it later from : \n"
           "https://github.com/ossia/score-user-library"));

    if (dl == QMessageBox::Yes)
    {
      zdl::download_and_extract(
          QUrl{"https://github.com/ossia/score-user-library/archive/master.zip"},
          getPackagesPath(),
          [this] (const auto&) mutable {
            QDir packages_dir{getPackagesPath()};
            packages_dir.rename("score-user-library-master", "default");

            rescanLibrary();
          },
      [] (qint64 bytesReceived, qint64 bytesTotal) {
        qDebug() << (((bytesReceived / 1024.) / (bytesTotal / 1024.)) * 100)
                 << "% downloaded"; },
          [] {});
    }
  }
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, RootPath)

Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(RootPath);
}

QString Presenter::settingsName()
{
  return tr("Library");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_library_on.png"),
      QStringLiteral(":/icons/settings_library_off.png"),
      QStringLiteral(":/icons/settings_library_off.png"));
}

View::View()
{
  m_widg = new score::FormWidget{tr("Library")};

  auto lay = m_widg->layout();

  SETTINGS_UI_PATH_SETUP("Default Path", RootPath);
}

SETTINGS_UI_PATH_IMPL(RootPath)
QWidget* View::getWidget()
{
  return m_widg;
}
}
