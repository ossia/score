
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>
#include <core/document/Document.hpp>
#include <QQmlEngine>
#include <QQmlComponent>
#include <vector>

#include "JS/JSProcessMetadata.hpp"
#include "JSProcessModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process
{
class LayerModel;
}
namespace Process
{
class ProcessModel;
}

class QObject;
#include <iscore/model/Identifier.hpp>

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
        "iscore.value('OSCdevice:/millumin/layer/y/instance'); \n"
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
  static QQmlEngine test_engine;
  m_script = script;

  if(script.startsWith("import"))
  {
    QQmlComponent c{&test_engine};
    c.setData(script.toUtf8(), QUrl());
    const auto& errs = c.errors();
    if(!errs.empty())
    {
      const auto& err = errs.first();
      emit scriptError(err.line(), err.toString());
    }
    else
    {
      m_properties.clear();
      auto obj = c.create();
      const auto mobj = obj->metaObject();
      int i = mobj->propertyOffset();
      const int N = mobj->propertyCount();
      m_properties.reserve(N - i);
      for(; i < N; i++)
      {
        auto prop = obj->metaObject()->property(i);
        m_properties.emplace_back(prop.name(), prop.read(obj));
      }

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
}
}
