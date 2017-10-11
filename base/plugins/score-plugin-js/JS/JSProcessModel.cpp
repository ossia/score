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
  m_script
      = "(function(t) { \n"
        "     var obj = new Object; \n"
        "     obj[\"address\"] = 'OSCdevice:/millumin/layer/x/instance'; \n"
        "     obj[\"value\"] = t + "
        "score.value('OSCdevice:/millumin/layer/y/instance'); \n"
        "     return [ obj ]; \n"
        "});";
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
        auto inl = new Process::Port{Id<Process::Port>(i++), this};
        inl->type = Process::PortType::Message;
        inl->outlet = false;
        m_inlets.push_back(inl);
      }

      for(auto n : cld_outlet) {
        auto inl = new Process::Port{Id<Process::Port>(i++), this};
        inl->type = Process::PortType::Message;
        inl->outlet = true;
        m_outlets.push_back(inl);
      }
      delete obj;

      emit scriptOk();
    }
  }
  else
  {
    auto f = test_engine.evaluate(script);
    if(f.isError())
    {
      emit scriptError(f.property("lineNumber").toInt(), f.toString());
    }
    else
    {
      emit scriptOk();
    }
  }

  emit scriptChanged(script);
  emit inletsChanged();
  emit outletsChanged();
}
}
