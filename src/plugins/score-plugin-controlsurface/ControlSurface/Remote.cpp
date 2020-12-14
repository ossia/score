#include <ControlSurface/Remote.hpp>
#include <Process/Dataflow/Port.hpp>
#include <score/tools/Bind.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <State/ValueSerialization.hpp>

namespace ControlSurface
{
struct RemoteMessages
{
  static QString initMessage(Model& process)
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "ControlSurfaceAdded"sv;
    r.obj[score::StringConstant().Path] = Path{process};

    Process::Inlets controls;
    process.forEachControl([&] (auto& inl, auto& val) {
      controls.push_back(&inl);
    });
    r.obj["Controls"] = controls;
    r.stream.EndObject();
    return r.toString();
  }

  static QString deinitMessage(Model& process)
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "ControlSurfaceRemoved"sv;
    r.obj[score::StringConstant().Path] = Path{process};
    r.stream.EndObject();
    return r.toString();
  }

  static void controlSurface(
      const rapidjson::Value& obj,
      Model& process,
      const score::DocumentContext& doc)
  {
    auto it = obj.FindMember("Path");
    if (it == obj.MemberEnd())
      return;
    auto ctrl_it = obj.FindMember("id");
    if(ctrl_it == obj.MemberEnd())
      return;

    auto path = score::unmarshall<Path<Model>>(it->value);
    if (!path.valid())
      return;

    {
      Model& cs = path.find(doc);
      Id<Process::Inlet> id(ctrl_it->value.GetInt());
      auto it = ossia::find_if(cs.inlets(), [&](const auto& inlet) {
        return inlet->id() == id;
      });
      if(it == cs.inlets().end())
        return;

      auto value_it = obj.FindMember("Value");
      if(value_it == obj.MemberEnd())
        return;

      auto v = score::unmarshall<ossia::value>(value_it->value);
      if(Process::ControlInlet* inl = qobject_cast<Process::ControlInlet*>(*it)) {
        inl->setValue(std::move(v));
      }
    }
  }
};

Remote::Remote(
    Model& proc,
    RemoteControl::DocumentPlugin& doc,
    const Id<score::Component>& id,
    QObject* parent_obj)
    : RemoteControl::ProcessComponent_T<Model>{
        proc, doc, id, "ControlSurfaceComponent", parent_obj}
{
  con(proc, &Model::startExecution,
      this, [this] {
    RemoteControl::Handler h;

    h.onAdded = [this] (const std::vector<RemoteControl::WSClient>& clts) {
      auto msg = RemoteMessages::initMessage(this->process());
      for(auto& clt : clts)
        clt.socket->sendTextMessage(msg);
    };
    h.onClientConnection = [this] (const RemoteControl::WSClient& clt) {
      auto msg = RemoteMessages::initMessage(this->process());
      clt.socket->sendTextMessage(msg);
    };

    h.onRemoved = [this] (const std::vector<RemoteControl::WSClient>& clts) {
      auto msg = RemoteMessages::deinitMessage(this->process());
      for(auto& clt : clts)
        clt.socket->sendTextMessage(msg);
    };
    h.onClientDisconnection = [this] (const RemoteControl::WSClient& clt) {
      auto msg = RemoteMessages::deinitMessage(this->process());
      clt.socket->sendTextMessage(msg);
    };

    h.answers["ControlSurface"] =
        [this](const rapidjson::Value& v, const RemoteControl::WSClient&) {
      RemoteMessages::controlSurface(v, this->process(), this->system().context());
    };

    system().receiver.addHandler(this, std::move(h));
  });

  con(proc, &Model::stopExecution,
      this, [this] {
    system().receiver.removeHandler(this);
  });
}

}
