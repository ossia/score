#include <State/Domain.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/Qml/EditContext.hpp>

#include <ossia/network/domain/domain.hpp>

#include <vector>

namespace JS
{
QObject* EditJsContext::automate(QObject* interval, QObject* port)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
  if(!itv)
    return nullptr;
  auto ctl = qobject_cast<Process::Inlet*>(port);
  if(!ctl)
    return nullptr;
  if(ctl->type() != Process::PortType::Message)
    return nullptr;

  auto [m, _] = macro(*doc);
  return m->automate(*itv, *ctl);
}

QObject* EditJsContext::port(QObject* obj, QString name)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return nullptr;

  for(auto p : proc->inlets())
  {
    if(p->name() == name)
      return p;
  }
  for(auto p : proc->outlets())
  {
    if(p->name() == name)
      return p;
  }
  return nullptr;
}

QObject* EditJsContext::inlet(QObject* obj, int index)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return nullptr;
  if(index < 0 || index >= std::ssize(proc->inlets()))
    return nullptr;

  return proc->inlets()[index];
}

QObject* EditJsContext::inlet(QObject* obj, QString name)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return nullptr;

  for(auto p : proc->inlets())
    if(p->name() == name)
      return p;
  return nullptr;
}

int EditJsContext::inlets(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return 0;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return 0;
  return std::ssize(proc->inlets());
}

QObject* EditJsContext::outlet(QObject* obj, int index)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return nullptr;
  if(index < 0 || index >= std::ssize(proc->outlets()))
    return nullptr;

  return proc->outlets()[index];
}

QObject* EditJsContext::outlet(QObject* obj, QString name)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return nullptr;

  for(auto p : proc->outlets())
    if(p->name() == name)
      return p;
  return nullptr;
}

int EditJsContext::outlets(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return 0;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return 0;
  return std::ssize(proc->outlets());
}

QObject* EditJsContext::createCable(QObject* outlet, QObject* inlet)
{
  return createCable(outlet, inlet, Process::CableType::ImmediateGlutton);
}

QObject*
EditJsContext::createCable(QObject* outlet, QObject* inlet, Process::CableType tp)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto src = qobject_cast<Process::Outlet*>(outlet);
  if(!src)
    return nullptr;
  auto sink = qobject_cast<Process::Inlet*>(inlet);
  if(!sink)
    return nullptr;
  if(src->type() != sink->type())
    return nullptr;

  auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->document);
  auto [m, _] = macro(*doc);
  auto& c = m->createCable(root, *src, *sink, tp);
  return &c;
}

// A port stores its cables as paths, which may not resolve while a document is
// being torn down or if the other end was just removed. Both accessors below
// skip what does not resolve, so a script never sees a dangling cable and the
// indices of cable(port, i) stay in step with the count from cables(port).
static void
resolvedCables(const Process::Port& port, const score::DocumentContext& ctx,
               std::vector<Process::Cable*>& out)
{
  for(const auto& path : port.cables())
    if(auto c = path.try_find(ctx))
      out.push_back(c);
}

int EditJsContext::cables(QObject* port)
{
  auto doc = ctx();
  if(!doc)
    return 0;
  auto p = qobject_cast<Process::Port*>(port);
  if(!p)
    return 0;

  std::vector<Process::Cable*> found;
  resolvedCables(*p, *doc, found);
  return std::ssize(found);
}

QObject* EditJsContext::cable(QObject* port, int index)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto p = qobject_cast<Process::Port*>(port);
  if(!p)
    return nullptr;

  std::vector<Process::Cable*> found;
  resolvedCables(*p, *doc, found);
  if(index < 0 || index >= std::ssize(found))
    return nullptr;
  return found[index];
}

QObject* EditJsContext::source(QObject* cable)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto c = qobject_cast<Process::Cable*>(cable);
  if(!c)
    return nullptr;
  return c->source().try_find(*doc);
}

QObject* EditJsContext::sink(QObject* cable)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto c = qobject_cast<Process::Cable*>(cable);
  if(!c)
    return nullptr;
  return c->sink().try_find(*doc);
}

void EditJsContext::setAddress(QObject* obj, QString addr)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto proc = qobject_cast<Process::Port*>(obj);
  if(!proc)
    return;
  auto a = State::parseAddressAccessor(addr);
  if(!a)
    return;

  auto [m, _] = macro(*doc);
  m->setProperty<Process::Port::p_address>(*proc, std::move(*a));
}

void EditJsContext::setValue(QObject* obj, double value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, float(value));
}

void EditJsContext::setValue(QObject* obj, QVector2D value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(
      *port, ossia::vec2f{value.x(), value.y()});
}

void EditJsContext::setValue(QObject* obj, QVector3D value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(
      *port, ossia::vec3f{value.x(), value.y(), value.z()});
}

void EditJsContext::setValue(QObject* obj, QVector4D value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(
      *port, ossia::vec4f{value.x(), value.y(), value.z(), value.w()});
}

void EditJsContext::setValue(QObject* obj, QString value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, value.toStdString());
}

void EditJsContext::setValue(QObject* obj, bool value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, value);
}

void EditJsContext::setValue(QObject* obj, int value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, value);
}

void EditJsContext::setValue(QObject* obj, QList<QString> value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;

  std::vector<ossia::value> vals;
  for(auto& v : value)
  {
    vals.push_back(v.toStdString());
  }
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, std::move(vals));
}

// Score.setValue(Score.inlet(Score.find("Javascript"), 0), [ 0, 0.1, 2.0 ])
void EditJsContext::setValue(QObject* obj, QList<qreal> value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;

  std::vector<ossia::value> vals(value.begin(), value.end());
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, std::move(vals));
}

void EditJsContext::setValue(QObject* obj, QList<QVariant> value)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return;

  auto [m, _] = macro(*doc);
  m->setProperty<Process::ControlInlet::p_value>(*port, ossia::qt::qt_to_ossia{}(value));
}

double EditJsContext::min(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return {};
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return {};

  auto min = port->domain().get().get_min();
  if(!min.valid())
    return {};

  return ossia::convert<double>(min);
}

double EditJsContext::max(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return {};
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return {};
  
  auto max = port->domain().get().get_max();
  if(!max.valid())
    return {};

  return ossia::convert<double>(max);
}

QVector<QVariant> EditJsContext::enumValues(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return {};
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return {};

  QVector<QVariant> ret;
  auto vals = ossia::get_values(port->domain().get());
  if(vals.empty())
    return {};

  for(auto& v : vals)
  {
    if(auto str = v.target<std::string>())
    {
      ret.push_back(QString::fromStdString(*str));
    }
    else
    {
      ret.push_back(ossia::convert<double>(v));
	}
    // FIXME handle other kinds of enums
  }
  return ret;
}

}
