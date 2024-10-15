#include "CPUNode.hpp"

#include <JS/ConsolePanel.hpp>
#include <JS/Executor/ExecutionHelpers.hpp>
#include <JS/Qml/Utils.hpp>

#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/MapSerialization.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <ossia-qt/qml_engine_functions.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QQmlContext>
#include <QQmlEngine>

namespace JS
{

js_node::js_node(ossia::execution_state& st)
    : m_st{st}
{
  m_not_threadable = true;
}

js_node::~js_node()
{
  delete m_engine;
}

void js_node::setupComponent()
{
  SCORE_ASSERT(m_object);
  m_object->setParent(m_engine);
  int input_i = 0;
  int output_i = 0;

  for(auto n : m_object->children())
  {
    if(auto imp_in = qobject_cast<Impulse*>(n))
    {
      m_jsInlets.push_back(imp_in);
      m_impulseInlets.push_back({imp_in, m_inlets[input_i++]});
    }
    else if(auto ctrl_in = qobject_cast<ControlInlet*>(n))
    {
      m_jsInlets.push_back(ctrl_in);
      m_ctrlInlets.push_back({ctrl_in, m_inlets[input_i++]});
    }
    else if(auto val_in = qobject_cast<ValueInlet*>(n))
    {
      m_jsInlets.push_back(val_in);
      m_valInlets.push_back({val_in, m_inlets[input_i++]});
    }
    else if(auto aud_in = qobject_cast<AudioInlet*>(n))
    {
      m_jsInlets.push_back(aud_in);
      m_audInlets.push_back({aud_in, m_inlets[input_i++]});
    }
    else if(auto mid_in = qobject_cast<MidiInlet*>(n))
    {
      m_jsInlets.push_back(mid_in);
      m_midInlets.push_back({mid_in, m_inlets[input_i++]});
    }
    else if(auto val_out = qobject_cast<ValueOutlet*>(n))
    {
      m_valOutlets.push_back({val_out, m_outlets[output_i++]});
    }
    else if(auto aud_out = qobject_cast<AudioOutlet*>(n))
    {
      m_audOutlets.push_back({aud_out, m_outlets[output_i++]});
    }
    else if(auto mid_out = qobject_cast<MidiOutlet*>(n))
    {
      m_midOutlets.push_back({mid_out, m_outlets[output_i++]});
    }
  }
}

void js_node::setScript(const QString& val)
{
  if(!m_engine)
  {
    m_engine = new QQmlEngine;
    m_execFuncs = new ossia::qt::qml_engine_functions{
        m_st.exec_devices(), [&]<typename... Args>(Args&&... args) {
      m_st.insert(std::forward<Args>(args)...);
    }, m_engine};

    m_engine->rootContext()->setContextProperty("Util", new JsUtils);
    m_engine->rootContext()->setContextProperty("Device", m_execFuncs);

    QObject::connect(
        m_execFuncs, &ossia::qt::qml_engine_functions::system, qApp,
        [](const QString& code) {
      std::thread{[code] { ::system(code.toStdString().c_str()); }}.detach();
    }, Qt::QueuedConnection);

    if(auto* js_panel = score::GUIAppContext().findPanel<JS::PanelDelegate>())
    {
      QObject::connect(
          m_execFuncs, &ossia::qt::qml_engine_functions::exec, js_panel,
          &JS::PanelDelegate::evaluate, Qt::QueuedConnection);
      QObject::connect(
          m_execFuncs, &ossia::qt::qml_engine_functions::compute, m_execFuncs,
          [this, js_panel](const QString& code, const QString& cbname) {
        // Exec thread

        // Callback ran in UI thread
        auto cb = [this, cbname](QVariant v) {
          // Go back to exec thread, we have to go through the normal engine exec ctx
          ossia::qt::run_async(m_execFuncs, [this, v, cbname] {
            auto mo = m_object->metaObject();
            for(int i = 0; i < mo->methodCount(); i++)
            {
              if(mo->method(i).name() == cbname)
              {
                mo->method(i).invoke(
                    m_object, Qt::DirectConnection, QGenericReturnArgument(),
                    QArgument<QVariant>{"v", v});
              }
            }
          });
        };

        // Go to ui thread
        ossia::qt::run_async(js_panel, [js_panel, code, cb]() {
          js_panel->compute(code, cb); // This invokes cb
        });
      }, Qt::DirectConnection);
    }
  }

  m_jsInlets.clear();
  m_ctrlInlets.clear();
  m_impulseInlets.clear();
  m_valInlets.clear();
  m_audInlets.clear();
  m_midInlets.clear();
  m_valOutlets.clear();
  m_audOutlets.clear();
  m_midOutlets.clear();

  delete m_object;
  if((m_object = createJSObject(val, m_engine)))
    setupComponent();
}

void js_node::run(
    const ossia::token_request& tk, ossia::exec_state_facade estate) noexcept
{
  if(!m_engine || !m_object)
    return;

  auto& tick = m_object->tick();
  if(!tick.isCallable())
    return;
  // if (t.date == ossia::Zero)
  //   return;

  QEventLoop e;
  if(std::exchange(triggerStart, false))
  {
    if(m_object->start().isCallable())
      m_object->start().call();
  }
  if(std::exchange(triggerPause, false))
  {
    if(m_object->pause().isCallable())
      m_object->pause().call();
  }
  if(std::exchange(triggerResume, false))
  {
    if(m_object->resume().isCallable())
      m_object->resume().call();
  }

  // Copy audio
  for(std::size_t inl_i = 0; inl_i < m_audInlets.size(); inl_i++)
  {
    auto& dat = m_audInlets[inl_i].second->target<ossia::audio_port>()->get();

    const int dat_size = std::ssize(dat);
    QVector<QVector<double>> audio(dat_size);
    for(int i = 0; i < dat_size; i++)
    {
      const int dat_i_size = dat[i].size();
      audio[i].resize(dat_i_size);
      for(int j = 0; j < dat_i_size; j++)
        audio[i][j] = dat[i][j];
    }
    m_audInlets[inl_i].first->setAudio(audio);
  }

  // Copy values
  for(std::size_t i = 0; i < m_valInlets.size(); i++)
  {
    auto& vp = *m_valInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    m_valInlets[i].first->clear();
    if(dat.empty())
    {
      if(vp.is_event)
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
      for(auto& val : dat)
      {
        // TODO why not js_value_outbound_visitor ? it makes more sense.
        auto qvar = val.value.apply(ossia::qt::ossia_to_qvariant{});
        m_valInlets[i].first->setValue(qvar);
        m_valInlets[i].first->addValue(
            QVariant::fromValue(InValueMessage{(double)val.timestamp, std::move(qvar)}));
      }
    }
  }

  // Impulses are handed separately

  for(std::size_t i = 0; i < m_impulseInlets.size(); i++)
  {
    auto& vp = *m_impulseInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    for(int k = 0; k < dat.size(); k++)
    {
      m_impulseInlets[i].first->impulse();
    }
  }

  // Copy controls
  for(std::size_t i = 0; i < m_ctrlInlets.size(); i++)
  {
    auto& vp = *m_ctrlInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    if(!dat.empty())
    {
      auto var = dat.back().value.apply(ossia::qt::ossia_to_qvariant{});
      m_ctrlInlets[i].first->setValue(std::move(var));
    }
  }

  // Copy midi
  for(std::size_t i = 0; i < m_midInlets.size(); i++)
  {
    auto& dat = m_midInlets[i].second->target<ossia::midi_port>()->messages;
    m_midInlets[i].first->setMidi(dat);
  }

  if(m_tickCall.empty())
    m_tickCall = {{}, {}};

  m_tickCall[0] = m_engine->toScriptValue(TokenRequestValueType{tk});
  m_tickCall[1] = m_engine->toScriptValue(ExecutionStateValueType{estate});

  auto res = tick.call(m_tickCall);
  if(res.isError())
  {
    qDebug() << "JS Error at " << res.property("lineNumber").toInt() << ": "
             << res.toString();
  }

  const auto [tick_start, d] = estate.timings(tk);
  for(std::size_t i = 0; i < m_valOutlets.size(); i++)
  {
    auto& ossia_port = *m_valOutlets[i].second->target<ossia::value_port>();
    auto& js_port = *m_valOutlets[i].first;

    const QJSValue& v = js_port.value();
    if(!v.isNull() && !v.isError() && !v.isUndefined())
    {
      ossia_port.write_value(ossia::qt::value_from_js(v), tick_start);
    }
    for(auto& v : js_port.values)
    {
      ossia_port.write_value(ossia::qt::value_from_js(std::move(v.value)), v.timestamp);
    }
    js_port.clear();
  }

  for(std::size_t i = 0; i < m_midOutlets.size(); i++)
  {
    auto& dat = *m_midOutlets[i].second->target<ossia::midi_port>();
    for(const auto& mess : m_midOutlets[i].first->midi())
    {
      libremidi::message m;
      m.bytes.resize(mess.size());
      for(int j = 0; j < mess.size(); j++)
      {
        m.bytes[j] = mess[j];
      }
      dat.messages.push_back(std::move(m));
    }
    m_midOutlets[i].first->clear();
  }

  for(std::size_t out = 0; out < m_audOutlets.size(); out++)
  {
    auto& src = m_audOutlets[out].first->audio();
    auto& snk = m_audOutlets[out].second->target<ossia::audio_port>()->get();
    snk.resize(src.size());
    for(int chan = 0; chan < src.size(); chan++)
    {
      snk[chan].resize(src[chan].size() + tick_start);

      for(int j = 0; j < src[chan].size(); j++)
        snk[chan][j + tick_start] = src[chan][j];
    }
  }

  if(std::exchange(triggerStop, false))
  {
    if(m_object->stop().isCallable())
      m_object->stop().call();
  }
  e.processEvents();
  if(m_gcIndex++ % 64 == 0)
    m_engine->collectGarbage();
}
}
