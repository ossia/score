// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia-qt/js_utilities.hpp>
#include <QEventLoop>
#include <vector>

#include "Component.hpp"
#include "JSAPIWrapper.hpp"
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <JS/JSProcessModel.hpp>
#include <ossia-qt/invoke.hpp>
#include <QQmlComponent>

namespace JS
{
namespace Executor
{
struct js_control_updater
{
    ValueInlet& control;
    ossia::value v;
    void operator()() const {
      control.setValue(v.apply(ossia::qt::ossia_to_qvariant{}));
    }
};
Component::Component(
    JS::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<JS::ProcessModel, ossia::node_process>{
         element, ctx, id, "JSComponent", parent}
{
  std::shared_ptr<js_node> node = std::make_shared<js_node>("");
  this->node = node;
  auto proc = std::make_shared<ossia::node_process>(node);
  m_ossia_process = proc;

  const auto& inlets = element.inlets();
  std::vector<int> control_indices;
  int i = 0;
  for(auto port : inlets)
  {
    switch(port->type)
    {
      case Process::PortType::Message:
      {
        node->inputs().push_back(ossia::make_inlet<ossia::value_port>());
        if(qobject_cast<Process::ControlInlet*>(port))
          control_indices.push_back(i);
        else
          node->inputs().back()->data.target<ossia::value_port>()->is_event = true;
        break;
      }
      case Process::PortType::Audio:
      {
        node->inputs().push_back(ossia::make_inlet<ossia::audio_port>());
        break;
      }
      case Process::PortType::Midi:
      {
        node->inputs().push_back(ossia::make_inlet<ossia::midi_port>());
        break;
      }
    }
    i++;
  }

  for(auto port : element.outlets())
  {
    switch(port->type)
    {
      case Process::PortType::Message:
      {
        node->outputs().push_back(ossia::make_outlet<ossia::value_port>());
        break;
      }
      case Process::PortType::Audio:
      {
        node->outputs().push_back(ossia::make_outlet<ossia::audio_port>());
        break;
      }
      case Process::PortType::Midi:
      {
        node->outputs().push_back(ossia::make_outlet<ossia::midi_port>());
        break;
      }
    }
  }

  node->setScript(element.script());

  // Set-up controls
  for(auto ctrl_idx : control_indices)
  {
    auto port = inlets[ctrl_idx];
    auto ctrl = static_cast<Process::ControlInlet*>(port);
    auto val_inlet = node->m_valInlets[ctrl_idx];
    connect(ctrl, &Process::ControlInlet::valueChanged,
            this, [=] (const ossia::value& val) {
      this->in_exec(js_control_updater{*val_inlet.first, val});
    });
    js_control_updater{*val_inlet.first, ctrl->value()}();
  }

  /*
  con(element, &JS::ProcessModel::scriptChanged,
      this, [=] (const QString& str) {
    in_exec(
          [proc=std::dynamic_pointer_cast<js_node>(m_node),
          &str]
    { proc->setScript(str); });
  });
  */
}

Component::~Component()
{
}

void js_node::setScript(const QString& val)
{
  if(val.trimmed().startsWith("import"))
  {
    QQmlComponent c{&m_engine};
    c.setData(val.toUtf8(), QUrl());
    const auto& errs = c.errors();
    if(!errs.empty())
    {
      ossia::logger()
          .error("Uncaught exception at line {} : {}",
                 errs[0].line(),
          errs[0].toString().toStdString());
    }
    else
    {
      m_object = c.create();
      if(m_object)
      {
        m_object->setParent(&m_engine);
        int inlets_i = 0;
        int outlets_i = 0;
        for(auto n : m_object->children())
        {
          if(auto val_in = qobject_cast<ValueInlet*>(n))
          {
            m_valInlets.push_back({val_in, inputs()[inlets_i]});
            inlets_i++;
          }
          else if(auto aud_in = qobject_cast<AudioInlet*>(n))
          {
            m_audInlets.push_back({aud_in, inputs()[inlets_i]});
            inlets_i++;
          }
          else if(auto mid_in = qobject_cast<MidiInlet*>(n))
          {
            m_midInlets.push_back({mid_in, inputs()[inlets_i]});
            inlets_i++;
          }
          else if(auto val_out = qobject_cast<ValueOutlet*>(n))
          {
            m_valOutlets.push_back({val_out, outputs()[outlets_i]});
            outlets_i++;
          }
          else if(auto aud_out = qobject_cast<AudioOutlet*>(n))
          {
            m_audOutlets.push_back({aud_out, outputs()[outlets_i]});
            outlets_i++;
          }
          else if(auto mid_out = qobject_cast<MidiOutlet*>(n))
          {
            m_midOutlets.push_back({mid_out, outputs()[outlets_i]});
            outlets_i++;
          }
        }
      }
    }
  }
}

void js_node::run(ossia::token_request t, ossia::execution_state&)
{
  if(t.date == ossia::Zero)
    return;

  // Copy audio
  for(int i = 0; i < m_audInlets.size(); i++)
  {
    auto& dat = m_audInlets[i].second->data.target<ossia::audio_port>()->samples;

    const int dat_size = (int)dat.size();
    QVector<QVector<double>> audio(dat_size);
    for(int i = 0; i < dat_size; i++)
    {
      const int dat_i_size = dat[i].size();
      audio[i].resize(dat_i_size);
      for(int j = 0; j < dat_i_size; j++)
        audio[i][j] = dat[i][j];
    }
    m_audInlets[i].first->setAudio(audio);
  }

  // Copy values
  for(int i = 0; i < m_valInlets.size(); i++)
  {
    auto& vp = *m_valInlets[i].second->data.target<ossia::value_port>();
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

    for(auto& val : dat)
    {
      auto qvar = val.value.apply(ossia::qt::ossia_to_qvariant{});
      m_valInlets[i].first->setValue(qvar);
      m_valInlets[i].first->addValue(QVariant::fromValue(ValueMessage{(double)val.timestamp, std::move(qvar)}));
    }
  }

  // Copy midi
  for(int i = 0; i < m_midInlets.size(); i++)
  {
    auto& dat = m_midInlets[i].second->data.target<ossia::midi_port>()->messages;
    m_midInlets[i].first->setMidi(dat);
  }

  QMetaObject::invokeMethod(
        m_object, "onTick",
        Qt::DirectConnection,
        Q_ARG(QVariant, double(m_prev_date)),
        Q_ARG(QVariant, double(t.date)),
        Q_ARG(QVariant, t.position),
        Q_ARG(QVariant, double(t.offset))
        );

  for(int i = 0; i < m_valOutlets.size(); i++)
  {
    auto& dat = *m_valOutlets[i].second->data.target<ossia::value_port>();
    const auto& v = m_valOutlets[i].first->value();
    if(!v.isNull() && v.isValid())
      dat.add_raw_value(ossia::qt::qt_to_ossia{}(v));
    for(auto& v : m_valOutlets[i].first->values)
    {
      dat.add_value(ossia::qt::qt_to_ossia{}(std::move(v.value)), v.timestamp);
    }
    m_valOutlets[i].first->clear();
  }

  for(int i = 0; i < m_midOutlets.size(); i++)
  {
    auto& dat = *m_midOutlets[i].second->data.target<ossia::midi_port>();
    for(const auto& mess : m_midOutlets[i].first->midi())
    {
      mm::MidiMessage m;
      m.data.resize(mess.size());
      for(int j = 0; j < mess.size(); j++)
      {
        m.data[j] = mess[j];
      }
      dat.messages.push_back(std::move(m));
    }
    m_midOutlets[i].first->clear();
  }

  for(int out = 0; out < m_audOutlets.size(); out++)
  {
    auto& src = m_audOutlets[out].first->audio();
    auto& snk = m_audOutlets[out].second->data.target<ossia::audio_port>()->samples;
    snk.resize(src.size());
    for(int chan = 0; chan < src.size(); chan++)
    {
      snk[chan].resize(src[chan].size() + int64_t(t.offset));

      for(int j = 0; j < src[chan].size(); j++)
        snk[chan][j + int64_t(t.offset)] = src[chan][j];
    }
  }
}

}
}
