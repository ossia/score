#pragma once

#include <Engine/ApplicationPlugin.hpp>
#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/Commands/ScriptMacro.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>

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
#include <wobjectimpl.h>
#include <Process/ProcessList.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>

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

  void createProcess(QObject* interval, QString name, QString data)
  {
    auto doc = ctx();
    if (!doc)
      return;
    auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
    if (!itv)
      return;

    auto& factories = doc->app.interfaces<Process::ProcessFactoryList>();
    auto [m, _] = macro(*doc);

    for(auto& fact : factories)
    {
      if(fact.prettyName().compare(name, Qt::CaseInsensitive))
      {
        m->createProcess(*itv, fact.concreteKey(), data, {});
        break;
      }
    }
  }
  W_SLOT(createProcess)

  void setCurvePoints(QObject* process, QVector<QVariantList> points)
  {
    auto doc = ctx();
    if (!doc)
      return;
    
    auto proc = qobject_cast<Process::ProcessModel*>(process);
    if(!proc)
      return;
    
    auto curve = proc->findChild<Curve::Model*>();
    if(!curve)
      return;

    Curve::SegmentData dat;
    dat.id = Id<Curve::SegmentModel>(0);
    dat.start = {0, 0};
    dat.end = {1, 1};
    dat.type = Curve::PointArraySegment::static_concreteKey();
    Curve::PointArraySegmentData data;

    data.min_x = INT_MAX;
    data.min_y = INT_MAX;
    data.max_x = INT_MIN;
    data.max_y = INT_MIN;

    for(auto& pt : points) {
      if(pt.size() == 2) {
        auto x = pt[0].toDouble();
        auto y = pt[1].toDouble();
        if(x < data.min_x) data.min_x = x;
        if(x > data.max_x) data.max_x = x;
        if(y < data.min_y) data.min_y = y;
        if(y > data.max_y) data.max_y = y;
        data.m_points.push_back({x, y});
      }
    }

    dat.specificSegmentData = QVariant::fromValue(std::move(dat));

    auto [m, _] = macro(*doc);
    m->submit(new Curve::UpdateCurve{*curve, {std::move(dat)}});
  }
  W_SLOT(setCurvePoints)

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
  W_SLOT(automate)

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
    m_engine.globalObject().setProperty(
        "Score", m_engine.newQObject(new EditJsContext));
    m_engine.globalObject().setProperty(
        "Util", m_engine.newQObject(new JsUtils));
    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);
    m_widget->setStatusTip(QObject::tr(
                             "This panel prompts the scripting console \n"
                             "still in early developement"));

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
