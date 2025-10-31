#include "PortSource.hpp"

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

W_OBJECT_IMPL(JS::PortSource)
namespace JS
{

PortSource::PortSource(QObject* parent)
    : QObject{parent}
{
  connect(this, &PortSource::processChanged, this, [this] { rebuild(); });
  connect(this, &PortSource::portChanged, this, [this] { rebuild(); });
}

PortSource::~PortSource() { }

void PortSource::setTarget(const QQmlProperty& prop)
{
  m_targetProperty = prop;
  if(m_targetProperty.hasNotifySignal())
  {
    m_targetProperty.connectNotifySignal(this, SLOT(on_newUIValue()));
  }
}

void PortSource::on_newUIValue()
{
  // Avoid signal loops
  if(m_writingValue)
    return;
  if(m_inlet)
    m_inlet->setValue(ossia::qt::qt_to_ossia{}(m_targetProperty.read()));
}

Process::ProcessModel* PortSource::processInstance() const noexcept
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

void PortSource::rebuild()
{
  m_inlet = nullptr;

  if(!parent())
    return;

  if(m_port.typeId() == QMetaType::Type::QString)
  {
    auto process = processInstance();
    if(!process)
      return;

    auto port_name = m_port.value<QString>();
    m_inlet = process->findChild<Process::ControlInlet*>(port_name);
    if(!m_inlet)
    {
      auto& inls = process->inlets();
      for(auto& inl : inls)
      {
        if(inl->name() == port_name)
        {
          m_inlet = qobject_cast<Process::ControlInlet*>(inl);
          break;
        }
      }
    }
  }
  else if(m_port.typeId() == QMetaType::Type::Int)
  {
    auto process = processInstance();
    if(!process)
      return;

    int i = m_port.toInt();
    auto& inls = process->inlets();
    if(i >= 0 && i < inls.size())
      m_inlet = qobject_cast<Process::ControlInlet*>(inls[i]);
  }
  else
  {
    m_inlet = qobject_cast<Process::ControlInlet*>(m_port.value<QObject*>());
  }

  if(!m_inlet)
    return;

  connect(
      m_inlet, &Process::ControlInlet::executionValueChanged, this,
      [this](const ossia::value& v) {
    auto vv = v.apply(ossia::qt::ossia_to_qvariant{});
    m_writingValue = true;
    m_targetProperty.write(vv);
    m_writingValue = false;
  });
}
}
