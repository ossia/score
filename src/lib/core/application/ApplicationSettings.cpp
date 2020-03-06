// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationSettings.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QObject>
#include <QString>

#include <score_git_info.hpp>
namespace score
{
void ApplicationSettings::parse(QStringList cargs, int& argc, char** argv)
{
#if defined(__linux__)
  opengl = true;
#else
  opengl = false;
#endif
  QCommandLineParser parser;
  parser.setApplicationDescription(QObject::tr(
      "score - An interactive sequencer for the intermedia arts."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(
      "file", QCoreApplication::translate("main", "Scenario to load."));

  QCommandLineOption noGUI(
      "no-gui", QCoreApplication::translate("main", "Disable GUI"));
  parser.addOption(noGUI);

  QCommandLineOption noGL(
      "no-opengl", QCoreApplication::translate("main", "Disable OpenGL rendering"));
  parser.addOption(noGL);

  QCommandLineOption noRestore(
      "no-restore",
      QCoreApplication::translate("main", "Disable auto-restore"));
  parser.addOption(noRestore);

  QCommandLineOption autoplayOpt(
      "autoplay",
      QCoreApplication::translate("main", "Auto-play the loaded scenario"));
  parser.addOption(autoplayOpt);

  QCommandLineOption waitLoadOpt(
      "wait",
      QCoreApplication::translate(
          "main", "Wait N seconds after loading, before playing."),
      "N",
      "0");
  parser.addOption(waitLoadOpt);

  if (cargs.contains("--help") || cargs.contains("--version"))
  {
    QCoreApplication app(argc, argv);
    setQApplicationMetadata();
    parser.process(cargs);
    exit(0);
  }
  else
  {
    parser.process(cargs);
  }

  const QStringList args = parser.positionalArguments();

  tryToRestore = !parser.isSet(noRestore);
  gui = !parser.isSet(noGUI);
  opengl &= !parser.isSet(noGL);
  if (!gui)
    tryToRestore = false;
  autoplay = parser.isSet(autoplayOpt) && args.size() == 1;

  if (parser.isSet(waitLoadOpt))
    waitAfterLoad = parser.value(waitLoadOpt).toInt();

  if (!args.empty() && QFile::exists(args[0]))
  {
    loadList.push_back(args[0]);
  }
}

void setQApplicationMetadata()
{
  QCoreApplication::setOrganizationName("OSSIA");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("score");
  if (QString(SCORE_VERSION_EXTRA).isEmpty())
  {
    QCoreApplication::setApplicationVersion(QString("%1.%2.%3")
                                                .arg(SCORE_VERSION_MAJOR)
                                                .arg(SCORE_VERSION_MINOR)
                                                .arg(SCORE_VERSION_PATCH));
  }
  else
  {
    QCoreApplication::setApplicationVersion(QString("%1.%2.%3-%4")
                                                .arg(SCORE_VERSION_MAJOR)
                                                .arg(SCORE_VERSION_MINOR)
                                                .arg(SCORE_VERSION_PATCH)
                                                .arg(SCORE_VERSION_EXTRA));
  }
}
}
