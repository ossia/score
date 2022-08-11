// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationDropHandler.hpp"

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/thread.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QFileInfo>
#include <QUrl>
namespace Scenario
{

DropScenario::DropScenario()
{
  // TODO give them a mime type ?
  m_acceptableSuffixes.push_back("scenario");
}

bool DropScenario::drop(
    const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if(mime.hasUrls())
  {
    const auto& doc = pres.context().context;
    auto& sm = pres.model();
    auto path = mime.urls().first().toLocalFile();
    if(QFile f{path}; QFileInfo{f}.suffix() == "scenario" && f.open(QIODevice::ReadOnly))
    {
      CommandDispatcher<> d{doc.commandStack};
      d.submit(new Scenario::Command::ScenarioPasteElements(
          sm, readJson(f.readAll()), pres.toScenarioPoint(pos)));
      return true;
    }
  }

  return false;
}

DropScoreInScenario::DropScoreInScenario()
{
  m_acceptableSuffixes.push_back("score");
  m_acceptableSuffixes.push_back("scorebin");
}

bool DropScoreInScenario::drop(
    const ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if(mime.hasUrls())
  {
    const auto& doc = pres.context().context;
    auto& sm = pres.model();
    auto path = mime.urls().first().toLocalFile();
    if(QFile f{path}; QFileInfo{f}.suffix() == "score" && f.open(QIODevice::ReadOnly))
    {
      rapidjson::Document res;
      res.SetObject();

      auto obj = readJson(f.readAll());
      auto& docobj = obj["Document"];

      // ScenarioPasteElements expects the cable address to start from the topmost interval in the copied
      // content
      res.AddMember("Cables", docobj["Cables"], res.GetAllocator());
      for(auto& c : res["Cables"].GetArray())
      {
        auto source = c["Source"].GetArray();
        source.Erase(source.Begin());
        source.Erase(source.Begin());
        auto sink = c["Sink"].GetArray();
        sink.Erase(sink.Begin());
        sink.Erase(sink.Begin());
      }

      auto& scenar = docobj["BaseScenario"];

      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["Constraint"], obj.GetAllocator());
        res.AddMember("Intervals", arr, res.GetAllocator());
      }
      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["StartState"], obj.GetAllocator());
        arr.PushBack(scenar["EndState"], obj.GetAllocator());
        res.AddMember("States", arr, res.GetAllocator());
      }
      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["StartEvent"], obj.GetAllocator());
        arr.PushBack(scenar["EndEvent"], obj.GetAllocator());
        res.AddMember("Events", arr, res.GetAllocator());
      }
      {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.PushBack(scenar["StartTimeNode"], obj.GetAllocator());
        arr.PushBack(scenar["EndTimeNode"], obj.GetAllocator());
        res.AddMember("TimeNodes", arr, res.GetAllocator());
      }

      CommandDispatcher<> d{doc.commandStack};
      d.submit(new Scenario::Command::ScenarioPasteElements(
          sm, res, pres.toScenarioPoint(pos)));
      return true;
    }
  }

  return false;
}

static void getAddressesRecursively(
    const Device::Node& node, State::Address curAddr,
    std::vector<Device::FullAddressSettings>& addresses)
{
  // TODO refactor with CreateCurves and AddressAccessorEditWidget
  if(node.is<Device::AddressSettings>())
  {
    const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
    // FIXME see https://github.com/ossia/libossia/issues/291
    if(ossia::is_numeric(addr.value) || ossia::is_array(addr.value))
    {
      Device::FullAddressSettings as;
      static_cast<Device::AddressSettingsCommon&>(as) = addr;
      as.address = curAddr;
      addresses.push_back(std::move(as));
    }
    // TODO interpolation
  }

  for(auto& child : node)
  {
    const Device::AddressSettings& addr = child.get<Device::AddressSettings>();

    State::Address newAddr{curAddr};
    newAddr.path.append(addr.name);
    getAddressesRecursively(child, newAddr, addresses);
  }
}

bool AutomationDropHandler::drop(
    const score::DocumentContext& ctx, const IntervalModel& cst, QPointF p,
    const QMimeData& mime)
{
  // TODO refactor with AddressEditWidget
  if(mime.formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();
    if(nl.empty())
      return false;

    std::vector<Device::FullAddressSettings> addresses;
    for(auto& np : nl)
    {
      getAddressesRecursively(np.second, np.first, addresses);
    }

    if(addresses.empty())
      return false;

    CreateCurvesFromAddresses({&cst}, addresses, ctx.commandStack);

    return true;
  }
  else
  {
    return false;
  }
}

bool DropScoreInInterval::drop(
    const score::DocumentContext& doc, const IntervalModel& interval, QPointF p,
    const QMimeData& mime)
{
  if(mime.hasUrls())
  {
    auto path = mime.urls().first().toLocalFile();
    if(QFile f{path}; QFileInfo{f}.suffix() == "score" && f.open(QIODevice::ReadOnly))
    {
      auto obj = readJson(f.readAll());
      auto& docobj = obj["Document"];
      auto& scenar = docobj["BaseScenario"];
      auto& itv = scenar["Constraint"];

      Scenario::Command::Macro m{new Command::DropProcessInIntervalMacro, doc};
      for(auto& json : itv["Processes"].GetArray())
      {
        rapidjson::Value v{rapidjson::kObjectType};
        v.AddMember("Process", json, obj.GetAllocator());

        m.loadProcessInSlot(interval, v);
      }

      // Reload cables
      {
        ObjectPath old_path{
            {"Scenario::ScenarioDocumentModel", 1},
            {"Scenario::BaseScenario", 0},
            {"Scenario::IntervalModel", 0}};
        auto new_path = score::IDocument::path(interval).unsafePath();
        auto cables = Dataflow::serializedCablesFromCableJson(
            old_path, docobj["Cables"].GetArray());

        auto& document
            = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc.document);
        for(auto& c : cables)
        {
          c.first = getStrongId(document.cables);
        }
        m.loadCables(new_path, cables);
      }

      // Finally we show the newly created rack
      m.showRack(interval);

      m.commit();

      return true;
    }
  }

  return false;
}

}
