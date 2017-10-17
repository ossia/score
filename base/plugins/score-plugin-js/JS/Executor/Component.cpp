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
Component::Component(
    ::Engine::Execution::IntervalComponent& parentInterval,
    JS::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<JS::ProcessModel, ossia::node_process>{
        parentInterval, element, ctx, id, "JSComponent", parent}
{
  auto node = std::make_shared<js_node>(element.script());
  auto proc = std::make_shared<ossia::node_process>(ctx.plugin.execGraph, node);
  m_ossia_process = proc;
  m_node = node;

  auto& devices = ctx.devices.list();
  for(auto port : element.inlets())
  {
    auto inlet = ossia::make_inlet<ossia::value_port>();
    auto dest = Engine::score_to_ossia::makeDestination(devices, port->address());
    if(dest)
      inlet->address = &dest->address();

    node->inputs().push_back(inlet);
    ctx.plugin.inlets.insert({port, {m_node, inlet}});
  }

  for(auto port : element.outlets())
  {
    auto outlet = ossia::make_outlet<ossia::value_port>();
    auto dest = Engine::score_to_ossia::makeDestination(devices, port->address());
    if(dest)
      outlet->address = &dest->address();

    node->outputs().push_back(outlet);
    ctx.plugin.outlets.insert({port, {m_node, outlet}});
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
  m_inlets.clear();
  m_outlets.clear();
  m_valInlets.clear();
  m_valOutlets.clear();
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
        m_valInlets = m_object->findChildren<JS::ValueInlet*>();
        m_valOutlets = m_object->findChildren<JS::ValueOutlet*>();
      }
    }
  }
}

void js_node::run(ossia::execution_state&)
{
  for(int i = 0; i < m_valInlets.size(); i++)
  {
    auto& dat = m_inlets[i]->data.target<ossia::value_port>()->data;
    if(!dat.empty())
      m_valInlets[i]->setValue(dat.front().apply(ossia::qt::ossia_to_qvariant{}));
  }
  /*
  m_object->dumpObjectInfo();
  m_object->dumpObjectTree();
  for(int i = 0; i < m_object->metaObject()->methodCount(); i++)
    qDebug() << m_object->metaObject()->method(i).methodSignature();
  */
  QMetaObject::invokeMethod(
        m_object, "onTick",
        Qt::DirectConnection,
        Q_ARG(QVariant, double(this->m_date)),
        Q_ARG(QVariant, this->m_position),
        Q_ARG(QVariant, double(this->m_offset))
        );

  for(int i = 0; i < m_valOutlets.size(); i++)
  {
    auto& dat = m_outlets[i]->data.target<ossia::value_port>()->data;
    dat.push_back(ossia::qt::qt_to_ossia{}(m_valOutlets[i]->value()));
  }
}

}
}
