// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>
#include <core/document/Document.hpp>
#include <QQmlEngine>
#include <QQmlComponent>
#include <vector>
#include <JS/Qml/QmlObjects.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>

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
  m_script =
R"_(import QtQuick 2.0
Item {
  ValueInlet { id: in1 }
  ValueOutlet { id: out1 }

  function onTick(time, position, offset) {
    out1.val = in1.val + Math.random();
  }
}
)_";
  metadata().setInstanceName(*this);
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{source, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , m_script{source.m_script}
{
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
      auto cld_inlet = obj->findChildren<ValueInlet*>();
      auto cld_outlet = obj->findChildren<ValueOutlet*>();

      int i = 0;
      for(auto n : cld_inlet) {
        auto port = new Process::Port{Id<Process::Port>(i++), this};
        port->type = Process::PortType::Message;
        port->outlet = false;
        port->setCustomData(n->objectName());
        m_inlets.push_back(port);
      }

      for(auto n : cld_outlet) {
        auto port = new Process::Port{Id<Process::Port>(i++), this};
        port->type = Process::PortType::Message;
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
