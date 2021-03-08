// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include "JSAPIWrapper.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/ConsolePanel.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/Qml/Metatypes.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/js_utilities.hpp>
#include <ossia-qt/time.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>

#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlContext>

#include <Execution/DocumentPlugin.hpp>

#include <vector>

namespace JS
{
namespace Executor
{
class js_node final : public ossia::graph_node
{
public:
  js_node(ossia::execution_state& st);

  void setScript(const QString& val);

  void run(const ossia::token_request& t, ossia::exec_state_facade) noexcept override;

  ossia::execution_state& m_st;
  QQmlEngine* m_engine{};
  std::vector<Inlet*> m_jsInlets;
  std::vector<std::pair<ControlInlet*, ossia::inlet_ptr>> m_ctrlInlets;
  std::vector<std::pair<ValueInlet*, ossia::inlet_ptr>> m_valInlets;
  std::vector<std::pair<ValueOutlet*, ossia::outlet_ptr>> m_valOutlets;
  std::vector<std::pair<AudioInlet*, ossia::inlet_ptr>> m_audInlets;
  std::vector<std::pair<AudioOutlet*, ossia::outlet_ptr>> m_audOutlets;
  std::vector<std::pair<MidiInlet*, ossia::inlet_ptr>> m_midInlets;
  std::vector<std::pair<MidiOutlet*, ossia::outlet_ptr>> m_midOutlets;
  JS::Script* m_object{};
  ExecStateWrapper* m_execFuncs{};
  QJSValueList m_tickCall;

  void setupComponent_gui(JS::Script*);

  void setControl(std::size_t index, const QVariant& val)
  {
    if (index > m_jsInlets.size())
      return;
    if (auto v = qobject_cast<ValueInlet*>(m_jsInlets[index]))
      v->setValue(val);
  }

private:
  void setupComponent(QQmlComponent& c);
};

struct js_process final : public ossia::node_process
{
  using node_process::node_process;
  js_node& js() const { return static_cast<js_node&>(*node); }
  void start() override
  {
    if (auto obj = js().m_object)
      if (obj->start().isCallable())
        obj->start().call();
  }
  void stop() override
  {
    if (auto obj = js().m_object)
      if (obj->stop().isCallable())
        obj->stop().call();
  }
  void pause() override
  {
    if (auto obj = js().m_object)
      if (obj->pause().isCallable())
        obj->pause().call();
  }
  void resume() override
  {
    if (auto obj = js().m_object)
      if (obj->resume().isCallable())
        obj->resume().call();
  }
  void transport_impl(ossia::time_value date) override
  {
    QMetaObject::invokeMethod(
        js().m_object, "transport", Qt::DirectConnection, Q_ARG(QVariant, double(date.impl)));
  }
  void offset_impl(ossia::time_value date) override
  {
    QMetaObject::invokeMethod(
        js().m_object, "offset", Qt::DirectConnection, Q_ARG(QVariant, double(date.impl)));
  }
};
Component::Component(
    JS::ProcessModel& element,
    const ::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<JS::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id,
        "JSComponent",
        parent}
{
  std::shared_ptr<js_node> node = std::make_shared<js_node>(*ctx.execState);
  this->node = node;
  auto proc = std::make_shared<js_process>(node);
  m_ossia_process = proc;

  on_scriptChange(element.qmlData());
  con(element, &JS::ProcessModel::qmlDataChanged, this, &Component::on_scriptChange);
  SCORE_TODO_("Reinstate JS panel live scripting");
  /*
  if (!node->m_object)
    throw std::runtime_error{"Invalid JS"};

  connect(node->m_execFuncs, &ExecStateWrapper::exec,
      this, [=] (const QString& code) {
    auto& console = system().doc.app.panel<JS::PanelDelegate>();
    console.evaluate(code);
  }, Qt::QueuedConnection);
*/
}

Component::~Component() { }

void Component::on_scriptChange(const QString& script)
{
  auto& setup = system().setup;
  Execution::Transaction commands{system()};
  auto proc = std::dynamic_pointer_cast<js_node>(node);

  // 0. Unregister all the previous inlets / outlets
  setup.unregister_node_soft(m_oldInlets, m_oldOutlets, node);

  // 1. Create new inlet & outlet arrays
  ossia::inlets inls;
  ossia::outlets outls;
  std::vector<Execution::ExecutionCommand> controlSetups;

  {
    if (auto object = process().currentObject())
    {
      int idx = 0;
      for (auto n : object->children())
      {
        if (auto ctrl_in = qobject_cast<ControlInlet*>(n))
        {
          inls.push_back(new ossia::value_inlet);
          inls.back()->target<ossia::value_port>()->is_event = false;

          ++idx;
        }
        else if (auto val_in = qobject_cast<ValueInlet*>(n))
        {
          inls.push_back(new ossia::value_inlet);

          if (!val_in->is_control())
          {
            inls.back()->target<ossia::value_port>()->is_event = true;
          }
          else
          {
            auto ctrl = qobject_cast<Process::ControlInlet*>(process().inlets()[idx]);
            SCORE_ASSERT(ctrl);
            connect(
                ctrl, &Process::ControlInlet::valueChanged, this, [=](const ossia::value& val) {
                  this->in_exec([proc, val, idx] {
                    proc->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
                  });
                });
            controlSetups.push_back([proc, val = ctrl->value(), idx] {
              proc->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
            });
          }

          ++idx;
        }
        else if (auto aud_in = qobject_cast<AudioInlet*>(n))
        {
          inls.push_back(new ossia::audio_inlet);

          ++idx;
        }
        else if (auto mid_in = qobject_cast<MidiInlet*>(n))
        {
          inls.push_back(new ossia::midi_inlet);

          ++idx;
        }
        else if (auto val_out = qobject_cast<ValueOutlet*>(n))
        {
          outls.push_back(new ossia::value_outlet);
        }
        else if (auto aud_out = qobject_cast<AudioOutlet*>(n))
        {
          outls.push_back(new ossia::audio_outlet);
        }
        else if (auto mid_out = qobject_cast<MidiOutlet*>(n))
        {
          outls.push_back(new ossia::midi_outlet);
        }
      }
    }
  }

  // Send the updates to the node
  commands.push_back([proc, script, inls, outls]() mutable {
    proc->root_inputs() = std::move(inls);
    proc->root_outputs() = std::move(outls);
    proc->setScript(std::move(script));
  });

  commands.commands.reserve(commands.commands.size() + controlSetups.size());
  for (auto& cmd : controlSetups)
    commands.commands.push_back(std::move(cmd));
  controlSetups.clear();

  // Register the new inlets
  SCORE_ASSERT(process().inlets().size() == inls.size());
  SCORE_ASSERT(process().outlets().size() == outls.size());
  for (std::size_t i = 0; i < inls.size(); i++)
  {
    setup.register_inlet(*process().inlets()[i], inls[i], node, commands);
  }
  for (std::size_t i = 0; i < outls.size(); i++)
  {
    setup.register_outlet(*process().outlets()[i], outls[i], node, commands);
  }

  commands.run_all();

  m_oldInlets = process().inlets();
  m_oldOutlets = process().outlets();
}

js_node::js_node(ossia::execution_state& st) : m_st{st} { }

void js_node::setupComponent(QQmlComponent& c)
{
  auto object = c.create();
  if ((m_object = qobject_cast<JS::Script*>(object)))
  {
    m_object->setParent(m_engine);
    int input_i = 0;
    int output_i = 0;
    m_jsInlets.clear();
    m_ctrlInlets.clear();
    m_valInlets.clear();
    m_audInlets.clear();
    m_midInlets.clear();
    m_valOutlets.clear();
    m_audOutlets.clear();
    m_midOutlets.clear();
    for (auto n : m_object->children())
    {
      if (auto ctrl_in = qobject_cast<ControlInlet*>(n))
      {
        m_jsInlets.push_back(ctrl_in);
        m_ctrlInlets.push_back({ctrl_in, m_inlets[input_i++]});
      }
      else if (auto val_in = qobject_cast<ValueInlet*>(n))
      {
        m_jsInlets.push_back(val_in);
        m_valInlets.push_back({val_in, m_inlets[input_i++]});
      }
      else if (auto aud_in = qobject_cast<AudioInlet*>(n))
      {
        m_jsInlets.push_back(aud_in);
        m_audInlets.push_back({aud_in, m_inlets[input_i++]});
      }
      else if (auto mid_in = qobject_cast<MidiInlet*>(n))
      {
        m_jsInlets.push_back(mid_in);
        m_midInlets.push_back({mid_in, m_inlets[input_i++]});
      }
      else if (auto val_out = qobject_cast<ValueOutlet*>(n))
      {
        m_valOutlets.push_back({val_out, m_outlets[output_i++]});
      }
      else if (auto aud_out = qobject_cast<AudioOutlet*>(n))
      {
        m_audOutlets.push_back({aud_out, m_outlets[output_i++]});
      }
      else if (auto mid_out = qobject_cast<MidiOutlet*>(n))
      {
        m_midOutlets.push_back({mid_out, m_outlets[output_i++]});
      }
    }
  }
  else
  {
    delete object;
  }
}
void js_node::setScript(const QString& val)
{
  if (!m_engine)
  {
    m_engine = new QQmlEngine;
    m_execFuncs = new ExecStateWrapper{m_st, m_engine};
    m_engine->rootContext()->setContextProperty("Device", m_execFuncs);
  }

  if (val.trimmed().startsWith("import"))
  {
    QQmlComponent c{m_engine};
    c.setData(val.toUtf8(), QUrl());
    const auto& errs = c.errors();
    if (!errs.empty())
    {
      ossia::logger().error(
          "Uncaught exception at line {} : {}", errs[0].line(), errs[0].toString().toStdString());
    }
    else
    {
      setupComponent(c);
    }
  }
  else
  {
    qDebug() << "URL: " << val << QUrl::fromLocalFile(val);
    QQmlComponent c{m_engine, QUrl::fromLocalFile(val)};
    const auto& errs = c.errors();
    if (!errs.empty())
    {
      ossia::logger().error(
          "Uncaught exception at line {} : {}", errs[0].line(), errs[0].toString().toStdString());
    }
    else
    {
      setupComponent(c);
    }
  }
}

void js_node::run(const ossia::token_request& tk, ossia::exec_state_facade estate) noexcept
{
  if (!m_engine || !m_object)
    return;

  auto& tick = m_object->tick();
  if (!tick.isCallable())
    return;
  // if (t.date == ossia::Zero)
  //   return;

  QEventLoop e;
  // Copy audio
  for (int i = 0; i < m_audInlets.size(); i++)
  {
    auto& dat = m_audInlets[i].second->target<ossia::audio_port>()->samples;

    const int dat_size = (int)dat.size();
    QVector<QVector<double>> audio(dat_size);
    for (int i = 0; i < dat_size; i++)
    {
      const int dat_i_size = dat[i].size();
      audio[i].resize(dat_i_size);
      for (int j = 0; j < dat_i_size; j++)
        audio[i][j] = dat[i][j];
    }
    m_audInlets[i].first->setAudio(audio);
  }

  // Copy values
  for (int i = 0; i < m_valInlets.size(); i++)
  {
    auto& vp = *m_valInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    m_valInlets[i].first->clear();
    if (dat.empty())
    {
      if (vp.is_event)
      {
        m_valInlets[i].first->setValue(QVariant{});
      }
      else
      {
        // Use control or same method as before
      }
    }
    else
    {
      for (auto& val : dat)
      {
        // TODO why not js_value_outbound_visitor ? it makes more sense.
        auto qvar = val.value.apply(ossia::qt::ossia_to_qvariant{});
        m_valInlets[i].first->setValue(qvar);
        m_valInlets[i].first->addValue(
            QVariant::fromValue(ValueMessage{(double)val.timestamp, std::move(qvar)}));
      }
    }
  }

  // Copy controls
  for (int i = 0; i < m_ctrlInlets.size(); i++)
  {
    auto& vp = *m_ctrlInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    m_ctrlInlets[i].first->clear();
    if (!dat.empty())
    {
      auto var = dat.back().value.apply(ossia::qt::ossia_to_qvariant{});
      m_ctrlInlets[i].first->setValue(std::move(var));
    }
  }

  // Copy midi
  for (int i = 0; i < m_midInlets.size(); i++)
  {
    auto& dat = m_midInlets[i].second->target<ossia::midi_port>()->messages;
    m_midInlets[i].first->setMidi(dat);
  }

  if (m_tickCall.empty())
    m_tickCall = {{}, {}};

  m_tickCall[0] = m_engine->toScriptValue(tk);
  m_tickCall[1] = m_engine->toScriptValue(estate);

  auto res = tick.call(m_tickCall);
  if(res.isError())
  {
    qDebug() << "JS Error at "
             << res.property("lineNumber").toInt()
             << ": "
             << res.toString();
  }

  for (int i = 0; i < m_valOutlets.size(); i++)
  {
    auto& dat = *m_valOutlets[i].second->target<ossia::value_port>();
    const auto& v = m_valOutlets[i].first->value();
    if (!v.isNull() && v.isValid())
      dat.write_value(ossia::qt::qt_to_ossia{}(v), estate.physical_start(tk));
    for (auto& v : m_valOutlets[i].first->values)
    {
      dat.write_value(ossia::qt::qt_to_ossia{}(std::move(v.value)), v.timestamp);
    }
    m_valOutlets[i].first->clear();
  }

  for (int i = 0; i < m_midOutlets.size(); i++)
  {
    auto& dat = *m_midOutlets[i].second->target<ossia::midi_port>();
    for (const auto& mess : m_midOutlets[i].first->midi())
    {
      libremidi::message m;
      m.bytes.resize(mess.size());
      for (int j = 0; j < mess.size(); j++)
      {
        m.bytes[j] = mess[j];
      }
      dat.messages.push_back(std::move(m));
    }
    m_midOutlets[i].first->clear();
  }

  auto tick_start = estate.physical_start(tk);
  for (int out = 0; out < m_audOutlets.size(); out++)
  {
    auto& src = m_audOutlets[out].first->audio();
    auto& snk = m_audOutlets[out].second->target<ossia::audio_port>()->samples;
    snk.resize(src.size());
    for (int chan = 0; chan < src.size(); chan++)
    {
      snk[chan].resize(src[chan].size() + tick_start);

      for (int j = 0; j < src[chan].size(); j++)
        snk[chan][j + tick_start] = src[chan][j];
    }
  }

  e.processEvents();
  m_engine->collectGarbage();
}
}
}
