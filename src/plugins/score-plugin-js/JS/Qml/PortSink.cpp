#include "PortSink.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QQmlContext>
#include <QQmlEngine>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::PortSink)
namespace JS
{

PortSink::PortSink(QObject* parent)
    : QObject{parent}
{
  connect(this, &PortSink::processChanged, this, [this] { rebuild(); });
  connect(this, &PortSink::portChanged, this, [this] { rebuild(); });
}

PortSink::~PortSink() { }

void PortSink::setTarget(const QQmlProperty& prop)
{
  m_targetProperty = prop;
}

Process::ProcessModel* PortSink::processInstance() const noexcept
{
  auto doc = score::GUIAppContext().documents.currentDocument();
  if(!doc)
    return nullptr;

  auto& model = doc->model().modelDelegate();
  auto processes = model.findChildren<Process::ProcessModel*>(
      QString{}, Qt::FindChildrenRecursively);

  for(auto proc : processes)
  {
    if(proc->metadata().getName() == m_process)
    {
      return proc;
    }
  }

  return nullptr;
}

void PortSink::rebuild()
{
  m_outlet = nullptr;

  if(!parent())
    return;

  const auto port_type = m_port.typeId();
  if(port_type == QMetaType::Type::QString)
  {
    auto process = processInstance();
    if(!process)
      return;
    auto port_name = m_port.value<QString>();
    m_outlet = process->findChild<Process::Outlet*>(port_name);
    if(!m_outlet)
    {
      auto& ports = process->outlets();
      for(auto& port : ports)
      {
        if(port->name() == port_name)
        {
          m_outlet = qobject_cast<Process::Outlet*>(port);
          break;
        }
      }
    }
  }
  else if(port_type == QMetaType::Type::Int)
  {
    auto process = processInstance();
    if(!process)
      return;

    int i = m_port.toInt();

    auto& ports = process->outlets();
    if(i >= 0 && i < ports.size())
      m_outlet = qobject_cast<Process::Outlet*>(ports[i]);
  }
  else
  {
    m_outlet = qobject_cast<Process::Outlet*>(m_port.value<QObject*>());
  }

  if(!m_outlet)
    return;

  // TODO work more generally for any outlet
  if(auto value = qobject_cast<Process::ControlOutlet*>(m_outlet.get()))
  {
    connect(
        value, &Process::ControlOutlet::executionValueChanged, this,
        [this](const ossia::value& v) {
      auto vv = v.apply(ossia::qt::ossia_to_qvariant{});
      m_targetProperty.write(vv);
    });
  }
}
}
