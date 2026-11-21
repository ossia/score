#include "Snippets.hpp"

#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <Scenario/Library/SlotLibraryHandler.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/Snippet.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QObject>

namespace Scenario
{
std::optional<QString> writeSnippet(
    const QByteArray& json, const QString& libraryPath, const QString& name,
    QWidget* parent)
{
  // "Folder/Name": the folders are categories of the library.
  QString rel = QDir::cleanPath(name.trimmed());
  while(rel.startsWith('/') || rel.startsWith("../"))
    rel = rel.mid(rel.indexOf('/') + 1);
  if(rel.isEmpty() || rel == "." || rel == "..")
    return std::nullopt;

  const QString root = libraryPath + "/" + Library::snippetsFolder();
  const QString file = root + "/" + rel + "." + Library::snippetSuffix();
  if(!QDir{}.mkpath(QFileInfo{file}.absolutePath()))
    return std::nullopt;

  const bool replacing = QFile::exists(file);
  if(replacing)
  {
    const auto answer = QMessageBox::question(
        parent, QObject::tr("Save as snippet"),
        QObject::tr("A snippet named %1 exists. Replace it?").arg(rel),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if(answer != QMessageBox::Yes)
      return std::nullopt;
  }

  QFile f{file};
  if(!f.open(QIODevice::WriteOnly))
    return std::nullopt;
  f.write(json);
  f.close();

  // The library only scans its folders on start and on a rescan: a new
  // snippet is added to it here. A replaced one is already listed.
  if(!replacing)
  {
    if(auto panel = score::GUIAppContext().findPanel<Library::ProcessPanel>())
    {
      const QByteArray utf8 = QFileInfo{file}.absoluteFilePath().toUtf8();
      if(auto entry = snippetEntry(std::string_view{utf8.data(), size_t(utf8.size())}))
        panel->processWidget().processModel().publish(std::move(*entry));
    }
  }
  return file;
}

std::optional<QString>
saveSelectionAsSnippet(const score::DocumentContext& ctx, QWidget* parent)
{
  auto si = focusedScenarioInterface(ctx);
  if(!si)
    return std::nullopt;

  // What Copy puts in the clipboard: the selected boxes, states and the
  // cables between their processes.
  JSONReader r;
  copySelectedElementsToJson(r, *const_cast<ScenarioInterface*>(si), ctx);
  if(r.empty())
    return std::nullopt;
  const QByteArray json = r.toByteArray();
  {
    const auto doc = readJson(json);
    const auto intervals = doc.FindMember("Intervals");
    const auto states = doc.FindMember("States");
    const bool has_content
        = (intervals != doc.MemberEnd() && intervals->value.IsArray()
           && !intervals->value.Empty())
          || (states != doc.MemberEnd() && states->value.IsArray()
              && !states->value.Empty());
    if(!has_content)
      return std::nullopt;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      parent, QObject::tr("Save as snippet"),
      QObject::tr("Name (Folder/Name puts it in a folder):"), QLineEdit::Normal,
      QObject::tr("Snippet"), &ok);
  if(!ok)
    return std::nullopt;

  const auto& lib = ctx.app.settings<Library::Settings::Model>();
  return writeSnippet(json, lib.getUserLibraryPath(), name, parent);
}

bool insertSnippet(const ScenarioPresenter& pres, QPointF pos, const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  auto json = readJson(f.readAll());
  if(!json.IsObject())
    return false;

  CommandDispatcher<> d{pres.context().context.commandStack};
  d.submit(
      new Command::ScenarioPasteElements(pres.model(), json, pres.toScenarioPoint(pos)));
  return true;
}
}
