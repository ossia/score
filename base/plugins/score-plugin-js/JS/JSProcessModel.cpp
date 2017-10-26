// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>
#include <core/document/Document.hpp>
#include <QQmlEngine>
#include <QQmlComponent>
#include <vector>
#include <JS/Qml/QmlObjects.hpp>
#include <Process/Dataflow/Port.hpp>

#include "JS/JSProcessMetadata.hpp"
#include "JSProcessModel.hpp"
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <score/model/Identifier.hpp>

namespace JS
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  setScript(
R"_(import QtQuick 2.0
import Score 1.0
Item {
  ValueInlet { id: in1 }
  ValueOutlet { id: out1 }

  function onTick(oldtime, time, position, offset) {
    out1.value = in1.value + 10 * Math.random();
  }
}
)_");
  metadata().setInstanceName(*this);
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{source, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  setScript(source.m_script);
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::setScript(const QString& script)
{
  qDeleteAll(m_inlets);
  m_inlets.clear();
  qDeleteAll(m_outlets);
  m_outlets.clear();
  QQmlEngine test_engine;
  m_script = script;

  if(script.trimmed().startsWith("import"))
  {
    QQmlComponent c{&test_engine};
    c.setData(script.trimmed().toUtf8(), QUrl());
    const auto& errs = c.errors();
    if(!errs.empty())
    {
      const auto& err = errs.first();
      qDebug() << err.line() << err.toString();
      emit scriptError(err.line(), err.toString());
    }
    else
    {
      auto obj = c.create();
      auto cld_inlet = obj->findChildren<Inlet*>();
      auto cld_outlet = obj->findChildren<Outlet*>();

      int i = 0;
      for(auto n : cld_inlet) {
        auto port = new Process::Port{Id<Process::Port>(i++), this};
        if(qobject_cast<ValueInlet*>(n))
          port->type = Process::PortType::Message;
        else if(qobject_cast<AudioInlet*>(n))
          port->type = Process::PortType::Audio;
        port->outlet = false;
        port->setCustomData(n->objectName());
        m_inlets.push_back(port);
      }

      for(auto n : cld_outlet) {
        auto port = new Process::Port{Id<Process::Port>(i++), this};
        if(qobject_cast<ValueOutlet*>(n))
          port->type = Process::PortType::Message;
        else if(qobject_cast<AudioOutlet*>(n))
        {
          if(n == cld_outlet[0])
            port->setPropagate(true);
          port->type = Process::PortType::Audio;
        }
        port->outlet = true;
        port->setCustomData(n->objectName());
        m_outlets.push_back(port);
      }
      delete obj;

      emit scriptOk();
    }
  }

  emit scriptChanged(script);
  emit inletsChanged();
  emit outletsChanged();
}
}
