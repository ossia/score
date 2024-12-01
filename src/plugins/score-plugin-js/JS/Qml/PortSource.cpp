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
  // Avoir signal loops
  // if(m_writingValue)
  //   return;
  if(m_inlet)
  {
    m_inlet->setValue(ossia::qt::qt_to_ossia{}(m_targetProperty.read()));
  }
}

void PortSource::rebuild()
{
  if(m_inlet)
  {
    // TODO
  }

  m_inlet = nullptr;

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

  if(m_port.typeId() == QMetaType::Type::QString)
  {
    auto port_name = m_port.value<QString>();
    m_inlet = process->findChild<Process::ControlInlet*>(port_name);
    if(!m_inlet)
    {
      auto& inls = process->inlets();
      for(auto& inl : inls)
      {
        qDebug() << inl->name() << " == " << port_name;
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
    int i = m_port.toInt();
    auto& inls = process->inlets();
    if(i >= 0 && i < inls.size())
      m_inlet = qobject_cast<Process::ControlInlet*>(inls[i]);
  }

  if(!m_inlet)
    return;
}
}
