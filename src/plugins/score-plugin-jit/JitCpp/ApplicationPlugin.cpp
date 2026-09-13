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
#include <QRegularExpression>

#include <optional>
#include <vector>

namespace Jit
{
namespace
{
//! Qualified name of the type whose body contains `pos`.
//!
//! An avendish object is an ordinary C++ type, and the factory has to name it.
//! All the file gives us is where its uuid sits, so walk the scopes that are
//! still open at that point. Strings, character literals and comments are
//! skipped because their braces would unbalance the count.
QString enclosingTypeName(const QString& src, int pos)
{
  static const QRegularExpression decl{
      R"(\b(namespace|struct|class)\s+([A-Za-z_][A-Za-z_0-9]*(?:\s*::\s*[A-Za-z_][A-Za-z_0-9]*)*))"};

  struct Scope
  {
    QString name;
    bool isType{};
  };
  std::vector<Scope> open;
  std::optional<Scope> pending;

  auto matches = decl.globalMatch(src);
  auto next = matches.hasNext() ? matches.next() : QRegularExpressionMatch{};

  for(int i = 0; i < pos && i < src.size(); ++i)
  {
    if(next.hasMatch() && next.capturedStart() == i)
    {
      QString name = next.captured(2);
      name.remove(QChar(' '));
      pending = Scope{name, next.captured(1) != QLatin1String("namespace")};
      i = next.capturedEnd() - 1;
      next = matches.hasNext() ? matches.next() : QRegularExpressionMatch{};
      continue;
    }

    const QChar c = src[i];
    if(c == '/' && i + 1 < src.size())
    {
      if(src[i + 1] == '/')
      {
        i = src.indexOf(QChar('\n'), i);
        if(i < 0)
          break;
        continue;
      }
      if(src[i + 1] == '*')
      {
        i = src.indexOf(QStringLiteral("*/"), i + 2);
        if(i < 0)
          break;
        ++i;
        continue;
      }
    }
    else if(c == '"' || c == '\'')
    {
      const QChar quote = c;
      for(++i; i < src.size(); ++i)
      {
        if(src[i] == QChar('\\'))
          ++i;
        else if(src[i] == quote)
          break;
      }
      continue;
    }
    else if(c == '{')
    {
      open.push_back(pending ? *pending : Scope{});
      pending.reset();
    }
    else if(c == '}')
    {
      if(!open.empty())
        open.pop_back();
    }
    else if(c == ';')
    {
      pending.reset();
    }
  }

  QStringList parts;
  bool sawType = false;
  for(const Scope& s : open)
  {
    if(s.name.isEmpty())
      continue;
    parts << s.name;
    if(s.isType)
      sawType = true;
  }
  if(!sawType)
    return {};
  return parts.join(QStringLiteral("::"));
}
}
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
          QTimer::singleShot(1000, qApp, [] {
            score::GUIApplicationInterface::instance().forceExit();
          });
        });

        if(!setupNode(node_to_compile))
          exit(1);
      }
      else if(QFile::exists(addon_to_compile))
      {
        con(m_compiler, &AddonCompiler::jobCompleted, this, [this](auto addon) {
          registerAddon(addon);
          QTimer::singleShot(1000, qApp, [] {
            score::GUIApplicationInterface::instance().forceExit();
          });
        });

        if(!setupAddon(addon_to_compile))
          exit(1);
      }
      con(m_compiler, &AddonCompiler::jobFailed, this, [] {
        score::GUIApplicationInterface::instance().requestExit();
        QTimer::singleShot(500, [] { QCoreApplication::exit(1); });
      });
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

  auto [json, target, cpp_files, files, flags] = loadAddon(addon);

  if(cpp_files.empty())
  {
    qDebug() << "Add-on has no cpp files";
    return false;
  }

  // The generated headers are named after the CMake target, which is how the
  // add-on's sources include them; the folder name is only a fallback for add-ons
  // with no CMakeLists to read the target from.
  auto addon_files_path
      = generateAddonFiles(!target.isEmpty() ? target : addonFolderName, addon, files);
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

      // Avendish objects spell it halp_meta(uuid, "..."); score's own generic
      // nodes used make_uuid("...").
      int uuid_decl = node.indexOf("halp_meta(uuid");
      int skip = sizeof("halp_meta(uuid") - 1;
      if(uuid_decl == -1)
      {
        uuid_decl = node.indexOf("make_uuid");
        skip = sizeof("make_uuid") - 1;
      }
      if(uuid_decl == -1)
        return false;
      int umin = node.indexOf('"', uuid_decl + skip);
      if(umin == -1)
        return false;
      int umax = node.indexOf('"', umin + 1);
      if(umax == -1)
        return false;
      if((umax - umin) != 37)
        return false;
      const auto uuid = QString{node.mid(umin + 1, 36)};

      const QString source = QString::fromUtf8(node);
      const QString type = enclosingTypeName(source, umin);
      if(type.isEmpty())
      {
        qDebug() << "Could not find the object declaring the uuid in" << f;
        return false;
      }

      // The same pair of translation units CMake generates for an add-on with a
      // single avendish object: prototype.cpp.in's custom_factories<T>
      // specialisation, and plugin_prototype.cpp.in's plug-in around it.
      QString tu = source;
      tu += QStringLiteral(R"_(
#include <Avnd/Factories.hpp>

namespace oscr
{
template <>
void custom_factories<%1>(
    std::vector<score::InterfaceBase*>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  oscr::instantiate_fx<%1>(fx, ctx, key);
}
}

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score_plugin_engine.hpp>

struct score_jit_node final
    : public score::FactoryInterface_QtInterface
    , public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "%2")

  std::vector<score::InterfaceBase*> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override
  {
    std::vector<score::InterfaceBase*> fx;
    ::oscr::custom_factories<%1>(fx, ctx, key);
    return fx;
  }

  std::vector<score::PluginKey> required() const override
  {
    return {score_plugin_engine::static_key()};
  }
};

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_jit_node)
)_")
                .arg(type, uuid);

      QString id = uuid;
      id.remove(QChar('-'));
      qDebug() << "Registering JIT node" << f << "as" << type;
      m_compiler.submitJob(
          id.toStdString(), tu.toStdString(), {}, CompilerOptions{false});
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
