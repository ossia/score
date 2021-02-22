#include "Interval.hpp"
#include <score/tools/Bind.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/EntitySerialization.hpp>

namespace RemoteControl
{

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

  void speed(
      const rapidjson::Value& obj,
      const score::DocumentContext& doc) const
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
      if(!csp)
        return;
      auto& cs = *csp;

      cs.duration.setSpeed(speed);
    }
  }
};

IntervalBase::IntervalBase(
    const Id<score::Component>& id,
    Scenario::IntervalModel& Interval,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : parent_t{Interval, doc, id, "IntervalComponent", parent_comp}
{
  doc.registerInterval(Interval);
  con(Interval, &Scenario::IntervalModel::executionStarted,
      this, [this] {
    RemoteControl::Handler h;
    IntervalMessages msgs{this->interval()};

    h.setupDefaultHandler(msgs);

    h.answers["IntervalSpeed"] =
        [this, msgs](const rapidjson::Value& v, const RemoteControl::WSClient&) {
      msgs.speed(v, this->system().context());
    };

    system().receiver.addHandler(this, std::move(h));
  });

  con(Interval, &Scenario::IntervalModel::executionStopped,
      this, [this] {
    system().receiver.removeHandler(this);
  });
}

IntervalBase::~IntervalBase()
{
  SCORE_ASSERT(this->m_interval);

  system().unregisterInterval(*this->m_interval);
}

ProcessComponent* IntervalBase::make(
    const Id<score::Component>& id,
    ProcessComponentFactory& factory,
    Process::ProcessModel& process)
{
  return factory.make(process, system(), id, this);
}

bool IntervalBase::removing(const Process::ProcessModel& cst, const ProcessComponent& comp)
{
  return true;
}

}
