#include "Snippets.hpp"

#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/PasteAnchors.hpp>
#include <Scenario/Commands/Scenario/ScenarioPaste.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>

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
#include <QAction>
#include <QDialog>
#include <QMenu>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QObject>

#include <limits>
#include <map>

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

namespace
{
//! The boxes selected in the focused timeline as they are drawn, small.
QImage renderSelection(const score::DocumentContext& ctx)
{
  auto focus = Process::ProcessFocusManager::get(ctx);
  if(!focus)
    return {};
  auto pres = qobject_cast<const ScenarioPresenter*>(focus->focusedPresenter());
  if(!pres || !pres->view().scene())
    return {};

  QRectF rect;
  for(const TemporalIntervalPresenter& itv : pres->getIntervals())
  {
    if(!itv.model().selection.get())
      continue;
    const auto* v = itv.view();
    rect |= v->mapRectToScene(v->boundingRect() | v->childrenBoundingRect());
  }
  if(rect.width() < 1. || rect.height() < 1.)
    return {};

  const QSize size = rect.size().scaled(QSizeF{320., 180.}, Qt::KeepAspectRatio).toSize();
  if(size.isEmpty())
    return {};
  QImage img{size, QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);
  QPainter p{&img};
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  pres->view().scene()->render(&p, QRectF{QPointF{}, QSizeF{size}}, rect);
  return img;
}

struct SnippetForm
{
  QString name;
  Library::SnippetInfo info;
};

std::optional<SnippetForm> askSnippet(QWidget* parent, const QImage& thumbnail)
{
  QDialog dlg{parent};
  dlg.setWindowTitle(QObject::tr("Save as snippet"));
  auto lay = new QFormLayout{&dlg};

  auto name = new QLineEdit{QObject::tr("Snippet"), &dlg};
  name->setToolTip(QObject::tr("Folder/Name puts it in a folder of the library"));
  lay->addRow(QObject::tr("Name"), name);

  auto description = new QPlainTextEdit{&dlg};
  description->setMaximumHeight(80);
  lay->addRow(QObject::tr("Description"), description);

  auto tags = new QLineEdit{&dlg};
  tags->setPlaceholderText(QObject::tr("Separated by commas"));
  lay->addRow(QObject::tr("Tags"), tags);

  if(!thumbnail.isNull())
  {
    auto preview = new QLabel{&dlg};
    preview->setPixmap(QPixmap::fromImage(thumbnail));
    lay->addRow(preview);
  }

  auto buttons = new QDialogButtonBox{QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg};
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  lay->addRow(buttons);

  name->selectAll();
  name->setFocus();
  if(dlg.exec() != QDialog::Accepted)
    return std::nullopt;

  SnippetForm res;
  res.name = name->text();
  res.info.description = description->toPlainText().trimmed();
  for(const QString& t : tags->text().split(',', Qt::SkipEmptyParts))
    if(auto tag = t.trimmed(); !tag.isEmpty())
      res.info.tags.push_back(tag);
  res.info.thumbnail = thumbnail;
  return res;
}
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

  const auto form = askSnippet(parent, renderSelection(ctx));
  if(!form)
    return std::nullopt;

  const auto& lib = ctx.app.settings<Library::Settings::Model>();
  return writeSnippet(
      Library::withSnippetInfo(json, form->info), lib.getUserLibraryPath(), form->name,
      parent);
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

bool insertSnippetInInterval(
    const score::DocumentContext& ctx, const IntervalModel& interval, QPointF nodePos,
    const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  auto json = readJson(f.readAll());
  if(!json.IsObject())
    return false;
  const auto intervals_it = json.FindMember("Intervals");
  if(intervals_it == json.MemberEnd() || !intervals_it->value.IsArray()
     || intervals_it->value.Empty())
    return false;
  auto& alloc = json.GetAllocator();
  auto intervals = intervals_it->value.GetArray();

  // Where the boxes were: the anchors between their processes are found from it.
  ObjectPath origin;
  if(auto it = json.FindMember("Origin"); it != json.MemberEnd())
    origin <<= JsonValue{it->value};

  // The nodes of all the boxes move together, their top-left one to nodePos.
  {
    double x0 = std::numeric_limits<double>::max();
    double y0 = std::numeric_limits<double>::max();
    for(auto& itv : intervals)
      if(itv.HasMember("Processes") && itv["Processes"].IsArray())
        for(auto& proc : itv["Processes"].GetArray())
          if(proc.HasMember("Pos") && proc["Pos"].IsArray() && proc["Pos"].Size() == 2)
          {
            x0 = std::min(x0, proc["Pos"][0].GetDouble());
            y0 = std::min(y0, proc["Pos"][1].GetDouble());
          }
    if(x0 != std::numeric_limits<double>::max())
      for(auto& itv : intervals)
        if(itv.HasMember("Processes") && itv["Processes"].IsArray())
          for(auto& proc : itv["Processes"].GetArray())
            if(proc.HasMember("Pos") && proc["Pos"].IsArray() && proc["Pos"].Size() == 2)
            {
              proc["Pos"][0].SetDouble(proc["Pos"][0].GetDouble() - x0 + nodePos.x());
              proc["Pos"][1].SetDouble(proc["Pos"][1].GetDouble() - y0 + nodePos.y());
            }
  }

  Command::Macro m{new Command::DropProcessInIntervalMacro, ctx};

  // (box, process) in the snippet -> process in `interval`
  std::map<std::pair<int32_t, int32_t>, int32_t> ids;
  std::vector<ObjectPath> pasted;
  for(auto& itv : intervals)
  {
    if(!itv.HasMember("Processes") || !itv["Processes"].IsArray() || !itv.HasMember("id"))
      continue;
    const int32_t itv_id = itv["id"].GetInt();

    rapidjson::Document copy;
    copy.SetObject();
    auto& calloc = copy.GetAllocator();
    copy.AddMember("Processes", rapidjson::Value(itv["Processes"], calloc), calloc);
    {
      ObjectPath parent = origin;
      parent.vec().push_back(ObjectIdentifier{"Scenario::IntervalModel", itv_id});
      JSONReader r;
      r.readFrom(parent);
      const auto parent_json = readJson(r.toByteArray());
      copy.AddMember("ProcessesParent", rapidjson::Value(parent_json, calloc), calloc);
    }

    std::vector<std::pair<int32_t, int32_t>> map;
    auto paths = loadCopiedProcesses(m, interval, copy, map, ctx);
    pasted.insert(pasted.end(), paths.begin(), paths.end());
    for(auto [old_id, new_id] : map)
      ids[{itv_id, old_id}] = new_id;
  }
  if(pasted.empty())
    return false;

  // The cables between them: their ends start at the box in the snippet, and
  // in `interval` at the copy of the process.
  if(auto cables_it = json.FindMember("Cables");
     cables_it != json.MemberEnd() && cables_it->value.IsArray())
  {
    const auto copied = cableDataFromCablesJson(cables_it->value.GetArray());
    auto& document = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);
    const auto relocate = [&](ObjectPath& p) {
      auto& vec = p.vec();
      if(vec.size() < 2 || vec[0].objectName() != "Scenario::IntervalModel")
        return false;
      auto it = ids.find({vec[0].id(), vec[1].id()});
      if(it == ids.end())
        return false;
      vec.erase(vec.begin());
      vec.front() = ObjectIdentifier{vec.front().objectName(), it->second};
      return true;
    };

    Dataflow::SerializedCables cables;
    for(Process::CableData cd : copied)
    {
      if(relocate(cd.source.unsafePath()) && relocate(cd.sink.unsafePath()))
      {
        Id<Process::Cable> id = getStrongId(document.cables);
        while(ossia::any_of(cables, [&](const auto& c) { return c.first == id; }))
          id = getStrongId(document.cables);
        cables.emplace_back(id, std::move(cd));
      }
    }
    if(!cables.empty())
      m.loadCables(score::IDocument::path(interval).unsafePath(), cables);
  }

  m.submit(new Command::FollowPasted{std::move(pasted)});
  m.showRack(interval);
  m.commit();
  return true;
}

void fillSnippetsMenu(QMenu& menu, std::function<void(const QString&)> insert)
{
  auto panel = score::GUIAppContext().findPanel<Library::ProcessPanel>();
  const Library::ProcessNode* root{};
  if(panel)
  {
    // The branch the snippets are listed under: the one holding them.
    for(const auto& top : panel->processWidget().processModel().rootNode())
    {
      bool found = false;
      top.visit([&](const Library::ProcessNode& n) {
        found = found || n.key == Library::snippetKey();
      });
      if(found)
      {
        root = &top;
        break;
      }
    }
  }

  if(!root)
  {
    auto none = menu.addAction(QObject::tr("No snippets in the library"));
    none->setEnabled(false);
    return;
  }

  const auto shared = std::make_shared<std::function<void(const QString&)>>(std::move(insert));
  const std::function<void(QMenu&, const Library::ProcessNode&)> fill
      = [&](QMenu& m, const Library::ProcessNode& node) {
    for(const auto& child : node)
    {
      if(child.key == Library::snippetKey())
      {
        auto act = m.addAction(child.prettyName);
        const QString path = child.customData;
        QObject::connect(act, &QAction::triggered, &m, [shared, path] { (*shared)(path); });
      }
      else if(child.childCount() > 0)
      {
        fill(*m.addMenu(child.prettyName), child);
      }
    }
  };
  fill(menu, *root);
}
}
