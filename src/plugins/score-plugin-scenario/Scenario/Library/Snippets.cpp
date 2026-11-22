#include "Snippets.hpp"
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

#include <Process/Dataflow/CableCopy.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/widgets/MessageBox.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
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
#include <Library/PresetFile.hpp>
#include <Library/Snippet.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>

#include <QDir>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QAction>
#include <QDialog>
#include <QMenu>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGraphicsScene>
#include <QGraphicsView>
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
    const auto answer = score::question(
        parent, QObject::tr("Save as snippet"),
        QObject::tr("A snippet named %1 exists. Replace it?").arg(rel));
    if(answer != QMessageBox::Yes)
      return std::nullopt;
  }

  // A snippet replaced is kept as it was if the new one cannot be written
  if(!Library::writePresetData(file, json))
  {
    score::warning(
        parent, QObject::tr("Save as snippet"),
        QObject::tr("The snippet could not be written in %1").arg(file));
    return std::nullopt;
  }

  // The library only scans its folders on start and on a rescan: a new
  // snippet is added to it here; a replaced one is listed, with what it says
  // of itself now.
  if(auto panel = score::GUIAppContext().findPanel<Library::ProcessPanel>())
  {
    auto& model = panel->processWidget().processModel();
    const QString path = QFileInfo{file}.absoluteFilePath();
    if(replacing)
    {
      model.setSearchText(
          Library::snippetKey(), path,
          Library::snippetSearchText(Library::readSnippetInfo(json, false)));
    }
    else
    {
      const QByteArray utf8 = path.toUtf8();
      if(auto entry = snippetEntry(std::string_view{utf8.data(), size_t(utf8.size())}))
        model.publish(std::move(*entry));
    }
  }
  return file;
}

namespace
{
//! Between a node and the next one put after it
constexpr double nodeSpacing = 50.;

//! `rect` of `scene` as it is drawn, small enough for the library.
QImage renderRect(QGraphicsScene& scene, QRectF rect)
{
  if(rect.width() < 1. || rect.height() < 1.)
    return {};

  constexpr QSizeF thumbnailSize{320., 180.};
  const QSizeF size = rect.size().scaled(thumbnailSize, Qt::KeepAspectRatio);
  if(size.toSize().isEmpty())
    return {};
  // As sharp as the screen it is shown on
  const qreal dpr = qApp->devicePixelRatio();
  QImage img{(size * dpr).toSize(), QImage::Format_ARGB32_Premultiplied};
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);
  QPainter p{&img};
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  scene.render(&p, QRectF{QPointF{}, size}, rect);
  return img;
}

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
  return renderRect(*pres->view().scene(), rect);
}

//! The nodes selected in the node view as they are drawn, small.
QImage renderSelectedNodes(const score::DocumentContext& ctx)
{
  auto pres = score::IDocument::presenterDelegate<ScenarioDocumentPresenter>(ctx.document);
  if(!pres)
    return {};
  auto* scene = pres->view().view().scene();
  if(!scene)
    return {};
  QRectF rect;
  for(QGraphicsItem* item : scene->items())
    if(auto node = dynamic_cast<Process::NodeItem*>(item))
      if(node->model().selection.get())
        rect |= node->sceneBoundingRect();
  return renderRect(*scene, rect);
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
  // What Copy puts in the clipboard: in the timeline the selected boxes,
  // states and the cables between their processes; in the node view the
  // selected processes and the cables between them.
  JSONReader r;
  if(auto si = focusedScenarioInterface(ctx))
    copySelectedElementsToJson(r, *const_cast<ScenarioInterface*>(si), ctx);
  else if(!copySelectedProcesses(r, ctx))
    return std::nullopt;
  if(r.empty())
    return std::nullopt;
  auto doc = readJson(r.toByteArray());
  const auto nonEmpty = [&](const char* key) {
    const auto it = doc.FindMember(key);
    return it != doc.MemberEnd() && it->value.IsArray() && !it->value.Empty();
  };
  if(!doc.IsObject()
     || (!nonEmpty("Intervals") && !nonEmpty("States") && !nonEmpty("Processes")))
    return std::nullopt;
  // Which document it was copied from only means something in this session:
  // a snippet is the same wherever and whenever it is inserted.
  doc.RemoveMember("OriginDocument");

  const bool fromTimeline = doc.HasMember("TimeNodes");
  const auto form = askSnippet(
      parent, fromTimeline ? renderSelection(ctx) : renderSelectedNodes(ctx));
  if(!form)
    return std::nullopt;

  Library::addSnippetInfo(doc, form->info);
  const auto& lib = ctx.app.settings<Library::Settings::Model>();
  return writeSnippet(jsonToByteArray(doc), lib.getUserLibraryPath(), form->name, parent);
}

bool insertSnippet(const ScenarioPresenter& pres, QPointF pos, const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  auto json = readJson(f.readAll());
  if(!json.IsObject())
    return false;

  // Processes saved from the node view go in a new box there.
  if(!json.HasMember("TimeNodes"))
  {
    if(!json.HasMember("Processes") || !json.HasMember("Cables"))
      return false;
    return pasteProcessesInNewBox(
        pres.model(), pres.toScenarioPoint(pos), json, pres.context().context);
  }

  // What nothing leads into starts when the score plays, as a state made by
  // double-clicking does
  const auto& scenario = pres.model();
  Command::Macro m{new Command::InsertSnippetMacro, pres.context().context};
  auto paste
      = new Command::ScenarioPasteElements(scenario, json, pres.toScenarioPoint(pos));
  m.submit(paste);
  const auto ledInto = [&](const TimeSyncModel& ts) {
    return ossia::any_of(ts.events(), [&](const auto& ev) {
      return ossia::any_of(scenario.event(ev).states(), [&](const auto& st) {
        return bool(scenario.state(st).previousInterval());
      });
    });
  };
  for(const auto& id : paste->pastedTimeSyncs())
    if(auto ts = scenario.timeSyncs.find(id); ts != scenario.timeSyncs.end())
      if(!ledInto(*ts))
        addStartOnPlayTrigger(m, *ts);
  m.commit();
  return true;
}

namespace
{
//! Loads the processes of all the boxes of the snippet at `path` in
//! `interval`, and the cables between them; the paths of the new processes.
std::vector<ObjectPath> loadSnippet(
    Command::Macro& m, const score::DocumentContext& ctx, const IntervalModel& interval,
    QPointF nodePos, const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  auto json = readJson(f.readAll());
  if(!json.IsObject())
    return {};

  // A snippet saved from the timeline holds boxes; one saved from the node
  // view holds processes, as copied there, with no box around them.
  const bool boxes = json.HasMember("Intervals");
  struct Group
  {
    int32_t box{-1};
    rapidjson::Value* processes{};
  };
  std::vector<Group> groups;
  if(boxes)
  {
    if(!json["Intervals"].IsArray())
      return {};
    for(auto& itv : json["Intervals"].GetArray())
    {
      if(!itv.IsObject())
        continue;
      auto procs = itv.FindMember("Processes");
      auto id = itv.FindMember("id");
      if(procs != itv.MemberEnd() && procs->value.IsArray() && id != itv.MemberEnd()
         && id->value.IsInt())
        groups.push_back({id->value.GetInt(), &procs->value});
    }
  }
  else if(auto procs = json.FindMember("Processes");
          procs != json.MemberEnd() && procs->value.IsArray())
  {
    groups.push_back({-1, &procs->value});
  }
  if(groups.empty())
    return {};

  // The nodes of each box keep their layout, the boxes' side by side from
  // nodePos: in the snippet they were in separate node views.
  const auto pair = [](rapidjson::Value& proc, const char* key) -> rapidjson::Value* {
    if(!proc.IsObject())
      return nullptr;
    auto it = proc.FindMember(key);
    if(it == proc.MemberEnd() || !it->value.IsArray() || it->value.Size() != 2
       || !it->value[0].IsNumber() || !it->value[1].IsNumber())
      return nullptr;
    return &it->value;
  };
  constexpr double groupSpacing = 40.;
  double x = nodePos.x();
  for(auto& group : groups)
  {
    QRectF bounds;
    for(auto& proc : group.processes->GetArray())
      if(auto pos = pair(proc, "Pos"))
      {
        QSizeF size{200., 200.};
        if(auto sz = pair(proc, "Size"))
          size = {(*sz)[0].GetDouble(), (*sz)[1].GetDouble()};
        const QRectF r{QPointF{(*pos)[0].GetDouble(), (*pos)[1].GetDouble()}, size};
        bounds = bounds.isNull() ? r : bounds.united(r);
      }
    if(bounds.isNull())
      continue;
    for(auto& proc : group.processes->GetArray())
      if(auto pos = pair(proc, "Pos"))
      {
        (*pos)[0].SetDouble((*pos)[0].GetDouble() - bounds.left() + x);
        (*pos)[1].SetDouble((*pos)[1].GetDouble() - bounds.top() + nodePos.y());
      }
    x += bounds.width() + groupSpacing;
  }

  // (box, process) in the snippet -> process in `interval`; the box is -1 for
  // processes saved without one.
  std::map<std::pair<int32_t, int32_t>, int32_t> ids;
  std::vector<ObjectPath> pasted;
  if(boxes)
  {
    // Where the boxes were: the anchors between their processes are found from it.
    ObjectPath origin;
    if(auto it = json.FindMember("Origin"); it != json.MemberEnd())
      origin <<= JsonValue{it->value};

    for(const auto& group : groups)
    {
      ObjectPath from = origin;
      from.vec().push_back(ObjectIdentifier{"Scenario::IntervalModel", group.box});
      std::vector<std::pair<int32_t, int32_t>> map;
      auto paths = loadIntervalProcesses(m, interval, *group.processes, from, map, ctx);
      pasted.insert(pasted.end(), paths.begin(), paths.end());
      for(auto [old_id, new_id] : map)
        ids[{group.box, old_id}] = new_id;
    }
  }
  else
  {
    std::vector<std::pair<int32_t, int32_t>> map;
    pasted = loadCopiedProcesses(m, interval, json, map, ctx);
    for(auto [old_id, new_id] : map)
      ids[{-1, old_id}] = new_id;
  }
  if(pasted.empty())
    return {};

  // The cables between them: their ends start at the box in the snippet, and
  // in `interval` at the copy of the process.
  if(auto cables_it = json.FindMember("Cables");
     cables_it != json.MemberEnd() && cables_it->value.IsArray())
  {
    const auto relocate = [&](ObjectPath& p) {
      auto& vec = p.vec();
      int32_t box = -1;
      if(boxes)
      {
        // Through a process of the box, not to a port of the box itself
        if(vec.size() < 2 || vec[0].objectName() != "Scenario::IntervalModel"
           || vec[1].objectName() == "Inlet" || vec[1].objectName() == "Outlet")
          return false;
        box = vec[0].id();
        vec.erase(vec.begin());
      }
      if(vec.empty())
        return false;
      auto it = ids.find({box, vec[0].id()});
      if(it == ids.end())
        return false;
      vec.front() = ObjectIdentifier{vec.front().objectName(), it->second};
      return true;
    };
    auto& document = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);
    auto cables = Dataflow::keepCopiedCables(
        cableDataFromCablesJson(cables_it->value.GetArray()), relocate, document.cables);
    if(!cables.empty())
      m.loadCables(score::IDocument::path(interval).unsafePath(), cables);
  }

  return pasted;
}
}

bool insertSnippetInInterval(
    const score::DocumentContext& ctx, const IntervalModel& interval, QPointF nodePos,
    const QString& path)
{
  Command::Macro m{new Command::DropProcessInIntervalMacro, ctx};
  auto pasted = loadSnippet(m, ctx, interval, nodePos, path);
  if(pasted.empty())
    return false;
  m.submit(new Command::FollowPasted{std::move(pasted)});
  m.showRack(interval);
  m.commit();
  return true;
}

bool insertSnippetAfter(
    const score::DocumentContext& ctx, const Scenario::ProcessModel& scenario,
    const StateModel& state, double y, const QString& path)
{
  // As long as a box made by Tab
  constexpr auto duration = TimeVal::fromMsecs(10000.);

  Command::Macro m{new Command::InsertSnippetMacro, ctx};
  // A state already followed by a box: another state on its event starts it
  const StateModel* from = &state;
  if(state.nextInterval())
    from = &m.createState(scenario, state.eventId(), y);
  const auto date = scenario.event(from->eventId()).date();
  auto& box = m.createIntervalAfter(
      scenario, from->id(), Scenario::Point{date + duration, from->heightPercentage()});

  auto pasted = loadSnippet(m, ctx, box, QPointF{}, path);
  if(pasted.empty())
    return false;
  m.submit(new Command::FollowPasted{std::move(pasted)});
  m.showRack(box);
  m.submit(new Command::ChangeElementName{box, QFileInfo{path}.completeBaseName()});
  m.commit();
  return true;
}

bool insertSnippetInCable(
    const score::DocumentContext& ctx, const Process::Cable& cable, const QString& path)
{
  auto* source = cable.source().try_find(ctx);
  auto* sink = cable.sink().try_find(ctx);
  if(!source || !sink)
    return false;
  auto* interval = closestParentInterval(source);
  if(!interval)
    return false;

  // Between the two nodes the cable joins.
  QPointF nodePos{};
  {
    auto* from = Process::parentProcess(source);
    auto* to = Process::parentProcess(sink);
    if(from && to)
      nodePos = (from->position() + to->position()) / 2.;
    else if(from)
      nodePos = from->position() + QPointF{from->size().width() + nodeSpacing, 0.};
  }

  Command::Macro m{new Command::DropProcessInIntervalMacro, ctx};
  auto pasted = loadSnippet(m, ctx, *interval, nodePos, path);
  if(pasted.empty())
    return false;

  // The snippet as a chain: the cable's source feeds the left-most inlet of
  // its type that nothing feeds, and the right-most outlet of its type that
  // goes nowhere feeds the cable's sink.
  std::vector<const Process::ProcessModel*> procs;
  for(const auto& p : pasted)
    if(auto proc = p.try_find<Process::ProcessModel>(ctx))
      procs.push_back(proc);
  std::sort(procs.begin(), procs.end(), [](auto a, auto b) {
    return a->position().x() < b->position().x();
  });

  const Process::Inlet* inlet{};
  for(auto it = procs.begin(); it != procs.end() && !inlet; ++it)
    for(auto* in : (*it)->inlets())
      if(in->type() == source->type() && in->cables().empty())
      {
        inlet = in;
        break;
      }

  const Process::Outlet* outlet{};
  for(auto it = procs.rbegin(); it != procs.rend() && !outlet; ++it)
    for(auto* out : (*it)->outlets())
      if(out->type() == sink->type() && out->cables().empty())
      {
        outlet = out;
        break;
      }

  auto& document = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);
  const auto type = cable.type();
  if(inlet && outlet)
  {
    m.removeCable(document, cable);
    m.createCable(document, *source, *inlet, type);
    m.createCable(document, *outlet, *sink, type);
  }
  else if(inlet)
  {
    m.createCable(document, *source, *inlet, type);
  }
  else if(outlet)
  {
    m.createCable(document, *outlet, *sink, type);
  }

  m.submit(new Command::FollowPasted{std::move(pasted)});
  m.commit();
  return true;
}

void fillSnippetsMenu(QMenu& menu, std::function<void(const QString&)> insert)
{
  // The branch the snippets are listed under
  const Library::ProcessNode* root{};
  if(auto panel = score::GUIAppContext().findPanel<Library::ProcessPanel>())
  {
    auto& model = panel->processWidget().processModel();
    if(const auto idx = model.find(Library::snippetKey()); idx.isValid())
      root = &model.nodeFromModelIndex(idx);
  }

  if(!root || !root->hasChildren())
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
