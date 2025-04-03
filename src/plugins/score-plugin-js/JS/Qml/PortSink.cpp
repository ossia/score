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

void PortSink::rebuild()
{
  m_outlet = nullptr;

  if(!parent())
    return;

  auto doc = score::GUIAppContext().documents.currentDocument();
  if(!doc)
    return;

  auto& model = doc->model().modelDelegate();
  auto processes = model.findChildren<Process::ProcessModel*>(
      QString{}, Qt::FindChildrenRecursively);

  Process::ProcessModel* process = nullptr;
  for(auto proc : processes)
  {
    if(proc->metadata().getName() == m_process)
    {
      process = proc;
      break;
    }
  }
  if(!process)
    return;

  auto& ports = process->outlets();
  if(m_port.typeId() == QMetaType::Type::QString)
  {
    auto port_name = m_port.value<QString>();
    m_outlet = process->findChild<Process::Outlet*>(port_name);
    if(!m_outlet)
    {
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
  else if(m_port.typeId() == QMetaType::Type::Int)
  {
    int i = m_port.toInt();
    if(i >= 0 && i < ports.size())
      m_outlet = qobject_cast<Process::Outlet*>(ports[i]);
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
