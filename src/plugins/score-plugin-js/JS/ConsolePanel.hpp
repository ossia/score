#pragma once

#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/AddDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/Commands/ScriptMacro.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

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

#include <Engine/ApplicationPlugin.hpp>
#include <wobjectimpl.h>
namespace JS
{

class EditJsContext : public QObject
{
  W_OBJECT(EditJsContext)
public:
  EditJsContext() { }

  const score::DocumentContext& ctx()
  {
    return score::GUIAppContext().currentDocument();
  }

  void createOSCDevice(QString name, QString host, int in, int out)
  {
    auto& plug = ctx().plugin<Explorer::DeviceDocumentPlugin>();
    Device::DeviceSettings set;
    set.name = name;
    set.deviceSpecificSettings
        = QVariant::fromValue(Protocols::OSCSpecificSettings{in, out, host});
    set.protocol = Protocols::OSCProtocolFactory::static_concreteKey();

    Scenario::Command::Macro m{new ScriptMacro, ctx()};
    m.submit(new Explorer::Command::AddDevice{plug, std::move(set)});
    m.commit();
  }
  W_SLOT(createOSCDevice)

  void createAddress(QString addr, QString type)
  {
    auto a = State::Address::fromString(addr);
    if (!a)
      return;

    auto& plug = ctx().plugin<Explorer::DeviceDocumentPlugin>();
    Scenario::Command::Macro m{new ScriptMacro, ctx()};

    Device::FullAddressSettings set;
    set.address = *a;

    const ossia::net::parameter_data* t = ossia::default_parameter_for_type(type.toStdString());
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
    m.submit(new Explorer::Command::AddWholeAddress{plug, std::move(set)});
    m.commit();
  }
  W_SLOT(createAddress)

  void automate(QObject* interval, QString addr)
  {
    auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
    if (!itv)
      return;
    Scenario::Command::Macro m{new ScriptMacro, ctx()};
    m.automate(*itv, addr);
    m.commit();
  }
  W_SLOT(automate)

  void undo() { ctx().document.commandStack().undo(); }
  W_SLOT(undo)

  void redo() { ctx().document.commandStack().redo(); }
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

  QObject* document() { return score::GUIAppContext().documents.currentDocument(); }
  W_SLOT(document)

  /// Execution ///

  void play(QObject* obj)
  {
    auto plug
        = score::GUIAppContext().findGuiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    if (!plug)
      return;

    if (auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
    {
      plug->execution().playInterval(Scenario::parentScenario(*itv), itv->id());
    }
    else if (auto state = qobject_cast<Scenario::StateModel*>(obj))
    {
      plug->execution().playState(Scenario::parentScenario(*state), state->id());
    }
  }
  W_SLOT(play)

  void stop()
  {
    const auto& context = score::GUIAppContext();
    if (context.applicationSettings.gui)
    {
      auto& stop_action = context.actions.action<Actions::Stop>();
      stop_action.action()->trigger();
    }
    else
    {
      context.guiApplicationPlugin<Engine::ApplicationPlugin>().on_stop();
    }
  }
  W_SLOT(stop)
};

class PanelDelegate final : public QObject, public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx)
      : score::PanelDelegate{ctx}, m_widget{new QWidget}
  {
    m_engine.globalObject().setProperty("Score", m_engine.newQObject(new EditJsContext));
    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);
    m_edit = new QPlainTextEdit{m_widget};
    m_edit->setTextInteractionFlags(Qt::TextBrowserInteraction);

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
        m_edit->verticalScrollBar()->setValue(m_edit->verticalScrollBar()->maximum());
      }
    });
  }

  QJSEngine& engine() noexcept { return m_engine; }

  void evaluate(const QString& txt)
  {
    m_edit->appendPlainText(">> " + txt);
    auto res = m_engine.evaluate(txt);
    if (res.isError())
    {
      m_edit->appendPlainText("ERROR: " + res.toString() + "\n");
    }
    else
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

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override
  {
    return std::make_unique<PanelDelegate>(ctx);
  }
};
}
