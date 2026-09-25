#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Scenario/PasteAnchors.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Process/ProcessMimeSerialization.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace Scenario
{
rapidjson::Value* draggedCopy(rapidjson::Value& json)
{
  if(!json.IsObject())
    return nullptr;
  auto it = json.FindMember("Copy");
  if(it == json.MemberEnd() || !it->value.IsObject())
    return nullptr;
  auto& copy = it->value;
  if(!copy.HasMember("Processes") || !copy["Processes"].IsArray()
     || !copy.HasMember("Cables") || !copy["Cables"].IsArray())
    return nullptr;
  return &copy;
}

bool isProcessesDrag(const QMimeData& mime)
{
  if(!mime.hasFormat(score::mime::layerdata()))
    return false;
  auto json = readJson(mime.data(score::mime::layerdata()));
  return draggedCopy(json);
}

std::vector<const Process::ProcessModel*>
draggedProcesses(const rapidjson::Value& json, const score::DocumentContext& ctx)
{
  std::vector<const Process::ProcessModel*> res;
  auto copy = draggedCopy(const_cast<rapidjson::Value&>(json));
  if(!copy || !copiedFromHere(*copy, ctx))
    return res;
  auto paths = copy->FindMember("ProcessPaths");
  if(paths == copy->MemberEnd() || !paths->value.IsArray())
    return res;
  for(const auto& p : paths->value.GetArray())
  {
    ObjectPath path;
    path <<= JsonValue{p};
    if(auto proc = path.try_find<Process::ProcessModel>(ctx))
      res.push_back(proc);
  }
  return res;
}


void DropLayerInInterval::perform(
    const IntervalModel& interval, const score::DocumentContext& ctx,
    Scenario::Command::Macro& m, const rapidjson::Document& json)
{
  const auto pid = ossia::get_pid();
  bool same_doc = false;

  if(!json.HasMember("Path") || !json.HasMember("Cables"))
  {
    // TODO this is the "move the nodal slot" case
    return;
  }
  if(json.HasMember("PID") && json.HasMember("Document"))
  {
    same_doc = (pid == json["PID"].GetInt());
    // Document identifiers repeat across open documents
    if(json.HasMember("OriginDocument"))
      same_doc &= copiedFromHere(json, ctx);
    else
      same_doc &= (ctx.document.id().val() == json["Document"].GetInt());
  }

  if(same_doc)
  {
    auto old_p = JsonValue{json["Path"]}.to<Path<Process::ProcessModel>>();
    if(auto obj = old_p.try_find(ctx))
    {
      if(auto itv = qobject_cast<IntervalModel*>(obj->parent()))
      {
        const int slot_index
            = json.HasMember("SlotIndex") ? json["SlotIndex"].GetInt() : -1;
        if(slot_index == -1)
        {
          // Case of a nodal process, which wasn't in a slot.
          m.moveProcess(*itv, interval, obj->id());
        }
        else
        {
          // Process without a layer, just a preset
          // Move a slot from an interval to another
          const bool small_view = json.HasMember("View")
                                      ? JsonValue{json["View"]}.toString() == "Small"
                                      : true;

          if(small_view && (qApp->keyboardModifiers() & Qt::ALT))
          {
            m.moveSlot(*itv, interval, slot_index);
          }
          else
          {
            // Move a process from an interval to another
            m.moveProcess(*itv, interval, obj->id());
          }

          if(itv->processes.empty())
          {
            if(auto sm = dynamic_cast<Scenario::ProcessModel*>(itv->parent()))
            {
              auto& es = Scenario::endState(*itv, *sm);
              if(es.empty() && !es.nextInterval())
              {
                m.removeElements(*sm, Selection{itv, &es});
              }
            }
          }
        }
      }
    }
  }
  else
  {
    // Just create a new process
    if(auto proc = json.FindMember(score::StringConstant().Process);
       proc != json.MemberEnd())
    {
      if(proc->value.IsObject())
      {
        if(proc->value.HasMember(score::StringConstant().uuid))
        {
          // As a paste: anchors between objects of the process move to the
          // copy's, those to other objects are kept in the same document
          rapidjson::Document copy;
          copy.SetObject();
          auto& alloc = copy.GetAllocator();
          rapidjson::Value procs(rapidjson::kArrayType);
          procs.PushBack(rapidjson::Value(proc->value, alloc), alloc);
          copy.AddMember("Processes", procs, alloc);
          rapidjson::Value paths(rapidjson::kArrayType);
          paths.PushBack(rapidjson::Value(json["Path"], alloc), alloc);
          copy.AddMember("ProcessPaths", paths, alloc);
          if(same_doc)
          {
            const auto origin
                = score::IDocument::copyOrigin(ctx.document).toStdString();
            copy.AddMember(
                "OriginDocument", rapidjson::Value(origin.c_str(), alloc), alloc);
          }

          std::vector<std::pair<int32_t, int32_t>> ids;
          auto pasted = loadCopiedProcesses(m, interval, copy, ids, ctx);
          if(!pasted.empty())
            m.submit(new Command::FollowPasted{std::move(pasted)});
        }
      }
    }
    else
    {
      return;
    }
  }

  // Reload cables
  if(json.HasMember("Cables"))
  {
    auto new_path = score::IDocument::path(interval).unsafePath();

    // !!! FIXME this looks like it's not valid, use
    // serializedCablesFromCableJson instead, no ?
    auto cables = JsonValue{json["Cables"]}.to<Dataflow::SerializedCables>();

    auto& document
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

    for(auto& c : cables)
    {
      c.first = getStrongId(document.cables);
    }
    m.loadCables(new_path, cables);
  }

  // Finally we show the newly created rack
  m.showRack(interval);
}

bool DropLayerInInterval::drop(
    const score::DocumentContext& ctx, const IntervalModel& interval, QPointF p,
    const QMimeData& mime)
{
  if(mime.hasFormat(score::mime::layerdata()))
  {
    auto json = readJson(mime.data(score::mime::layerdata()));
    if(auto copy = draggedCopy(json))
    {
      CommandDispatcher<>{ctx.commandStack}.submit(
          new Command::PasteProcessesInInterval{
              *copy, interval, ExpandMode::GrowShrink, p});
      return true;
    }

    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};
    perform(interval, ctx, m, json);
    m.commit();
    return true;
  }
  else if(mime.hasUrls())
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};
    bool ok = false;
    for(const QUrl& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if(QFile f{path}; QFileInfo{f}.suffix() == "layer" && f.open(QIODevice::ReadOnly))
      {
        ok = true;
        perform(interval, ctx, m, readJson(f.readAll()));
      }
    }

    if(ok)
    {
      m.commit();
      return true;
    }
  }

  return false;
}

}
