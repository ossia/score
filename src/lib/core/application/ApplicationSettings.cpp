// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationSettings.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QString>

#include <score_git_info.hpp>
namespace score
{
void ApplicationSettings::parse(QStringList cargs, int& argc, char** argv)
{
  arguments = cargs;

  opengl = false;
  QCommandLineParser parser;
  parser.setApplicationDescription(
      QObject::tr("score - An interactive sequencer for the intermedia arts."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(
      "file", QCoreApplication::translate("main", "Scenario to load."));

  QCommandLineOption noGUI("no-gui", QCoreApplication::translate("main", "Disable GUI"));
  parser.addOption(noGUI);

  QCommandLineOption noGL(
      "no-opengl", QCoreApplication::translate("main", "Disable OpenGL rendering"));
  parser.addOption(noGL);

  QCommandLineOption GL(
      "opengl", QCoreApplication::translate("main", "Enable OpenGL rendering"));
  parser.addOption(GL);

  QCommandLineOption noRestore(
      "no-restore", QCoreApplication::translate("main", "Disable auto-restore"));
  parser.addOption(noRestore);

  QCommandLineOption autoplayOpt(
      "autoplay", QCoreApplication::translate("main", "Auto-play the loaded scenario"));
  parser.addOption(autoplayOpt);

  QCommandLineOption waitLoadOpt(
      "wait",
      QCoreApplication::translate(
          "main", "Wait N seconds after loading, before playing."),
      "N", "0");
  parser.addOption(waitLoadOpt);

#if defined(__APPLE__)
  // Bogus macOS gatekeeper BS:
  // https://stackoverflow.com/questions/55562155/qt-application-for-mac-not-being-launched
  for(auto it = cargs.begin(); it != cargs.end();)
  {
    auto& str = *it;
    if(str.startsWith("-psn"))
    {
      it = cargs.erase(it);
    }
    else
    {
      ++it;
    }
  }
#endif

  if(cargs.contains("--help") || cargs.contains("--version"))
  {
    QCoreApplication app(argc, argv);
    setQApplicationMetadata();
    parser.process(cargs);
    exit(0);
  }
  else
  {
    parser.parse(cargs);
  }

  // Remove all the positional arguments that aren't files
  // otherwise we get ./ossia-score foo.score -platform vnc
  // => {"foo.score", "vnc"}...
  QStringList args = parser.positionalArguments();
  for(auto it = args.begin(); it != args.end();)
  {
    if(QFile::exists(*it))
    {
      *it = QFileInfo{*it}.canonicalFilePath();
      ++it;
    }
    else
    {
      it = args.erase(it);
    }
  }

  tryToRestore = !parser.isSet(noRestore);
  gui = !parser.isSet(noGUI);
  if(parser.isSet(GL))
    opengl = true;
  if(parser.isSet(noGL))
    opengl = false;

  if(!gui)
    tryToRestore = false;
  autoplay = parser.isSet(autoplayOpt) && args.size() == 1;

  if(parser.isSet(waitLoadOpt))
    waitAfterLoad = parser.value(waitLoadOpt).toInt();

  if(!args.empty() && QFile::exists(args[0]))
  {
    loadList.push_back(args[0]);
  }
}

void setQApplicationMetadata()
{
  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("score");
  if(QString(SCORE_VERSION_EXTRA).isEmpty())
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
