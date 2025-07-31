#include <JitCpp/ApplicationPlugin.hpp>
#include <JitCpp/MetadataGenerator.hpp>
#include <Library/LibrarySettings.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/view/Window.hpp>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
namespace Jit
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  con(m_addonsWatch, &QFileSystemWatcher::directoryChanged, this, [&](const QString& a) {
    QTimer::singleShot(5000, this, [this] { rescanAddons(); });
  });
  con(m_addonsWatch, &QFileSystemWatcher::fileChanged, this,
      &ApplicationPlugin::updateAddon);

  con(m_nodesWatch, &QFileSystemWatcher::fileChanged, this,
      &ApplicationPlugin::setupNode);

  {
    // Command-line option parsing
    // This part is only used for testing nodes on CI
    QCommandLineParser parser;

    QCommandLineOption compile_node(
        "compile-node", QCoreApplication::translate("jit", "Node to compile"), "Name",
        "");
    parser.addOption(compile_node);
    QCommandLineOption compile_addon(
        "compile-addon",
        QCoreApplication::translate("jit", "Path to the addon to compile"), "Name", "");
    parser.addOption(compile_addon);

    parser.parse(ctx.applicationSettings.arguments);
    auto node_to_compile = parser.value(compile_node);
    auto addon_to_compile = parser.value(compile_addon);

    if((!node_to_compile.isEmpty() || !addon_to_compile.isEmpty()))
    {
      if(QFile::exists(node_to_compile))
      {
        con(m_compiler, &AddonCompiler::jobCompleted, this, [this](auto addon) {
          registerAddon(addon);
          QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
        });

        if(!setupNode(node_to_compile))
          exit(1);
      }
      else if(QFile::exists(addon_to_compile))
      {
        con(m_compiler, &AddonCompiler::jobCompleted, this, [this](auto addon) {
          registerAddon(addon);
          QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
        });

        if(!setupAddon(addon_to_compile))
          exit(1);
      }
      con(m_compiler, &AddonCompiler::jobFailed, this, [] { exit(1); });
    }
    else
    {
      con(m_compiler, &AddonCompiler::jobCompleted, this,
          &ApplicationPlugin::registerAddon, Qt::QueuedConnection);
    }
  }
}

void ApplicationPlugin::rescanAddons()
{
  QString addons = context.settings<Library::Settings::Model>().getPackagesPath();
  m_addonsWatch.addPath(addons);
  QDirIterator it{
      addons, QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot,
      QDirIterator::NoIteratorFlags};
  while(it.hasNext())
  {
    it.next();
    QDir addon_dir = it.fileInfo().filePath();
    auto p = addon_dir.absolutePath();
    if(!m_addonsPaths.contains(p))
    {
      if(addon_dir.exists("addon.json"))
      {
        QFile addon_json{addon_dir.filePath("addon.json")};
        if(addon_json.open(QIODevice::ReadOnly))
        {
          auto doc = QJsonDocument::fromJson(score::mapAsByteArray(addon_json));
          if(doc.isObject() && doc.object()["kind"] == "addon")
          {
            m_addonsPaths.insert(p);
            setupAddon(p);
          }
        }
      }
    }
  }
}

void ApplicationPlugin::rescanNodes()
{
  SCORE_TODO;
  /*
  const auto& libpath = context.settings<Library::Settings::Model>().getPath();
  QString nodes = libpath + "/Nodes";
  m_nodesWatch.addPath(nodes);

  QDirIterator it{
      nodes,
      {"*.hpp", "*.cpp"},
      QDir::Filter::Files | QDir::Filter::NoDotAndDotDot,
      QDirIterator::Subdirectories};
  while (it.hasNext())
  {
    auto path = it.next();
    m_nodesWatch.addPath(path);
    setupNode(path);
  }
  */
}

void ApplicationPlugin::initialize()
{
  rescanNodes();
  rescanAddons();

  // If we don't do this, the linker will strip the whole Qt5QuickWidgets lib altogether:
  // delete new QQuickWidget;
}

void ApplicationPlugin::registerAddon(score::Plugin_QtInterface* p)
{
  qDebug() << "registerAddon => " << typeid(p).name();
  score::GUIApplicationInterface::instance().registerPlugin(*p);
  qDebug() << "JIT addon registered" << p;
}

bool ApplicationPlugin::setupAddon(const QString& addon)
{
  qDebug() << "Registering JIT addon" << addon;
  QFileInfo addonInfo{addon};
  auto addonFolderName = addonInfo.fileName();
  if(addonFolderName == "Nodes")
    return false;

  auto [json, cpp_files, files, flags] = loadAddon(addon);

  if(cpp_files.empty())
  {
    qDebug() << "Add-on has no cpp files";
    return false;
  }

  auto addon_files_path = generateAddonFiles(addonFolderName, addon, files);
  flags.push_back("-I" + addon.toStdString());
  flags.push_back("-I" + addon_files_path.toStdString());

  std::string id = json["key"].toString().remove(QChar('-')).toStdString();
  if(id.empty())
  {
    id = addonFolderName.remove(QChar('-')).remove(QChar(' ')).toStdString();
  }

  qDebug() << "Submittin JIT addon build job";
  m_compiler.submitJob(id, cpp_files, flags, CompilerOptions{false});
  return true;
}

bool ApplicationPlugin::setupNode(const QString& f)
{
  QFileInfo fi{f};
  if(fi.suffix() == "hpp" || fi.suffix() == "cpp")
  {
    if(QFile file{f}; file.open(QIODevice::ReadOnly))
    {
      auto node = file.readAll();
      constexpr auto make_uuid_s = "make_uuid";
      auto make_uuid = node.indexOf(make_uuid_s);
      if(make_uuid == -1)
        return false;
      int umin = node.indexOf('"', make_uuid + 9);
      if(umin == -1)
        return false;
      int umax = node.indexOf('"', umin + 1);
      if(umax == -1)
        return false;
      if((umax - umin) != 37)
        return false;
      auto uuid = QString{node.mid(umin + 1, 36)};
      uuid.remove(QChar('-'));

      node.append(
          R"_(
            #include <score/plugins/PluginInstances.hpp>

            SCORE_EXPORT_PLUGIN(Control::score_generic_plugin<Node>)
            )_");

      qDebug() << "Registering JIT node" << f;
      m_compiler.submitJob(
          uuid.toStdString(), node.toStdString(), {}, CompilerOptions{false});
      return true;
    }
  }
  return false;
}

void ApplicationPlugin::updateAddon(const QString& f)
{
  qDebug() << f;
}

}
