// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentManager.hpp"

#include <score/application/ApplicationComponents.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/Zip.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/application/OpenDocumentsFile.hpp>
#include <core/command/CommandStackSerialization.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/ProjectInfo.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/QRecentFilesMenu.h>
#include <core/view/Window.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QByteArray>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QIODevice>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <wobjectimpl.h>

#include <unordered_map>
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

using LocalPluginVersionsMap
    = ossia::hash_map<UuidKey<score::Plugin>, score::Plugin_QtInterface*>;
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
    const QFileInfo file{doc.metadata().fileName()};
    if(file.exists())
    {
      if(auto dir = file.absoluteDir(); dir.exists())
        return dir;
    }
  }

#if !defined(__EMSCRIPTEN__)
  if(QSettings s; s.contains("score/last_open_doc"))
  {
    const QFileInfo file{s.value("score/last_open_doc").toString()};
    if(file.isDir() && file.exists())
      return QDir{file.absoluteFilePath()};
    else if(auto dir = file.absoluteDir(); dir.exists())
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
    : m_view{view}
    , m_builder{parentPresenter, view}
{
}

void DocumentManager::init(const score::GUIApplicationContext& ctx)
{
  if(m_view)
  {
    connect(
        m_view, &View::activeDocumentChanged, this, [&](const Id<DocumentModel>& doc) {
      prepareNewDocument(ctx);
      auto it = ossia::find_if(
          m_documents, [&](auto other) { return other->model().id() == doc; });
      setCurrentDocument(ctx, it != m_documents.end() ? *it : nullptr);
    }, Qt::QueuedConnection);

    connect(m_view, &View::closeRequested, this, [&](const Id<DocumentModel>& doc) {
      auto it = ossia::find_if(
          m_documents, [&](auto other) { return other->model().id() == doc; });
      SCORE_ASSERT(it != m_documents.end());
      closeDocument(ctx, **it);
    });

    m_recentFiles = new QRecentFilesMenu{tr("Recent files"), nullptr};

#if !defined(__EMSCRIPTEN__)
    QSettings settings;
    m_recentFiles->restoreState(settings.value("RecentFiles").toByteArray());
    connect(
        m_recentFiles, &QRecentFilesMenu::recentFileTriggered, this,
        [&](const QString& f) { loadFile(ctx, f); });
#endif
  }
}

DocumentManager::~DocumentManager()
{
  saveRecentFilesState();

  // The documents have to be deleted before the application context plug-ins.
  // This is because the Local device has to be deleted last in
  // ApplicationPlugin.
  for(auto document : m_documents)
  {
    // Reverse creation order, to match ~DocumentModel (e.g. the execution
    // plugin must be torn down before the device explorer plugin).
    auto& plugs = document->model().pluginModels();
    for(auto it = plugs.rbegin(); it != plugs.rend(); ++it)
    {
      (*it)->on_documentClosing();
    }
  }

  for(auto document : m_documents)
  {
    document->deleteLater();
  }

  m_documents.clear();
  m_currentDocument = nullptr;
  if(m_recentFiles)
    delete m_recentFiles;
}

Document*
DocumentManager::setupDocument(const score::GUIApplicationContext& ctx, Document* doc)
{
  if(doc)
  {
    auto it = ossia::find(m_documents, doc);
    if(it == m_documents.end())
      m_documents.push_back(doc);

    if(m_view)
    {
      m_view->addDocumentView(doc->view());
      connect(
          &doc->metadata(), &DocumentMetadata::fileNameChanged, this,
          [this, doc](const QString& s) { m_view->on_fileNameChanged(doc->view(), s); });
    }
    setCurrentDocument(ctx, doc);

    doc->ready();
  }
  else
  {
    setCurrentDocument(ctx, m_documents.empty() ? nullptr : m_documents.front());
  }

  return doc;
}

void DocumentManager::setCurrentDocument(
    const score::GUIApplicationContext& ctx, Document* doc)
{
  if(doc == m_currentDocument)
    return;

  auto old = m_currentDocument;
  m_currentDocument = doc;

  if(doc)
  {
    for(auto& panel : ctx.panels())
    {
      panel.setModel(doc->context());
    }
  }
  else
  {
    for(auto& panel : ctx.panels())
    {
      panel.setModel(std::nullopt);
    }
  }

  for(auto& ctrl : ctx.guiApplicationPlugins())
  {
    ctrl->on_documentChanged(old, m_currentDocument);
  }
  documentChanged(m_currentDocument);
}

bool DocumentManager::closeDocument(
    const score::GUIApplicationContext& ctx, Document& doc)
{
  // Warn the user if he might loose data. Only when there is a user: with
  // applicationSettings.gui false (headless, --script, offscreen QPA) nothing
  // can answer a modal, and QMessageBox::exec() aborts instead of returning.
  // Every score::MessageBox helper already guards on this same flag; this call
  // site was the one raw QMessageBox left, which is why a scripted /exit on a
  // modified document died in teardown with SIGABRT. No GUI means no one to
  // save for, so proceed as Discard -- the same outcome forceExit() already
  // produces, since it quits 500ms later whatever the answer would have been.
  if(!doc.commandStack().isAtSavedIndex() && ctx.applicationSettings.gui)
  {
    QMessageBox msgBox;
    msgBox.setText(tr("The document has been modified."));
    msgBox.setInformativeText(tr("Do you want to save your changes?"));
    msgBox.setIconPixmap(
        score::get_pixmap(QStringLiteral(":/icons/message_question.png")));

    msgBox.setStandardButtons(
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    int ret = msgBox.exec();
    switch(ret)
    {
      case QMessageBox::Save:
        if(saveDocument(doc))
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

void DocumentManager::forceCloseDocument(
    const score::GUIApplicationContext& ctx, Document& doc)
{
  // Clear the plug-ins, in reverse creation order: same rule as ~DocumentModel
  // and ~DocumentManager. The execution plug-in has to be torn down before the
  // device explorer one, whose on_documentClosing() disconnects every device --
  // which destroys the ossia parameters that the still-running execution graph
  // holds raw pointers to in ossia::inlet::address.
  auto& plugs = doc.model().pluginModels();
  for(auto it = plugs.rbegin(); it != plugs.rend(); ++it)
  {
    (*it)->on_documentClosing();
  }

  // Clear the app plugins
  for(auto plug : ctx.guiApplicationPlugins())
  {
    plug->on_closeDocument(doc);
  }

  doc.blockAllSignals();

  if(m_view)
    m_view->closeDocument(doc.view());

  // Clear the data model
  doc.model().on_documentClosing();

  // Delete the document
  QPointer<Document> d = &doc;

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  ossia::remove_one(m_documents, &doc);
  setCurrentDocument(ctx, !m_documents.empty() ? m_documents.back() : nullptr);

  if(d)
  {
    delete &doc;
  }
}

static void writeJsonToFile(QSaveFile& f, const rapidjson::StringBuffer& buffer)
{
  if(qEnvironmentVariableIsSet("SCORE_PRETTIFY_JSON"))
  {
    [[unlikely]];
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromRawData(buffer.GetString(), buffer.GetSize()));
    f.write(doc.toJson(QJsonDocument::JsonFormat::Indented));
  }
  else
  {
    [[likely]];
    f.write(buffer.GetString(), buffer.GetSize());
  }
}

namespace
{
// Refresh the project information that depends on the act of saving:
// last-saved date and, unless the user chose a fixed image, a capture of the
// document view as thumbnail.
void updateProjectInfoBeforeSave(Document& doc)
{
  auto info = doc.context().findPlugin<ProjectInfo::Model>();
  if(!info)
    return;

  info->setLastSaved(QDateTime::currentDateTime());

  if(!info->getAutomaticThumbnail())
    return;
  auto view = doc.view();
  if(!view)
    return;
  auto widget = view->viewDelegate().getWidget();
  if(!widget || !widget->isVisible() || widget->width() < 64 || widget->height() < 64)
    return;

  const QImage capture = widget->grab().toImage();
  if(capture.isNull())
    return;

  // A view that could not be captured (e.g. an OpenGL viewport on some
  // platforms) comes back as a flat color: keep the previous thumbnail then.
  {
    const QImage small = capture.scaled(16, 16, Qt::IgnoreAspectRatio);
    const QRgb first = small.pixel(0, 0);
    bool flat = true;
    for(int y = 0; y < small.height() && flat; y++)
      for(int x = 0; x < small.width(); x++)
        if(small.pixel(x, y) != first)
        {
          flat = false;
          break;
        }
    if(flat)
      return;
  }
  info->setThumbnail(ProjectInfo::Model::encodeThumbnail(capture));
}
}

Document* DocumentManager::newDocumentFromTemplate(
    const score::GUIApplicationContext& ctx, const QString& templatePath)
{
  // An archived project needs its folder for its media: it opens as a real file
  if(templatePath.endsWith(".zip", Qt::CaseInsensitive))
    return openArchive(ctx, templatePath);

  auto& doctypes = ctx.interfaces<DocumentDelegateList>();
  if(doctypes.empty())
    return nullptr;

  prepareNewDocument(ctx);
  auto doc = m_builder.newDocumentFromTemplate(
      ctx, Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
      templatePath, *doctypes.begin());
  if(!doc)
    return nullptr;
  return setupDocument(ctx, doc);
}

bool DocumentManager::saveDocument(Document& doc)
{
  auto savename = doc.metadata().fileName();

  if(QFileInfo{savename}.fileName().startsWith(tr("Untitled")))
  {
    saveDocumentAs(doc);
  }
  else if(savename.size() != 0)
  {
    QSaveFile f{savename};
    if(!f.open(QIODevice::WriteOnly))
      return false;

    updateProjectInfoBeforeSave(doc);

    if(savename.indexOf(".scorebin") != -1)
    {
      f.write(doc.saveAsByteArray());
    }
    else
    {
      JSONReader w;
      w.buffer.Reserve(1024 * 1024 * 16);
      doc.saveAsJson(w);
      writeJsonToFile(f, w.buffer);
    }

    if(f.commit())
    {
      if(m_recentFiles)
      {
        m_recentFiles->addRecentFile(savename);
        saveRecentFilesState();
      }
    }
    else
    {
      score::warning(
          nullptr, tr("Error while saving"),
          tr("Score could not save the file %1. Check that you have correct "
             "permissions.")
              .arg(savename));
      return false;
    }
  }

  return true;
}

bool DocumentManager::saveDocumentAs(Document& doc)
{
  if(!m_view)
    return false;

#if defined(__EMSCRIPTEN__)
  // wasm cannot write to a local path: hand the serialized document to the
  // browser as a download via the async saveFileContent API. Default to the
  // JSON .score format (the binary format is desktop-oriented).
  updateProjectInfoBeforeSave(doc);
  JSONReader w;
  w.buffer.Reserve(1024 * 1024 * 16);
  doc.saveAsJson(w);
  const QByteArray data{w.buffer.GetString(), (int)w.buffer.GetSize()};

  QString hint = doc.metadata().fileName();
  if(hint.isEmpty() || QFileInfo{hint}.fileName().startsWith(tr("Untitled")))
    hint = "untitled.score";
  QFileDialog::saveFileContent(data, hint, m_view);
  return true;
#else
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
  if(d.exec())
  {
    auto files = d.selectedFiles();
    QString savename = files.first();
    auto suf = d.selectedNameFilter();

    if(!savename.isEmpty())
    {
      if(suf == binFilter)
      {
        if(!savename.contains(".scorebin"))
          savename += ".scorebin";
      }
      else
      {
        if(!savename.contains(".scorejson") && !savename.contains(".score"))
          savename += ".score";
      }

      return saveDocumentAs(doc, savename);
    }
    return true;
  }
  return false;
#endif
}

bool DocumentManager::saveDocumentAs(Document& doc, const QString& savename)
{
  QSaveFile f{savename};
  if(!f.open(QIODevice::WriteOnly))
    return false;

  // Let the plug-ins fix up whatever the document stores relatively to its own
  // folder before that folder changes under them. Runs while the metadata
  // still holds the old file name, so paths can still be resolved from it.
  //
  // Through the components rather than GUIAppContext(): that one downcasts the
  // running ApplicationInterface, and score::MockApplication is not a GUI one.
  for(auto* plug : score::AppComponents().guiApplicationPlugins())
    plug->on_documentSaveAs(doc, savename);

  doc.metadata().setFileName(savename);
  updateProjectInfoBeforeSave(doc);

  if(savename.indexOf(".scorebin") != -1)
    f.write(doc.saveAsByteArray());
  else
  {
    JSONReader w;
    w.buffer.Reserve(1024 * 1024 * 16);
    doc.saveAsJson(w);

    writeJsonToFile(f, w.buffer);
  }

  if(f.commit())
  {
    if(m_recentFiles)
    {
      m_recentFiles->addRecentFile(savename);
      saveRecentFilesState();
    }
    doc.backupManager()->updateBackupData();
    return true;
  }
  else
  {
    score::warning(
        nullptr, tr("Error while saving"),
        tr("Score could not save the file %1. Check that you have correct "
           "permissions.")
            .arg(savename));
    return false;
  }
}

bool DocumentManager::saveStack()
{
  if(!m_view)
    return false;
  auto doc = currentDocument();
  if(!doc)
    return false;
  QFileDialog d{m_view, tr("Save Stack As")};
  d.setNameFilters({"*.stack"});
  d.setOption(QFileDialog::DontConfirmOverwrite, false);
  d.setFileMode(QFileDialog::AnyFile);
  d.setAcceptMode(QFileDialog::AcceptSave);

  if(d.exec())
  {
    auto files = d.selectedFiles();
    QString savename = files.first();
    if(!savename.isEmpty())
    {
      if(!savename.contains(".stack"))
        savename += ".stack";

      QSaveFile f{savename};
      if(!f.open(QIODevice::WriteOnly))
        return false;

      f.reset();
      DataStream::Serializer ser(&f);
      ser.readFrom(doc->id());
      ser.readFrom(doc->commandStack());
      f.commit();
    }
    return true;
  }
  return false;
}

Document* DocumentManager::loadStack(const score::GUIApplicationContext& ctx)
{
  if(!m_view)
    return nullptr;

  QString loadname = QFileDialog::getOpenFileName(
      m_view, tr("Open Stack"), getDialogDirectory(nullptr).absolutePath(), "*.stack");
  if(!loadname.isEmpty() && (loadname.indexOf(".stack") != -1))
  {
    return loadStack(ctx, loadname);
  }

  return nullptr;
}

Document* DocumentManager::loadStack(
    const score::GUIApplicationContext& ctx, const QString& loadname)
{
  QFile cmdF{loadname};

  if(cmdF.open(QIODevice::ReadOnly))
  {
    QByteArray cmdArr{score::mapAsByteArray(cmdF)};

    DataStream::Deserializer writer(cmdArr);

    Id<DocumentModel> id;
    writer.writeTo(id);

    prepareNewDocument(ctx);
    auto doc = m_builder.newDocument(
        ctx, id, *ctx.interfaces<DocumentDelegateList>().begin());
    setupDocument(ctx, doc);

    loadCommandStack(ctx.components, writer, doc->commandStack(), [doc](auto cmd) {
      cmd->redo(doc->context());
      return true;
    });
    return doc;
  }

  return nullptr;
}

namespace
{
//! Asks where the folder of an archived project should be created
class ArchiveDestinationDialog final : public QDialog
{
public:
  ArchiveDestinationDialog(
      const QString& archive, const ZipArchiveSummary& summary, const QString& suggested,
      QWidget* parent)
      : QDialog{parent}
  {
    setWindowTitle(tr("Open project archive"));
    setMinimumWidth(520);

    auto lay = new QVBoxLayout{this};
    auto intro = new QLabel{
        tr("%1 holds a score and %2 file(s), %3 in total.\n"
           "Choose the folder where the project will be created:")
            .arg(QFileInfo{archive}.fileName())
            .arg(summary.files - 1)
            .arg(QLocale{}.formattedDataSize(qint64(summary.uncompressedSize))),
        this};
    intro->setWordWrap(true);
    lay->addWidget(intro);

    auto row = new QHBoxLayout;
    m_folder = new QLineEdit{suggested, this};
    row->addWidget(m_folder, 1);
    auto browse = new QPushButton{tr("Browse..."), this};
    row->addWidget(browse);
    lay->addLayout(row);

    auto buttons
        = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Create and open"));
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    lay->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, [this] {
      const QString dir = QFileDialog::getExistingDirectory(
          this, tr("Project folder"), QFileInfo{m_folder->text()}.absolutePath());
      if(!dir.isEmpty())
        m_folder->setText(dir);
    });
    connect(m_folder, &QLineEdit::textChanged, this, [=](const QString& t) {
      buttons->button(QDialogButtonBox::Ok)->setEnabled(!t.trimmed().isEmpty());
    });
    connect(m_folder, &QLineEdit::returnPressed, this, [this, buttons] {
      if(buttons->button(QDialogButtonBox::Ok)->isEnabled())
        accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  }

  QString folder() const { return QDir::cleanPath(m_folder->text().trimmed()); }

private:
  QLineEdit* m_folder{};
};
}

Document* DocumentManager::openArchive(
    const score::GUIApplicationContext& ctx, const QString& archive)
{
  if(!m_view)
    return nullptr;

  const auto summary = summarizeZipArchive(archive);
  if(!summary)
  {
    QMessageBox::warning(
        m_view, tr("Not a project archive"),
        tr("%1 does not contain a score.").arg(QFileInfo{archive}.fileName()));
    return nullptr;
  }

  // Suggest a sibling of the last project rather than a folder inside it
  const QString baseName = QFileInfo{archive}.completeBaseName();
  QDir suggestedParent = getDialogDirectory(nullptr);
  if(!suggestedParent.entryList({"*.score", "*.scorejson"}, QDir::Files).isEmpty())
    suggestedParent.cdUp();
  const QString suggested = suggestedParent.filePath(baseName);

  ArchiveDestinationDialog dialog{archive, *summary, suggested, m_view};
  if(dialog.exec() != QDialog::Accepted)
    return nullptr;

  const QString folder = dialog.folder();
  const QString scorePath = QDir{folder}.filePath(summary->scoreFile);

  // The folder may already hold this very project: offer to just open it
  if(const QDir dir{folder}; dir.exists() && !dir.isEmpty())
  {
    if(QFile::exists(scorePath))
    {
      QMessageBox box{m_view};
      box.setIcon(QMessageBox::Question);
      box.setWindowTitle(tr("Project already extracted"));
      box.setText(
          tr("%1 already contains this project.\nOpen it as it is, or extract the "
             "archive again and overwrite it?")
              .arg(folder));
      auto open = box.addButton(tr("Open existing"), QMessageBox::AcceptRole);
      auto overwrite = box.addButton(tr("Overwrite"), QMessageBox::DestructiveRole);
      box.addButton(QMessageBox::Cancel);
      box.setDefaultButton(open);
      box.exec();
      if(box.clickedButton() == open)
        return loadFile(ctx, scorePath);
      if(box.clickedButton() != overwrite)
        return nullptr;
    }
    else if(
        QMessageBox::question(
            m_view, tr("Folder not empty"),
            tr("%1 is not empty. Extract the project into it anyway?").arg(folder),
            QMessageBox::Yes | QMessageBox::Cancel)
        != QMessageBox::Yes)
    {
      return nullptr;
    }
  }

  QProgressDialog progress{
      tr("Extracting %1...").arg(QFileInfo{archive}.fileName()), tr("Cancel"), 0,
      summary->files, m_view};
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(300);

  QString error;
  const bool ok = extractZipArchive(archive, folder, error, [&](int done, int total) {
    progress.setMaximum(total);
    progress.setValue(done);
    QCoreApplication::processEvents();
    return !progress.wasCanceled();
  });
  progress.close();

  if(!ok)
  {
    QMessageBox::warning(m_view, tr("Extraction failed"), error);
    return nullptr;
  }

  QSettings s;
  s.setValue("score/last_open_doc", folder);
  return loadFile(ctx, scorePath);
}

Document* DocumentManager::loadFile(const score::GUIApplicationContext& ctx)
{
  if(!m_view)
    return nullptr;

  static const QString filter{"Scores (*.scorebin *.score *.scorejson *.zip)"};

#if defined(__EMSCRIPTEN__)
  // wasm has neither a synchronous file dialog nor a local filesystem: use the
  // async callback API, which delivers the picked file's bytes to the lambda.
  // ctx is the application context (app-lifetime) so capturing it is safe.
  QFileDialog::getOpenFileContent(
      filter, [this, &ctx](const QString& name, const QByteArray& data) {
    if(name.isEmpty() || data.isEmpty())
      return;
    const auto format
        = name.endsWith(".scorebin") ? DataStream::type() : JSONObject::type();
    auto& doctype = *ctx.interfaces<DocumentDelegateList>().begin();
    loadDocument(ctx, name, data, format, doctype);
  }, m_view);
  return nullptr;
#else
  QString loadname = QFileDialog::getOpenFileName(
      m_view, tr("Open"), getDialogDirectory(nullptr).absolutePath(), filter);

  if(loadname.isEmpty())
    return nullptr;

  QSettings s;
  s.setValue("score/last_open_doc", QFileInfo(loadname).absoluteDir().path());

  return loadFile(ctx, loadname);
#endif
}

Document* DocumentManager::loadFile(
    const score::GUIApplicationContext& ctx, const QString& fileName)
{
  if(fileName.endsWith(".zip", Qt::CaseInsensitive))
    return openArchive(ctx, fileName);

  Document* doc{};
  if(!fileName.isEmpty()
     && (fileName.indexOf(".scorebin") != -1 || fileName.indexOf(".scorejson") != -1
         || fileName.indexOf(".score") != 1))
  {

    if(QFile f{fileName}; f.open(QIODevice::ReadOnly))
    {
      f.close();
      if(m_recentFiles)
      {
        m_recentFiles->addRecentFile(fileName);
        saveRecentFilesState();
      }

      if(fileName.indexOf(".scorebin") != -1)
      {
        doc = loadDocument(
            ctx, fileName, *ctx.interfaces<DocumentDelegateList>().begin());
      }
      else if(fileName.indexOf(".score") != -1)
      {
        doc = loadDocument(
            ctx, fileName, *ctx.interfaces<DocumentDelegateList>().begin());
      }
    }
  }

  return doc;
}

void DocumentManager::closeVirginDocument(const score::GUIApplicationContext& ctx)
{
  if(auto cur = currentDocument(); cur && cur->virgin())
  {
    forceCloseDocument(ctx, *cur);
  }
}

void DocumentManager::prepareNewDocument(const score::GUIApplicationContext& ctx)
{
  m_preparingNewDocument = true;
  for(GUIApplicationPlugin* appPlugin : ctx.guiApplicationPlugins())
  {
    appPlugin->prepareNewDocument();
  }
  m_preparingNewDocument = false;
}

bool DocumentManager::closeAllDocuments(const score::GUIApplicationContext& ctx)
{
  while(!m_documents.empty())
  {
    bool b = closeDocument(ctx, *m_documents.back());
    if(!b)
      return false;
  }

  return true;
}

bool DocumentManager::preparingNewDocument() const
{
  return m_preparingNewDocument;
}

bool DocumentManager::checkAndUpdateJson(
    rapidjson::Value& obj, const score::GUIApplicationContext& ctx)
{
  if(obj.GetType() != rapidjson::kObjectType)
    return false;

  // Check the version
  Version loaded_version{0};
  auto it = obj.FindMember("Version");
  if(it != obj.MemberEnd())
    loaded_version = Version{it->value.GetInt()};

  LocalPluginVersionsMap local_plugins;
  for(const auto& plug : ctx.addons())
  {
    local_plugins.emplace(plug.key, plug.plugin);
  }

  std::vector<LoadedPluginVersions> loading_plugins;
  auto plugin_it = obj.FindMember("Plugins");
  if(plugin_it != obj.MemberEnd())
  {
    if(plugin_it->value.IsArray())
    {
      for(const auto& plugin_val : plugin_it->value.GetArray())
      {
        const auto& plugin_obj = plugin_val.GetObject();
        auto plugin_key_it = plugin_obj.FindMember("Key");
        if(plugin_key_it == plugin_obj.MemberEnd())
          continue;
        QByteArray key_arr = QByteArray::fromRawData(
            plugin_key_it->value.GetString(), plugin_key_it->value.GetStringLength());
        auto plugin_key = UuidKey<score::Plugin>::fromString(key_arr);

        Version plugin_version{0};
        auto plugin_ver_it = plugin_obj.FindMember("Version");
        if(plugin_ver_it != plugin_obj.MemberEnd())
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
  if(loaded_version > ctx.applicationSettings.saveFormatVersion)
  {
    mainLoadable = false;
  }
  else if(loaded_version < ctx.applicationSettings.saveFormatVersion)
  {
    // TODO update main
    auto res
        = updateJson(obj, loaded_version, ctx.applicationSettings.saveFormatVersion);
    if(!res)
    {
      return false;
    }
  }

  // Check the plug-ins
  bool pluginsAvailable = true;
  bool pluginsLoadable = true;

  for(const auto& plug : loading_plugins)
  {
    auto it = local_plugins.find(plug.plugin);
    if(it == local_plugins.end())
    {
      pluginsAvailable = false;
    }
    else
    {
      auto& current_local_plugin = it->second;
      if(plug.version > current_local_plugin->version())
      {
        pluginsLoadable = false;
      }
      else if(plug.version < current_local_plugin->version())
      {
        current_local_plugin->updateSaveFile(
            obj, plug.version, current_local_plugin->version());
      }
    }
  }

  return mainLoadable && pluginsAvailable && pluginsLoadable;
}

bool DocumentManager::updateJson(
    rapidjson::Value& object, Version json_ver, Version score_ver)
{
  score::hash_map<Version, std::pair<Version, std::function<void(QJsonObject&)>>>
      conversions;
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
  if(m_recentFiles)
  {
    QSettings settings;
    settings.setValue("RecentFiles", m_recentFiles->saveState());
    m_recentFiles->saveState();
  }
#endif
}

void DocumentManager::restoreDocuments(const score::GUIApplicationContext& ctx)
{
  auto prev_docs = DocumentBackups::restorableDocuments();
  for(const RestorableDocument& backup : prev_docs)
  {
    restoreDocument(ctx, backup, *ctx.interfaces<DocumentDelegateList>().begin());
  }

  // All the documents have been reloaded successfully.

  // Remove the ancient files
  {
    for(auto& doc : prev_docs)
    {
      QFile{doc.docPath}.remove();
      QFile{doc.commandsPath}.remove();
    }
    QSettings s{score::OpenDocumentsFile::path(), QSettings::IniFormat};
    s.setValue("score/docs", QMap<QString, QVariant>{});
    s.sync();
  }

  // For all currently open documents, add the to the backed up list
  for(auto* doc : m_documents)
  {
    doc->backupManager()->updateBackupData();
  }
}

Id<score::DocumentModel> getStrongId(const std::vector<score::Document*>& v)
{
  using namespace std;
  vector<int32_t> ids(v.size()); // Map reduce

  transform(
      v.begin(), v.end(), ids.begin(), [](const auto elt) { return elt->id().val(); });

  return Id<score::DocumentModel>{score::random_id_generator::getNextId(ids)};
}

}
