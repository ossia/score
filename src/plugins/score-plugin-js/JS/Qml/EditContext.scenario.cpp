#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <JS/Commands/ScriptMacro.hpp>
#include <JS/Qml/EditContext.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QTime>
namespace JS
{

EditJsContext::EditJsContext() { }

EditJsContext::~EditJsContext() { }

void EditJsContext::startMacro()
{
  auto doc = ctx();
  if(!doc)
    return;
  this->m_macro = std::make_unique<Macro>(new ScriptMacro, *doc);
}

void EditJsContext::endMacro()
{
  if(this->m_macro)
    this->m_macro->commit();
  this->m_macro.reset();
}

void EditJsContext::submit(Macro& m, score::Command* c)
{
  m.submit(c);
}

void EditJsContext::automate(QObject* interval, QString addr)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
  if(!itv)
    return;

  auto [m, _] = macro(*doc);
  m->automate(*itv, addr);
}

EditJsContext::MacroClear EditJsContext::macro(const score::DocumentContext& doc)
{
  if(m_macro)
  {
    return MacroClear{m_macro, false};
  }
  else
  {
    m_macro = std::make_unique<Macro>(new ScriptMacro, doc);
    return MacroClear{m_macro, true};
  }
}

EditJsContext::MacroClear::~MacroClear()
{
  if(clearOnDelete)
  {
    macro->commit();
    macro.reset();
  }
}
static TimeVal parseDuration(QString dur)
{
  if(auto tm = QTime::fromString(dur); tm.isValid())
  {
    return TimeVal::fromMsecs(
        tm.msec() + 1e3 * tm.second() + 1e3 * 60 * tm.minute()
        + 1e3 * 60 * 60 * tm.hour());
  }
  else
  {
    return TimeVal{ossia::flicks_per_second<int64_t> * 2};
  }
};

QObject* EditJsContext::createProcess(QObject* interval, QString name, QString data)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto& factories = doc->app.interfaces<Process::ProcessFactoryList>();
  Process::ProcessModelFactory* f{};
  for(auto& fact : factories)
  {
    if(fact.prettyName().compare(name, Qt::CaseInsensitive) == 0)
    {
      f = &fact;
      break;
    }
  }
  if(!f)
    return nullptr;

  if(auto itv = qobject_cast<Scenario::IntervalModel*>(interval))
  {
    auto [m, _] = macro(*doc);
    return m->createProcessInNewSlot(*itv, f->concreteKey(), data, {});
  }
  else if(auto st = qobject_cast<Scenario::StateModel*>(interval))
  {
    auto [m, _] = macro(*doc);
    return m->createProcess(*st, f->concreteKey(), data);
  }
  return nullptr;
}

void EditJsContext::setName(QObject* sel, QString new_name)
{
  using namespace Scenario;
  using namespace Scenario::Command;
  auto doc = ctx();
  if(!doc)
    return;

  auto [m, _] = macro(*doc);

  auto sanitize = [&](auto* ptr) {
    if(new_name == ptr->metadata().getName())
      return false;
    using T = std::remove_reference_t<decltype(*ptr)>;
    if(new_name.isEmpty())
      new_name = QString("%1.0").arg(Metadata<PrettyName_k, T>::get());
    return true;
  };

  if(auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
  {
    if(!sanitize(cst))
      return;

    m->submit(new ChangeElementName<Scenario::IntervalModel>{*cst, new_name});
  }
  else if(auto ev = qobject_cast<Scenario::EventModel*>(sel))
  {
    if(!sanitize(ev))
      return;

    m->submit(new ChangeElementName<Scenario::EventModel>{*ev, new_name});
  }
  else if(auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
  {
    if(!sanitize(tn))
      return;

    m->submit(new ChangeElementName<Scenario::TimeSyncModel>{*tn, new_name});
  }
  else if(auto st = qobject_cast<Scenario::StateModel*>(sel))
  {
    if(!sanitize(st))
      return;

    m->submit(new ChangeElementName<Scenario::StateModel>{*st, new_name});
  }
  else if(auto p = qobject_cast<Process::ProcessModel*>(sel))
  {
    if(new_name == p->metadata().getName())
      return;

    if(new_name.isEmpty())
      new_name = QString("%1.0").arg(p->objectName());

    m->submit(new ChangeElementName<Process::ProcessModel>{*p, new_name});
  }
}

QObject*
EditJsContext::createBox(QObject* obj, QString startTime, QString duration, double y)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(obj);
  if(!scenar)
    return nullptr;

  auto t0 = parseDuration(startTime);
  auto tdur = parseDuration(duration);

  auto [m, _] = macro(*doc);
  auto& itv = m->createBox(*scenar, t0, t0 + tdur, y);
  return &itv;
}

QObject* EditJsContext::createIntervalAfter(QObject* obj, QString duration, double y)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;

  auto state = qobject_cast<Scenario::StateModel*>(obj);
  if(!state)
    return nullptr;

  auto scenar = qobject_cast<Scenario::ProcessModel*>(state->parent());
  if(!scenar)
    return nullptr;

  auto& ev = Scenario::parentEvent(*state, *scenar);
  const auto t0 = ev.date();
  const auto tdur = parseDuration(duration);
  auto [m, _] = macro(*doc);

  if(state->nextInterval())
  {
    auto& new_state = m->createState(*scenar, ev.id(), y);
    auto& itv = m->createIntervalAfter(*scenar, new_state.id(), {t0 + tdur, y});
    return &itv;
  }
  else
  {
    auto& itv = m->createIntervalAfter(*scenar, state->id(), {t0 + tdur, y});
    return &itv;
  }
}
QObject* EditJsContext::startState(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
  if(!itv)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
  if(!scenar)
    return nullptr;

  return &Scenario::startState(*itv, *scenar);
}

QObject* EditJsContext::startEvent(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
  if(!itv)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
  if(!scenar)
    return nullptr;

  return &Scenario::startEvent(*itv, *scenar);
}

QObject* EditJsContext::startSync(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
  if(!itv)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
  if(!scenar)
    return nullptr;

  return &Scenario::startTimeSync(*itv, *scenar);
}

QObject* EditJsContext::endState(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
  if(!itv)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
  if(!scenar)
    return nullptr;

  return &Scenario::endState(*itv, *scenar);
}

QObject* EditJsContext::endEvent(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
  if(!itv)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
  if(!scenar)
    return nullptr;

  return &Scenario::endEvent(*itv, *scenar);
}

QObject* EditJsContext::endSync(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
  if(!itv)
    return nullptr;
  auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
  if(!scenar)
    return nullptr;

  return &Scenario::endTimeSync(*itv, *scenar);
}

void EditJsContext::remove(QObject* obj)
{
  if(!obj)
    return;

  auto doc = ctx();
  if(!doc)
    return;

  if(auto proc = qobject_cast<Process::ProcessModel*>(obj))
  {
    SCORE_TODO_("Delete processes from console");
  }
  else if(auto p = obj->parent())
  {
    if(auto scenar = qobject_cast<Scenario::ProcessModel*>(p))
    {
      auto [m, _] = macro(*doc);
      m->removeElements(*scenar, {static_cast<IdentifiedObjectAbstract*>(obj)});
    }
  }
}

QVariantList EditJsContext::messages(QObject* obj)
{
  QVariantList ret;
  if(!obj)
    return ret;

  auto doc = ctx();
  if(!doc)
    return ret;

  auto s = qobject_cast<Scenario::StateModel*>(obj);
  if(!s)
    return ret;

  auto msgs = Process::flatten(s->messages().rootNode());
  for(const auto& msg : msgs)
  {
    ret.push_back(QVariantMap{
        {"address", msg.address.toString()},
        {"value", msg.value.apply(ossia::qt::ossia_to_qvariant{})}});
  }
  return ret;
}

void EditJsContext::setMessages(QObject* obj, QVariantList msgs)
{
  if(!obj)
    return;

  auto doc = ctx();
  if(!doc)
    return;

  auto s = qobject_cast<Scenario::StateModel*>(obj);
  if(!s)
    return;

  State::MessageList state_msgs;
  for(const QVariant& msg : msgs)
  {
    const QVariantMap& obj = msg.toMap();
    if(auto addr = State::parseAddressAccessor(obj["address"].toString()))
      if(auto val = ossia::qt::qt_to_ossia{}(obj["value"]); val.valid())
        state_msgs.push_back(State::Message{std::move(*addr), std::move(val)});
  }

  auto [m, _] = macro(*doc);
  submit(*m, new Scenario::Command::ReplaceState{*s, state_msgs});
}

void EditJsContext::replaceAddress(QObjectList objects, QString before, QString after)
{
  if(objects.empty())
    return;

  auto doc = ctx();
  if(!doc)
    return;

  auto addr_before = State::parseAddress(before);
  if(!addr_before)
    return;

  auto addr_after = State::parseAddress(after);
  if(!addr_after)
    return;

  auto [m, _] = macro(*doc);
  m->findAndReplace(objects, *addr_before, *addr_after);
  // Score.replaceAddress(Score.selectedObjects(), "dlight:/", "toto:/");
}

}
