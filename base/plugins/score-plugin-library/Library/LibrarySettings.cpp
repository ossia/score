#include <Library/LibrarySettings.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>

#include <QApplication>
#include <QDir>
#include <QFormLayout>
#include <QLineEdit>
#include <QStandardPaths>
#include <QStyle>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Library::Settings::View)
W_OBJECT_IMPL(Library::Settings::Model)

namespace Library::Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(Path){QStringLiteral("Library/Path"), [] {
                                auto paths = QStandardPaths::standardLocations(
                                    QStandardPaths::DocumentsLocation);
                                return paths[0] + "/ossia score library";
                              }()};

static auto list()
{
  return std::tie(Path);
}
}

static void initSystemLibrary(QDir& lib_folder)
{
  lib_folder.mkpath(".");
  lib_folder.mkpath("./Scores");
  lib_folder.mkpath("./Medias");
  lib_folder.mkpath("./Presets");
  lib_folder.mkpath("./Devices");
  lib_folder.mkpath("./Cues");
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
  auto lib_folder = getPath();
  if (QDir dir{lib_folder}; !dir.exists())
  {
    initSystemLibrary(dir);
  }
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Path)

Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(Path);
}

QString Presenter::settingsName()
{
  return tr("Library");
}

QIcon Presenter::settingsIcon()
{
  return QApplication::style()->standardIcon(QStyle::SP_DriveHDIcon);
}

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  SETTINGS_UI_PATH_SETUP("Default Path", Path);
}

SETTINGS_UI_PATH_IMPL(Path)
QWidget* View::getWidget()
{
  return m_widg;
}
}
