#pragma once

#include <Engine/ApplicationPlugin.hpp>
#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Commands/EditPort.hpp>
#include <JS/Commands/ScriptMacro.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>
#include <score/actions/MenuManager.hpp>
#include <State/OSSIASerializationImpl.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <score/tools/MapCopy.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/value/format_value.hpp>

#include <QMenu>
#include <QJSEngine>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>
#include <score/tools/File.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <QJSValueIterator>
#include <wobjectimpl.h>
#include <Process/ProcessList.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Media/Step/Model.hpp>
#include <Media/Step/Commands.hpp>
#include <JS/Qml/Metatypes.hpp>

namespace JS
{
class JsUtils : public QObject
{
  W_OBJECT(JsUtils)
public:
    QByteArray readFile(QString path)
    {
      QFile f(path);
      if (f.open(QIODevice::ReadOnly))
        return f.readAll();
      return {};
    }
    W_SLOT(readFile)
};

struct ActionContext : public QObject
{
    W_OBJECT(ActionContext)
    public:
    QString Menu = "Menu";

    W_PROPERTY(QString, Menu READ Menu)
};
static TimeVal parseDuration(QString dur)
{
  if(auto tm = QTime::fromString(dur); tm.isValid())
  {
    return TimeVal::fromMsecs(tm.msec() + 1e3 * tm.second() + 1e3 * 60 * tm.minute() + 1e3 * 60 * 60 * tm.hour());
  }
  else
  {
    return TimeVal{ossia::flicks_per_second<int64_t> * 2};
  }
}
class EditJsContext : public QObject
{
  W_OBJECT(EditJsContext)
  using Macro = Scenario::Command::Macro;

  std::optional<Macro> m_macro;
  auto macro(const score::DocumentContext& doc)
  {
    struct clear {
      std::optional<Macro>& macro;
      bool clearOnDelete{};
      ~clear()
      {
        if(clearOnDelete)
        {
          macro->commit();
          macro = std::nullopt;
        }
      }
    };

    if(m_macro)
    {
      return clear{m_macro, false};
    }
    else
    {
      m_macro.emplace(new ScriptMacro, doc);
      return clear{m_macro, true};
    }
  }
public:
  EditJsContext() { }

  const score::DocumentContext* ctx()
  {
    return score::GUIAppContext().currentDocument();
  }

  /*
  void createOSCDevice(QString name, QString host, int in, int out)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
    Device::DeviceSettings set;
    set.name = name;
    set.deviceSpecificSettings
        = QVariant::fromValue(Protocols::OSCSpecificSettings{in, out, host});
    set.protocol = Protocols::OSCProtocolFactory::static_concreteKey();

    auto [m, _] = macro();
    m->submit(new Explorer::Command::LoadDevice{plug, std::move(set)});
  }
  W_SLOT(createOSCDevice)
*/
  void createAddress(QString addr, QString type)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto a = State::Address::fromString(addr);
    if (!a)
      return;

    auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
    auto [m, _] = macro(*doc);

    Device::FullAddressSettings set;
    set.address = *a;

    const ossia::net::parameter_data* t
        = ossia::default_parameter_for_type(type.toStdString());
    if (t)
    {
      set.unit = t->unit;
      if (t->bounding)
        set.clipMode = *t->bounding;
      if (t->domain)
        set.domain = *t->domain;

      set.ioType = ossia::access_mode::BI;
      set.value = t->value;
      if (set.value.get_type() == ossia::val_type::NONE)
      {
        set.value = ossia::init_value(ossia::underlying_type(t->type));
      }
    }
    m->submit(new Explorer::Command::AddWholeAddress{plug, std::move(set)});
  }
  W_SLOT(createAddress)

  QObject* createProcess(QObject* interval, QString name, QString data)
  {
    auto doc = ctx();
    if (!doc)
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
  W_SLOT(createProcess)

  void setName(QObject* sel, QString new_name)
  {
    using namespace Scenario;
    using namespace Scenario::Command;
    auto doc = ctx();
    if (!doc)
      return;

    auto [m, _] = macro(*doc);

    auto sanitize = [&](auto* ptr) {
      if (new_name == ptr->metadata().getName())
        return false;
      using T = std::remove_reference_t<decltype(*ptr)>;
      if (new_name.isEmpty())
        new_name = QString("%1.0").arg(Metadata<PrettyName_k, T>::get());
      return true;
    };

    if (auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
    {
      if (!sanitize(cst))
        return;

      m->submit(new ChangeElementName<Scenario::IntervalModel>{*cst, new_name});
    }
    else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
    {
      if (!sanitize(ev))
       return;

      m->submit(new ChangeElementName<Scenario::EventModel>{*ev, new_name});
    }
    else if (auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
    {
      if (!sanitize(tn))
        return;

      m->submit(new ChangeElementName<Scenario::TimeSyncModel>{*tn, new_name});
    }
    else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
    {
      if (!sanitize(st))
        return;

      m->submit(new ChangeElementName<Scenario::StateModel>{*st, new_name});
    }
    else if (auto p = qobject_cast<Process::ProcessModel*>(sel))
    {
      if (new_name == p->metadata().getName())
        return;

      if (new_name.isEmpty())
        new_name = QString("%1.0").arg(p->objectName());

      m->submit(new ChangeElementName<Process::ProcessModel>{*p, new_name});
    }
  }
  W_SLOT(setName)

  QObject* createBox(QObject* obj, QString startTime, QString duration, double y)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(obj);
    if (!scenar)
      return nullptr;

    auto t0 = parseDuration(startTime);
    auto tdur = parseDuration(duration);

    auto [m, _] = macro(*doc);
    auto& itv = m->createBox(*scenar, t0, t0 + tdur, y);
    return &itv;
  }
  W_SLOT(createBox)


  QObject* createIntervalAfter(QObject* obj, QString duration, double y)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;

    auto state = qobject_cast<Scenario::StateModel*>(obj);
    if (!state)
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
  W_SLOT(createIntervalAfter)

  QObject* port(QObject* obj, QString name)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto proc = qobject_cast<Process::ProcessModel*>(obj);
    if(!proc)
      return nullptr;

    for(auto p : proc->inlets()) {
      if(p->name() == name)
        return p;
    }
    for(auto p : proc->outlets()) {
      if(p->name() == name)
        return p;
    }
    return nullptr;
  }
  W_SLOT(port)

  QObject* inlet(QObject* obj, int index)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto proc = qobject_cast<Process::ProcessModel*>(obj);
    if(!proc)
      return nullptr;
    if(index < 0 || index >= std::ssize(proc->inlets()))
      return nullptr;

    return proc->inlets()[index];
  }
  W_SLOT(inlet)

  int inlets(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return 0;
    auto proc = qobject_cast<Process::ProcessModel*>(obj);
    if(!proc)
      return 0;
    return std::ssize(proc->inlets());
  }
  W_SLOT(inlets)

  QObject* outlet(QObject* obj, int index)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto proc = qobject_cast<Process::ProcessModel*>(obj);
    if(!proc)
      return nullptr;
    if(index < 0 || index >= std::ssize(proc->outlets()))
      return nullptr;

    return proc->outlets()[index];
  }
  W_SLOT(outlet)

  int outlets(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return 0;
    auto proc = qobject_cast<Process::ProcessModel*>(obj);
    if(!proc)
      return 0;
    return std::ssize(proc->outlets());
  }
  W_SLOT(outlets)

  void setAddress(QObject* obj, QString addr)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto proc = qobject_cast<Process::Port*>(obj);
    if(!proc)
      return;
    auto a = State::parseAddressAccessor(addr);
    if (!a)
      return;

    auto [m, _] = macro(*doc);
    m->setProperty<Process::Port::p_address>(*proc, std::move(*a));
  }
  W_SLOT(setAddress)

  void setValue(QObject* obj, double value)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return;
    auto [m, _] = macro(*doc);
    m->setProperty<Process::ControlInlet::p_value>(*port, float(value));
  }
  W_SLOT(setValue, (QObject*, double))

  void setValue(QObject* obj, QString value)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return;
    auto [m, _] = macro(*doc);
    m->setProperty<Process::ControlInlet::p_value>(*port, value.toStdString());
  }
  W_SLOT(setValue, (QObject*, QString))

  void setValue(QObject* obj, bool value)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return;
    auto [m, _] = macro(*doc);
    m->setProperty<Process::ControlInlet::p_value>(*port, value);
  }
  W_SLOT(setValue, (QObject*, bool))

  void setValue(QObject* obj, QList<QString> value)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return;

    std::vector<ossia::value> vals;
    for(auto& v : value) {
      vals.push_back(v.toStdString());
    }
    auto [m, _] = macro(*doc);
    m->setProperty<Process::ControlInlet::p_value>(*port, std::move(vals));
  }
  W_SLOT(setValue, (QObject*, QList<QString>))

  QString valueType(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return {};
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return {};

    auto& v = port->value();
    if(!v.valid())
      return {};

    QString ret;
    ossia::apply_nonnull([&] (const auto& t) {
      using type = std::decay_t<decltype(t)>;
      ret = Metadata<Json_k, type>::get();
    }, v);
    return ret;
  }
  W_SLOT(valueType)

  double min(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return {};
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return {};

    return port->domain().get().convert_min<double>();
  }
  W_SLOT(min)

  double max(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return {};
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return {};

    return port->domain().get().convert_max<double>();
  }
  W_SLOT(max)

  QVector<QString> enumValues(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return {};
    auto port = qobject_cast<Process::ControlInlet*>(obj);
    if(!port)
      return {};

    QVector<QString> ret;
    auto vals = ossia::get_values(port->domain().get());
    for(auto& v : vals) {
      if(auto str = v.target<std::string>()) {
        ret.push_back(QString::fromStdString(*str));
      }
    }
    return ret;
  }
  W_SLOT(enumValues)

  QObject* metadata(QObject* obj) const noexcept
  {
    if(!obj)
      return nullptr;
    return obj->findChild<score::ModelMetadata*>({}, Qt::FindDirectChildrenOnly);
  }
  W_SLOT(metadata)

  QObject* startState(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
    if (!itv)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
    if(!scenar)
      return nullptr;

    return &Scenario::startState(*itv, *scenar);
  }
  W_SLOT(startState)

  QObject* startEvent(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
    if (!itv)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
    if(!scenar)
      return nullptr;

    return &Scenario::startEvent(*itv, *scenar);
  }
  W_SLOT(startEvent)

  QObject* startSync(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
    if (!itv)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
    if(!scenar)
      return nullptr;

    return &Scenario::startTimeSync(*itv, *scenar);
  }
  W_SLOT(startSync)


  QObject* endState(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
    if (!itv)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
    if(!scenar)
      return nullptr;

    return &Scenario::endState(*itv, *scenar);
  }
  W_SLOT(endState)

  QObject* endEvent(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
    if (!itv)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
    if(!scenar)
      return nullptr;

    return &Scenario::endEvent(*itv, *scenar);
  }
  W_SLOT(endEvent)

  QObject* endSync(QObject* obj)
  {
    auto doc = ctx();
    if (!doc)
      return nullptr;
    auto itv = qobject_cast<Scenario::IntervalModel*>(obj);
    if (!itv)
      return nullptr;
    auto scenar = qobject_cast<Scenario::ProcessModel*>(itv->parent());
    if(!scenar)
      return nullptr;

    return &Scenario::endTimeSync(*itv, *scenar);
  }
  W_SLOT(endSync)


  void remove(QObject* obj)
  {
    if(!obj)
      return;

    auto doc = ctx();
    if (!doc)
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
  W_SLOT(remove)

  void setCurvePoints(QObject* process, QVector<QVariantList> points)
  {
    if(points.size() < 2)
      return;

    auto doc = ctx();
    if (!doc)
      return;

    auto proc = qobject_cast<Process::ProcessModel*>(process);
    if(!proc)
      return;

    auto curve = proc->findChild<Curve::Model*>();
    if(!curve)
      return;

    for(auto& pt : points) {
      if(pt.size() < 2)
        return;
    }

    int current_id = 0;
    std::vector<Curve::SegmentData> segt;

    double cur_x = points[0][0].toDouble();
    double cur_y = points[0][1].toDouble();

    for(int i = 1, N = std::ssize(points); i < N; i++) {
      const auto& pt = points[i];
      auto x = pt[0].toDouble();
      auto y = pt[1].toDouble();
      Curve::SegmentData dat;
      dat.id = Id<Curve::SegmentModel>{current_id};
      dat.start.rx() = cur_x;
      dat.start.ry() = cur_y;
      dat.end.rx() = x;
      dat.end.ry() = y;
      cur_x = x;
      cur_y = y;
      dat.previous = Id<Curve::SegmentModel>{current_id - 1};
      dat.following = Id<Curve::SegmentModel>{current_id + 1};
      dat.type = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
      dat.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});

      segt.push_back(dat);
      current_id++;
    }
    segt.front().previous = std::nullopt;
    segt.back().following = std::nullopt;

    auto [m, _] = macro(*doc);
    m->submit(new Curve::UpdateCurve{*curve, std::move(segt)});
  }
  W_SLOT(setCurvePoints)

  void setSteps(QObject* process, QVector<double> points)
  {
    if(points.empty())
      return;

    auto doc = ctx();
    if (!doc)
      return;

    auto proc = qobject_cast<Media::Step::Model*>(process);
    if(!proc)
      return;

    auto [m, _] = macro(*doc);
    m->submit(new Media::ChangeSteps{*proc, ossia::float_vector{points.begin(), points.end()}});
  }
  W_SLOT(setSteps)

  void automate(QObject* interval, QString addr)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
    if (!itv)
      return;

    auto [m, _] = macro(*doc);
    m->automate(*itv, addr);
  }
  W_SLOT(automate, (QObject*, QString))

  void automate(QObject* interval, QObject* port)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
    if (!itv)
      return;
    auto ctl = qobject_cast<Process::Inlet*>(port);
    if (!ctl)
      return;
    if(ctl->type() != Process::PortType::Message)
      return;

    auto [m, _] = macro(*doc);
    m->automate(*itv, *ctl);
  }
  W_SLOT(automate, (QObject*, QObject*))

  void startMacro()
  {
    auto doc = ctx();
    if (!doc)
      return;
    this->m_macro.emplace(new ScriptMacro, *doc);
  }
  W_SLOT(startMacro)

  void endMacro()
  {
    if(this->m_macro)
      this->m_macro->commit();
    this->m_macro = std::nullopt;
  }
  W_SLOT(endMacro)


  void undo()
  {
    auto doc = ctx();
    if (!doc)
      return;
    doc->document.commandStack().undo();
  }
  W_SLOT(undo)

  void redo()
  {
    auto doc = ctx();
    if (!doc)
      return;
    doc->document.commandStack().redo();
  }
  W_SLOT(redo)

  QObject* find(QString p)
  {
    auto doc = document();
    const auto meta = doc->findChildren<score::ModelMetadata*>();
    for (auto m : meta)
    {
      if (m->getName() == p)
      {
        return m->parent();
      }
    }
    return nullptr;
  }
  W_SLOT(find)

  QObject* document()
  {
    return score::GUIAppContext().documents.currentDocument();
  }
  W_SLOT(document)

  /// Execution ///

  void play(QObject* obj)
  {
    auto plug
        = score::GUIAppContext()
              .findGuiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    if (!plug)
      return;

    if (auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
    {
      plug->execution().playInterval(itv);
    }
    else if (auto state = qobject_cast<Scenario::StateModel*>(obj))
    {
      plug->execution().playState(
          &Scenario::parentScenario(*state), state->id());
    }
  }
  W_SLOT(play)

  void stop()
  {
    auto plug = score::GUIAppContext()
                    .findGuiApplicationPlugin<Engine::ApplicationPlugin>();
    if (plug)
      plug->execution().request_stop();
  }
  W_SLOT(stop)

  // File API
  QString readFile(QString path)
  {
    auto doc = ctx();
    if (!doc)
      return {};

    auto actual = score::locateFilePath(path, *doc);
    if(QFile f{actual}; f.exists() && f.open(QIODevice::ReadOnly))
    {
      return score::readFileAsQString(f);
    }
    else
    {
      return {};
    }
  }
  W_SLOT(readFile)

  QObject* selectedObject()
  {
    auto doc = ctx();
    if (!doc)
      return {};

    const auto& cur = doc->selectionStack.currentSelection();
    if(cur.empty())
      return nullptr;

    return *cur.begin();
  }
  W_SLOT(selectedObject)

  QVariantList selectedObjects()
  {
    auto doc = ctx();
    if (!doc)
      return {};

    const auto& cur = doc->selectionStack.currentSelection();
    if(cur.empty())
      return {};

    QVariantList list;
    for(auto& c : cur)
      list.push_back(QVariant::fromValue(c.data()));
    return list;
  }
  W_SLOT(selectedObjects)
};

class PanelDelegate final
    : public QObject
    , public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx)
      : score::PanelDelegate{ctx}
      , m_widget{new QWidget}
  {
    m_engine.installExtensions(QJSEngine::ConsoleExtension);
    m_engine.globalObject().setProperty(
        "Score", m_engine.newQObject(new EditJsContext));
    m_engine.globalObject().setProperty(
        "Util", m_engine.newQObject(new JsUtils));
    m_engine.globalObject().setProperty(
        "ActionContext", m_engine.newQObject(new ActionContext));
    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);
    m_widget->setStatusTip(QObject::tr(
                             "This panel prompts the scripting console \n"
                             "still in early development"));

    m_edit = new QPlainTextEdit{m_widget};
    m_edit->setTextInteractionFlags(Qt::TextEditorInteraction);

    lay->addWidget(m_edit, 1);
    m_lineEdit = new QLineEdit{m_widget};
    lay->addWidget(m_lineEdit, 0);

    // TODO ctrl-space !
    connect(m_lineEdit, &QLineEdit::editingFinished, this, [=] {
      auto txt = m_lineEdit->text();
      if (!txt.isEmpty())
      {
        evaluate(txt);
        m_lineEdit->clear();
        m_edit->verticalScrollBar()->setValue(
            m_edit->verticalScrollBar()->maximum());
      }
    });
  }

  QJSEngine& engine() noexcept { return m_engine; }

  void evaluate(const QString& txt)
  {
    m_edit->appendPlainText("> " + txt);
    auto res = m_engine.evaluate(txt);
    if (res.isError())
    {
      m_edit->appendPlainText("ERROR: " + res.toString() + "\n");
    }
    else if (!res.isUndefined())
    {
      m_edit->appendPlainText(res.toString() + "\n");
    }
  }

  QMenu* addMenu(QMenu* cur, QStringList names)
  {
    QMenu* newmenu{};

    for(int i = 0; i < names.size() - 1; i++)
    {
      for(auto act : cur->findChildren<QMenu*>(QString{}, Qt::FindDirectChildrenOnly))
      {
        if(act->title() == names[i])
        {
          cur = act;
          goto ok;
        }
      }

      newmenu = new QMenu{names[i], cur};
      cur->addMenu(newmenu);
      cur = newmenu;

      ok:
        continue;
    }
    return cur;
  }

  void importModule(const QString& path)
  {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QJSValue mod = m_engine.importModule(path);
    if(auto init = mod.property("initialize"); init.isCallable())
      init.call();
    if(auto init = mod.property("actions"); init.isArray())
    {
      QJSValueIterator it(init);
      while(it.hasNext())
      {
       if(it.next())
       {
         auto obj = it.value();
         if(obj.isObject())
         {
           auto name = obj.property("name");
           auto context = obj.property("context");
           auto action = obj.property("action");
           auto shortcut = obj.property("shortcut");
           if(!name.isString())
             return;
           if(context.toString() != "Menu")
             return;
           if(!action.isCallable())
             return;

           auto names = name.toString().split('/');
           if(names.empty())
             return;

           score::Menu& script = this->context().menus.get().at(score::Menus::Scripts());
           if(auto act = script.menu()->actions()[0]; act->objectName() == "DefaultScriptMenuAction")
             script.menu()->removeAction(act);

           auto menu = addMenu(script.menu(), names);

           auto act = new QAction{names.back(), this};
           if(auto str = shortcut.toString(); !str.isEmpty())
           {
             act->setShortcut(QKeySequence::fromString(str));
           }
           connect(act, &QAction::triggered, this, [action=std::move(action)] () mutable {
             auto res = action.call();
             if(res.isError())
             {
               qDebug() << "Script error: " << res.errorType() << res.toString();
             }
           });
           menu->addAction(act);
         }
       }
      }
    }

    auto obj = m_engine.globalObject();
    obj.setProperty(QFileInfo{path}.baseName(), mod);
    m_edit->appendPlainText(mod.toString());
#endif
  }

private:
  QWidget* widget() override { return m_widget; }

  const score::PanelStatus& defaultPanelStatus() const override
  {
    static const score::PanelStatus status{
        false,
        false,
        Qt::BottomDockWidgetArea,
        0,
        QObject::tr("Console"),
        "console",
        QObject::tr("Ctrl+Shift+C")};

    return status;
  }

  QWidget* m_widget{};
  QPlainTextEdit* m_edit{};
  QLineEdit* m_lineEdit{};
  QJSEngine m_engine;
};

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("6060a63c-26b1-4ec6-a468-27e72530ac69")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {
    return std::make_unique<PanelDelegate>(ctx);
  }
};
}


W_REGISTER_ARGTYPE(QVector<QVariantList>)
W_REGISTER_ARGTYPE(QList<QObject*>)
