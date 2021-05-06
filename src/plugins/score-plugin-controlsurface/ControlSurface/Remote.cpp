#include <ControlSurface/Remote.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/ValueSerialization.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

namespace ControlSurface
{
struct RemoteMessages
{
  Model& process;
  QString initMessage() const
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "ControlSurfaceAdded"sv;
    r.obj[score::StringConstant().Path] = Path{process};
    r.obj[score::StringConstant().Name] = process.metadata().getName();
    r.obj[score::StringConstant().Label] = process.metadata().getLabel();

    Process::Inlets controls;
    process.forEachControl(
        [&](auto& inl, auto& val) { controls.push_back(&inl); });
    r.obj["Controls"] = controls;
    r.stream.EndObject();
    return r.toString();
  }

  QString deinitMessage() const
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "ControlSurfaceRemoved"sv;
    r.obj[score::StringConstant().Path] = Path{process};
    r.stream.EndObject();
    return r.toString();
  }

  QString controlMessage(const Process::ControlInlet& inl) const
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "ControlSurfaceControl"sv;
    r.obj[score::StringConstant().Path] = Path{process};
    r.obj["Control"] = inl.id();
    r.obj[score::StringConstant().Value] = inl.value();
    r.stream.EndObject();
    return r.toString();
  }

  void controlSurface(
      const rapidjson::Value& obj,
      const score::DocumentContext& doc) const
  {
    auto it = obj.FindMember("Path");
    if (it == obj.MemberEnd())
      return;

    auto ctrl_it = obj.FindMember("id");
    if (ctrl_it == obj.MemberEnd())
      return;

    auto path = score::unmarshall<Path<Model>>(it->value);
    if (!path.valid())
      return;

    {
      Model* csp = path.try_find(doc);
      if (!csp)
        return;
      auto& cs = *csp;

      Id<Process::Inlet> id(ctrl_it->value.GetInt());
      auto it = ossia::find_if(
          cs.inlets(), [&](const auto& inlet) { return inlet->id() == id; });
      if (it == cs.inlets().end())
        return;

      auto value_it = obj.FindMember("Value");
      if (value_it == obj.MemberEnd())
        return;

      auto v = score::unmarshall<ossia::value>(value_it->value);
      if (Process::ControlInlet* inl
          = qobject_cast<Process::ControlInlet*>(*it))
      {
        inl->setValue(std::move(v));
      }
    }
  }
};

Remote::Remote(
    Model& proc,
    RemoteControl::DocumentPlugin& doc,
    QObject* parent_obj)
    : RemoteControl::ProcessComponent_T<Model>{
        proc,
        doc,
        "ControlSurfaceComponent",
        parent_obj}
{
  con(proc, &Model::startExecution, this, [this] {
    RemoteControl::Handler h;
    RemoteMessages msgs{process()};

    h.setupDefaultHandler(msgs);

    h.answers["ControlSurface"]
        = [this,
           msgs](const rapidjson::Value& v, const RemoteControl::WSClient&) {
            msgs.controlSurface(v, this->system().context());
          };

    process().forEachControl([&](const Process::ControlInlet& inl, auto& val) {
      con(inl, &Process::ControlInlet::valueChanged, this, [this, &inl] {
        RemoteMessages msgs{process()};
        system().receiver.sendMessage(msgs.controlMessage(inl));
      });
    });

    system().receiver.addHandler(this, std::move(h));
  });

  con(proc, &Model::stopExecution, this, [this] {
    system().receiver.removeHandler(this);

    process().forEachControl(
        [this](const Process::ControlInlet& inl, auto& val) {
          QObject::disconnect(
              &inl, &Process::ControlInlet::valueChanged, this, nullptr);
        });
  });
}

Remote::~Remote()
{
  system().receiver.removeHandler(this);
}
}
