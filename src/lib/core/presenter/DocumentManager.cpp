// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentManager.hpp"

#include <score/application/ApplicationComponents.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/command/CommandStack.hpp>
#include <core/command/CommandStackSerialization.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/QRecentFilesMenu.h>
#include <core/view/Window.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QMessageBox>
#include <QSaveFile>
#include <QSettings>
#include <QStringList>
#include <QStandardPaths>

#include <multi_index/hashed_index.hpp>
#include <multi_index/identity.hpp>
#include <multi_index/mem_fun.hpp>
#include <multi_index/member.hpp>
#include <multi_index_container.hpp>
#include <wobjectimpl.h>

#include <utility>
W_OBJECT_IMPL(score::DocumentManager)
namespace score
{
struct LoadedPluginVersions
{
  UuidKey<score::Plugin> plugin;
  score::Version version;
};
}

namespace bmi = multi_index;
using LocalPluginVersionsMap = bmi::multi_index_container<
    score::Plugin_QtInterface*,
    bmi::indexed_by<bmi::hashed_unique<bmi::const_mem_fun<
        score::Plugin_QtInterface,
        UuidKey<score::Plugin>,
        &score::Plugin_QtInterface::key>>>>;
using LoadedPluginVersionsMap = bmi::multi_index_container<
    score::Plugin_QtInterface*,
    bmi::indexed_by<bmi::hashed_unique<bmi::member<
        score::LoadedPluginVersions,
        UuidKey<score::Plugin>,
        &score::LoadedPluginVersions::plugin>>>>;

namespace std
{
template <>
struct hash<score::LoadedPluginVersions>
{
  std::size_t operator()(const score::LoadedPluginVersions& kagi) const noexcept
  {
    return std::hash<UuidKey<score::Plugin>>{}(kagi.plugin);
  }
};
}

namespace
{

static QDir getDialogDirectory(score::Document* current)
{
  if(current)
  {
    auto& doc = *current;
    QFileInfo file = doc.metadata().fileName();
    if(auto dir = file.absoluteDir(); dir.exists())
      return dir;
  }

#if !defined(__EMSCRIPTEN__)
  if (QSettings s; s.contains("score/last_open_doc"))
  {
    QFileInfo file = s.value("score/last_open_doc").toString();
    if(auto dir = file.absoluteDir(); dir.exists())
      return dir;
  }
#endif

  auto docs = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
  if(!docs.isEmpty())
    return docs.front();

  auto home = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  if(!home.isEmpty())
    return home.front();

  return {};
}

}

namespace score
{
DocumentManager::DocumentManager(score::View* view, QObject* parentPresenter)
    : m_view{view}, m_builder{parentPresenter, view}
{
}

void DocumentManager::init(const score::GUIApplicationContext& ctx)
{
  if (m_view)
  {
    connect(
        m_view,
        &View::activeDocumentChanged,
        this,
        [&](const Id<DocumentModel>& doc) {
          prepareNewDocument(ctx);
          auto it = ossia::find_if(
              m_documents, [&](auto other) { return other->model().id() == doc; });
          setCurrentDocument(ctx, it != m_documents.end() ? *it : nullptr);
        },
        Qt::QueuedConnection);

    connect(m_view, &View::closeRequested, this, [&](const Id<DocumentModel>& doc) {
      auto it
          = ossia::find_if(m_documents, [&](auto other) { return other->model().id() == doc; });
      SCORE_ASSERT(it != m_documents.end());
      closeDocument(ctx, **it);
    });

    m_recentFiles = new QRecentFilesMenu{tr("Recent files"), nullptr};

#if !defined(__EMSCRIPTEN__)
    QSettings settings("OSSIA", "score");
    m_recentFiles->restoreState(settings.value("RecentFiles").toByteArray());
    connect(m_recentFiles, &QRecentFilesMenu::recentFileTriggered, this, [&](const QString& f) {
      loadFile(ctx, f);
    });
#endif
  }
}

DocumentManager::~DocumentManager()
{
  saveRecentFilesState();

  // The documents have to be deleted before the application context plug-ins.
  // This is because the Local device has to be deleted last in
  // ApplicationPlugin.
  for (auto document : m_documents)
  {
    document->deleteLater();
  }

  m_documents.clear();
  m_currentDocument = nullptr;
  if (m_recentFiles)
    delete m_recentFiles;
}

Document* DocumentManager::setupDocument(const score::GUIApplicationContext& ctx, Document* doc)
{
  if (doc)
  {
    auto it = ossia::find(m_documents, doc);
    if (it == m_documents.end())
      m_documents.push_back(doc);

    if (m_view)
    {
      m_view->addDocumentView(doc->view());
      connect(&doc->metadata(), &DocumentMetadata::fileNameChanged, this, [=](const QString& s) {
        m_view->on_fileNameChanged(doc->view(), s);
      });
    }
    setCurrentDocument(ctx, doc);
  }
  else
  {
    setCurrentDocument(ctx, m_documents.empty() ? nullptr : m_documents.front());
  }

  return doc;
}

void DocumentManager::setCurrentDocument(const score::GUIApplicationContext& ctx, Document* doc)
{
  if (doc == m_currentDocument)
    return;

  auto old = m_currentDocument;
  m_currentDocument = doc;

  if (doc)
  {
    for (auto& panel : ctx.panels())
    {
      panel.setModel(doc->context());
    }
  }
  else
  {
    for (auto& panel : ctx.panels())
    {
      panel.setModel(std::nullopt);
    }
  }

  for (auto& ctrl : ctx.guiApplicationPlugins())
  {
    ctrl->on_documentChanged(old, m_currentDocument);
  }
  documentChanged(m_currentDocument);
}

bool DocumentManager::closeDocument(const score::GUIApplicationContext& ctx, Document& doc)
{
  // Warn the user if he might loose data
  if (!doc.commandStack().isAtSavedIndex())
  {
    QMessageBox msgBox;
    msgBox.setText(tr("The document has been modified."));
    msgBox.setInformativeText(tr("Do you want to save your changes?"));
    msgBox.setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_question.png")));

    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    int ret = msgBox.exec();
    switch (ret)
    {
      case QMessageBox::Save:
        if (saveDocument(doc))
          break;
        else
          return false;
      case QMessageBox::Discard:
        // Do nothing
        break;
      case QMessageBox::Cancel:
        return false;
        break;
      default:
        break;
    }
  }

  // Close operation
  forceCloseDocument(ctx, doc);
  return true;
}

void DocumentManager::forceCloseDocument(const score::GUIApplicationContext& ctx, Document& doc)
{
  for (auto plug : doc.model().pluginModels())
  {
    plug->on_documentClosing();
  }

  if (m_view)
    m_view->closeDocument(doc.view());

  QPointer<Document> d = &doc;

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  ossia::remove_one(m_documents, &doc);
  setCurrentDocument(ctx, !m_documents.empty() ? m_documents.back() : nullptr);

  if(d)
  {
    delete &doc;
  }
}

bool DocumentManager::saveDocument(Document& doc)
{
  auto savename = doc.metadata().fileName();

  if (savename.indexOf(tr("Untitled")) == 0)
  {
    saveDocumentAs(doc);
  }
  else if (savename.size() != 0)
  {
    QSaveFile f{savename};
    f.open(QIODevice::WriteOnly);
    if (savename.indexOf(".scorebin") != -1)
      f.write(doc.saveAsByteArray());
    else
    {
      JSONReader w;
      w.buffer.Reserve(1024 * 1024 * 16);
      doc.saveAsJson(w);

      f.write(w.buffer.GetString(), w.buffer.GetSize());
    }
    f.commit();

    m_recentFiles->addRecentFile(savename);
    saveRecentFilesState();
  }

  return true;
}

bool DocumentManager::saveDocumentAs(Document& doc)
{
  if (!m_view)
    return false;
  QFileDialog d{m_view, tr("Save Document As")};
  QString binFilter{tr("Binary (*.scorebin)")};
  QString jsonFilter{tr("Score (*.score)")};
  QStringList filters;
  filters << jsonFilter << binFilter;

  d.setNameFilters(filters);
  d.setOption(QFileDialog::DontConfirmOverwrite, false);
  d.setFileMode(QFileDialog::AnyFile);
  d.setAcceptMode(QFileDialog::AcceptSave);
  d.setDirectory(getDialogDirectory(&doc));

  d.selectFile("untitled.score");
  if (d.exec())
  {
    auto files = d.selectedFiles();
    QString savename = files.first();
    auto suf = d.selectedNameFilter();

    if (!savename.isEmpty())
    {
      if (suf == binFilter)
      {
        if (!savename.contains(".scorebin"))
          savename += ".scorebin";
      }
      else
      {
        if (!savename.contains(".scorejson") && !savename.contains(".score"))
          savename += ".score";
      }

      QSaveFile f{savename};
      f.open(QIODevice::WriteOnly);
      doc.metadata().setFileName(savename);
      if (savename.indexOf(".scorebin") != -1)
        f.write(doc.saveAsByteArray());
      else
      {
        JSONReader w;
        w.buffer.Reserve(1024 * 1024 * 16);
        doc.saveAsJson(w);

        f.write(w.buffer.GetString(), w.buffer.GetSize());
      }
      f.commit();

      m_recentFiles->addRecentFile(savename);
      saveRecentFilesState();
    }
    return true;
  }
  return false;
}

bool DocumentManager::saveStack()
{
  if (!m_view)
    return false;
  QFileDialog d{m_view, tr("Save Stack As")};
  d.setNameFilters({"*.stack"});
  d.setOption(QFileDialog::DontConfirmOverwrite, false);
  d.setFileMode(QFileDialog::AnyFile);
  d.setAcceptMode(QFileDialog::AcceptSave);

  if (d.exec())
  {
    auto files = d.selectedFiles();
    QString savename = files.first();
    if (!savename.isEmpty())
    {
      if (!savename.contains(".stack"))
        savename += ".stack";

      QSaveFile f{savename};
      f.open(QIODevice::WriteOnly);

      f.reset();
      DataStream::Serializer ser(&f);
      ser.readFrom(currentDocument()->id());
      ser.readFrom(currentDocument()->commandStack());
      f.commit();
    }
    return true;
  }
  return false;
}

Document* DocumentManager::loadStack(const score::GUIApplicationContext& ctx)
{
  if (!m_view)
    return nullptr;

  QString loadname = QFileDialog::getOpenFileName(m_view, tr("Open Stack"), QString(), "*.stack");
  if (!loadname.isEmpty() && (loadname.indexOf(".stack") != -1))
  {
    return loadStack(ctx, loadname);
  }

  return nullptr;
}

Document*
DocumentManager::loadStack(const score::GUIApplicationContext& ctx, const QString& loadname)
{
  QFile cmdF{loadname};

  if (cmdF.open(QIODevice::ReadOnly))
  {
    QByteArray cmdArr{cmdF.readAll()};
    cmdF.close();

    DataStream::Deserializer writer(cmdArr);

    Id<DocumentModel> id;
    writer.writeTo(id);

    prepareNewDocument(ctx);
    auto doc = m_builder.newDocument(ctx, id, *ctx.interfaces<DocumentDelegateList>().begin());
    setupDocument(ctx, doc);

    loadCommandStack(ctx.components, writer, doc->commandStack(), [doc](auto cmd) {
      cmd->redo(doc->context());
    });
    return doc;
  }

  return nullptr;
}

Document* DocumentManager::loadFile(const score::GUIApplicationContext& ctx)
{
  if (!m_view)
    return nullptr;

  QString loadname = QFileDialog::getOpenFileName(
      m_view, tr("Open"), getDialogDirectory(nullptr).absolutePath(), "Scores (*.scorebin *.score *.scorejson)");

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  s.setValue("score/last_open_doc", QFileInfo(loadname).absoluteDir().path());
#endif

  return loadFile(ctx, loadname);
}

Document*
DocumentManager::loadFile(const score::GUIApplicationContext& ctx, const QString& fileName)
{
  Document* doc{};
  if (!fileName.isEmpty()
      && (fileName.indexOf(".scorebin") != -1 || fileName.indexOf(".scorejson") != -1
          || fileName.indexOf(".score") != 1))
  {

    if (QFile f{fileName}; f.open(QIODevice::ReadOnly))
    {
      f.close();
      if (m_recentFiles)
      {
        m_recentFiles->addRecentFile(fileName);
        saveRecentFilesState();
      }

      if (fileName.indexOf(".scorebin") != -1)
      {
        doc = loadDocument(ctx, fileName, *ctx.interfaces<DocumentDelegateList>().begin());
      }
      else if (fileName.indexOf(".score") != -1)
      {
        doc = loadDocument(ctx, fileName, *ctx.interfaces<DocumentDelegateList>().begin());
      }
    }
  }

  return doc;
}

void DocumentManager::prepareNewDocument(const score::GUIApplicationContext& ctx)
{
  m_preparingNewDocument = true;
  for (GUIApplicationPlugin* appPlugin : ctx.guiApplicationPlugins())
  {
    appPlugin->prepareNewDocument();
  }
  m_preparingNewDocument = false;
}

bool DocumentManager::closeAllDocuments(const score::GUIApplicationContext& ctx)
{
  while (!m_documents.empty())
  {
    bool b = closeDocument(ctx, *m_documents.back());
    if (!b)
      return false;
  }

  return true;
}

bool DocumentManager::preparingNewDocument() const
{
  return m_preparingNewDocument;
}

bool DocumentManager::checkAndUpdateJson(
    rapidjson::Value& obj,
    const score::GUIApplicationContext& ctx)
{
  if (obj.GetType() != rapidjson::kObjectType)
    return false;

  // Check the version
  Version loaded_version{0};
  auto it = obj.FindMember("Version");
  if (it != obj.MemberEnd())
    loaded_version = Version{it->value.GetInt()};

  LocalPluginVersionsMap local_plugins;
  for (const auto& plug : ctx.addons())
  {
    local_plugins.insert(plug.plugin);
  }

  std::vector<LoadedPluginVersions> loading_plugins;
  auto plugin_it = obj.FindMember("Plugins");
  if (plugin_it != obj.MemberEnd())
  {
    if (plugin_it->value.IsArray())
    {
      for (const auto& plugin_val : plugin_it->value.GetArray())
      {
        const auto& plugin_obj = plugin_val.GetObject();
        auto plugin_key_it = plugin_obj.FindMember("Key");
        if (plugin_key_it == plugin_obj.MemberEnd())
          continue;
        QByteArray key_arr = QByteArray::fromRawData(
            plugin_key_it->value.GetString(), plugin_key_it->value.GetStringLength());
        auto plugin_key = UuidKey<score::Plugin>::fromString(key_arr);

        Version plugin_version{0};
        auto plugin_ver_it = plugin_obj.FindMember("Version");
        if (plugin_ver_it != plugin_obj.MemberEnd())
          plugin_version = Version{plugin_ver_it->value.GetInt()};

        loading_plugins.push_back({plugin_key, plugin_version});
      }
    }
    else
    {
      return false;
    }
  }

  // A file is loadable, if the main version
  // and all the plugin versions are <= to the current version,
  // and all the plug-ins are available.

  // Check the main document
  bool mainLoadable = true;
  if (loaded_version > ctx.applicationSettings.saveFormatVersion)
  {
    mainLoadable = false;
  }
  else if (loaded_version < ctx.applicationSettings.saveFormatVersion)
  {
    // TODO update main
    auto res = updateJson(obj, loaded_version, ctx.applicationSettings.saveFormatVersion);
    if (!res)
    {
      return false;
    }
  }

  // Check the plug-ins
  bool pluginsAvailable = true;
  bool pluginsLoadable = true;

  auto& local_map = local_plugins.get<0>();
  for (const auto& plug : loading_plugins)
  {
    auto it = local_map.find(plug.plugin);
    if (it == local_map.end())
    {
      pluginsAvailable = false;
    }
    else
    {
      auto& current_local_plugin = *it;
      if (plug.version > current_local_plugin->version())
      {
        pluginsLoadable = false;
      }
      else if (plug.version < current_local_plugin->version())
      {
        current_local_plugin->updateSaveFile(obj, plug.version, current_local_plugin->version());
      }
    }
  }

  return mainLoadable && pluginsAvailable && pluginsLoadable;
}

bool DocumentManager::updateJson(rapidjson::Value& object, Version json_ver, Version score_ver)
{
  score::hash_map<Version, std::pair<Version, std::function<void(QJsonObject&)>>> conversions;
  /*
    conversions.insert(
      {Version{2}, {Version{3}, [] (const QJsonObject& obj)
       {
         // Add '@' between address and accessor

       }}});

    // For now just do from n to n+1
    // TODO do the algorithm that does n..n+1..n+2..etc.

    auto it = conversions.find(json_ver);
    if(it != conversions.end())
    {
      it.value().second(object);
      return true;
    }
    */
  return false;
}

void DocumentManager::saveRecentFilesState()
{
#if !defined(__EMSCRIPTEN__)
  if (m_recentFiles)
  {
    QSettings settings("OSSIA", "score");
    settings.setValue("RecentFiles", m_recentFiles->saveState());
    m_recentFiles->saveState();
  }
#endif
}

void DocumentManager::restoreDocuments(const score::GUIApplicationContext& ctx)
{
  for (const RestorableDocument& backup : DocumentBackups::restorableDocuments())
  {
    restoreDocument(
        ctx,
        backup.filePath,
        backup.doc,
        backup.commands,
        *ctx.interfaces<DocumentDelegateList>().begin());
  }
}

Id<score::DocumentModel> getStrongId(const std::vector<score::Document*>& v)
{
  using namespace std;
  vector<int32_t> ids(v.size()); // Map reduce

  transform(v.begin(), v.end(), ids.begin(), [](const auto elt) { return elt->id().val(); });

  return Id<score::DocumentModel>{score::random_id_generator::getNextId(ids)};
}

Id<score::DocumentPlugin> getStrongId(const std::vector<score::DocumentPlugin*>& v)
{
  using namespace std;
  vector<int32_t> ids(v.size()); // Map reduce

  transform(v.begin(), v.end(), ids.begin(), [](const auto elt) { return elt->id().val(); });

  return Id<score::DocumentPlugin>{score::random_id_generator::getNextId(ids)};
}
}
