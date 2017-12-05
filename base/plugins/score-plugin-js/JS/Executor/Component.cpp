// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia-qt/js_utilities.hpp>
#include <vector>

#include "Component.hpp"
#include "JSAPIWrapper.hpp"
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <JS/JSProcessModel.hpp>
#include <QQmlComponent>

namespace JS
{
namespace Executor
{
struct js_control_updater
{
    ValueInlet& control;
    ossia::value v;
    void operator()() {
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
  auto proc = std::make_shared<ossia::node_process>(node);
  m_ossia_process = proc;
  m_node = node;

  const auto& inlets = element.inlets();
  auto& devices = ctx.devices.list();
  std::vector<int> control_indices;
  int i = 0;
  for(auto port : inlets)
  {
    switch(port->type)
    {
      case Process::PortType::Message:
      {
        auto inlet = ossia::make_inlet<ossia::value_port>();
        auto dest = Engine::score_to_ossia::makeDestination(devices, port->address());
        if(dest)
          inlet->address = &dest->address();

        node->inputs().push_back(inlet);
        if(dynamic_cast<Process::ControlInlet*>(port))
          control_indices.push_back(i);
        ctx.plugin.inlets.insert({port, {m_node, inlet}});
        break;
      }
      case Process::PortType::Audio:
      {
        auto inlet = ossia::make_inlet<ossia::audio_port>();
        auto dest = Engine::score_to_ossia::makeDestination(devices, port->address());
        if(dest)
          inlet->address = &dest->address();

        node->inputs().push_back(inlet);
        ctx.plugin.inlets.insert({port, {m_node, inlet}});
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
        auto outlet = ossia::make_outlet<ossia::value_port>();
        auto dest = Engine::score_to_ossia::makeDestination(devices, port->address());
        if(dest)
          outlet->address = &dest->address();

        node->outputs().push_back(outlet);
        ctx.plugin.outlets.insert({port, {m_node, outlet}});
        break;
      }

      case Process::PortType::Audio:
      {
        auto outlet = ossia::make_outlet<ossia::audio_port>();
        auto dest = Engine::score_to_ossia::makeDestination(devices, port->address());
        if(dest)
          outlet->address = &dest->address();

        node->outputs().push_back(outlet);
        ctx.plugin.outlets.insert({port, {m_node, outlet}});
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
      this->system().executionQueue.enqueue(js_control_updater{*val_inlet.first, val});
    });
  }

  ctx.plugin.execGraph->add_node(m_node);
  /*
  con(element, &JS::ProcessModel::scriptChanged,
      this, [=] (const QString& str) {
    system().executionQueue.enqueue(
          [proc=std::dynamic_pointer_cast<js_node>(m_node),
          &str]
    { proc->setScript(str); });
  });
  */
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
    auto& dat = m_valInlets[i].second->data.target<ossia::value_port>()->get_data();

    if(dat.empty())
    {
      // Use control
    }

    for(auto& val : dat)
    {
      m_valInlets[i].first->setValue(val.value.apply(ossia::qt::ossia_to_qvariant{}));
    }
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
    dat.add_raw_value(ossia::qt::qt_to_ossia{}(m_valOutlets[i].first->value()));
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
