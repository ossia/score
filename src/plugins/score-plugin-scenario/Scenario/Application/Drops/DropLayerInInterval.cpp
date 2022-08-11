#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace Scenario
{

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
    if(json.HasMember(score::StringConstant().Process)
       && json.HasMember(score::StringConstant().uuid))
    {
      m.loadProcessInSlot(interval, json);
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
  if(mime.formats().contains(score::mime::layerdata()))
  {
    Scenario::Command::Macro m{new Scenario::Command::DropProcessInIntervalMacro, ctx};

    const auto json = readJson(mime.data(score::mime::layerdata()));
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
