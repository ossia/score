#include "Interval.hpp"
#include <RemoteControl/Scenario/Process.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Bind.hpp>

#include <Process/Dataflow/Port.hpp>
#include <State/ValueSerialization.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>
#include <Process/Process.hpp>

namespace RemoteControl
{

class DefaultProcessComponent final
    : public ProcessComponent
{
  COMPONENT_METADATA("9bd540a2-79e1-4bb8-b9f6-7e775b4616dd")
public:
  DefaultProcessComponent(
      Process::ProcessModel& proc,
      DocumentPlugin& doc,
      QObject* parent);

  virtual ~DefaultProcessComponent();
};

struct RemoteMessages
{
  Process::ProcessModel& process;
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

    auto path = score::unmarshall<Path<Process::ProcessModel>>(it->value);
    if (!path.valid())
      return;

    {
      Process::ProcessModel* csp = path.try_find(doc);
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

DefaultProcessComponent::DefaultProcessComponent(
    Process::ProcessModel& proc,
    RemoteControl::DocumentPlugin& doc,
    QObject* parent_obj)
    : ProcessComponent{proc, doc, "Process", parent_obj}
{
  con(proc, &Process::ProcessModel::startExecution, this, [this] {
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

  con(proc, &Process::ProcessModel::stopExecution, this, [this] {
        system().receiver.removeHandler(this);

        process().forEachControl(
            [this](const Process::ControlInlet& inl, auto& val) {
              QObject::disconnect(
                  &inl, &Process::ControlInlet::valueChanged, this, nullptr);
            });
      });
}

DefaultProcessComponent::~DefaultProcessComponent()
{
  system().receiver.removeHandler(this);
}



struct IntervalMessages
{
  Scenario::IntervalModel& model;
  QString initMessage() const
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "IntervalAdded"sv;
    r.obj[score::StringConstant().Path] = Path{model};
    r.obj[score::StringConstant().Name] = model.metadata().getName();
    r.obj[score::StringConstant().Label] = model.metadata().getLabel();
    r.obj[score::StringConstant().Comment] = model.metadata().getComment();
    r.obj["Speed"] = model.duration.speed();
    r.obj["DefaultDuration"] = model.duration.defaultDuration().msec();
    r.stream.EndObject();
    return r.toString();
  }

  QString deinitMessage() const
  {
    using namespace std::literals;
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "IntervalRemoved"sv;
    r.obj[score::StringConstant().Path] = Path{model};
    r.stream.EndObject();
    return r.toString();
  }

  void
  speed(const rapidjson::Value& obj, const score::DocumentContext& doc) const
  {
    auto it = obj.FindMember("Path");
    if (it == obj.MemberEnd())
      return;

    auto speed_it = obj.FindMember("Speed");
    if (speed_it == obj.MemberEnd() || !speed_it->value.IsNumber())
      return;

    auto path = score::unmarshall<Path<Scenario::IntervalModel>>(it->value);
    if (!path.valid())
      return;

    {
      const double speed = speed_it->value.GetDouble();
      Scenario::IntervalModel* csp = path.try_find(doc);
      if (!csp)
        return;
      auto& cs = *csp;

      cs.duration.setSpeed(speed);
    }
  }

  void
  gain(const rapidjson::Value& obj, const score::DocumentContext& doc) const
  {
    auto it = obj.FindMember("Path");
    if (it == obj.MemberEnd())
      return;

    auto gain_it = obj.FindMember("Gain");
    if (gain_it == obj.MemberEnd() || !gain_it->value.IsNumber())
      return;

    auto path = score::unmarshall<Path<Scenario::IntervalModel>>(it->value);
    if (!path.valid())
      return;

    {
      const double gain = gain_it->value.GetDouble();
      Scenario::IntervalModel* csp = path.try_find(doc);
      if (!csp)
        return;
      if (csp->graphal())
        return;

      auto& cs = *csp;

      cs.outlet->setGain(gain);
    }
  }
};

IntervalBase::IntervalBase(
    Scenario::IntervalModel& Interval,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : parent_t{Interval, doc, "IntervalComponent", parent_comp}
{
  doc.registerInterval(Interval);
  con(Interval,
      &Scenario::IntervalModel::executionEvent,
      this,
      [this](Scenario::IntervalExecutionEvent ev) {
        auto& recv = system().receiver;
        switch (ev)
        {
          case Scenario::IntervalExecutionEvent::Playing:
          {
            // In case we do transport, executionStarted is called again without stopped
            recv.removeHandler(this);

            RemoteControl::Handler h;
            IntervalMessages msgs{this->interval()};

            h.setupDefaultHandler(msgs);

            h.answers["IntervalSpeed"] = [this, msgs](
                                             const rapidjson::Value& v,
                                             const RemoteControl::WSClient&) {
              msgs.speed(v, this->system().context());
            };
            h.answers["IntervalGain"] = [this, msgs](
                                            const rapidjson::Value& v,
                                            const RemoteControl::WSClient&) {
              msgs.gain(v, this->system().context());
            };

            recv.addHandler(this, std::move(h));
            break;
          }
          case Scenario::IntervalExecutionEvent::Stopped:
            recv.removeHandler(this);
            break;
          case Scenario::IntervalExecutionEvent::Paused:
          {
            using namespace std::literals;
            JSONReader r;
            r.stream.StartObject();
            r.obj[score::StringConstant().Message] = "IntervalPaused"sv;
            r.obj[score::StringConstant().Path] = Path{this->interval()};
            r.stream.EndObject();

            recv.sendMessage(r.toString());
            break;
          }
          case Scenario::IntervalExecutionEvent::Resumed:
          {
            using namespace std::literals;
            JSONReader r;
            r.stream.StartObject();
            r.obj[score::StringConstant().Message] = "IntervalResumed"sv;
            r.obj[score::StringConstant().Path] = Path{this->interval()};
            r.stream.EndObject();

            recv.sendMessage(r.toString());
            break;
          }
          default:
            break;
        }
      });
}

IntervalBase::~IntervalBase()
{
  assert(this->m_interval);

  system().unregisterInterval(*this->m_interval);
}

ProcessComponent* IntervalBase::make(
    ProcessComponentFactory& factory,
    Process::ProcessModel& process)
{
  return factory.make(process, system(), this);
}

ProcessComponent* IntervalBase::make(Process::ProcessModel& process)
{
  if(process.flags() & Process::ProcessFlags::ControlSurface)
    return new DefaultProcessComponent{process, this->system(), this};
  return nullptr;
}

bool IntervalBase::removing(
    const Process::ProcessModel& cst,
    const ProcessComponent& comp)
{
  return true;
}

}
