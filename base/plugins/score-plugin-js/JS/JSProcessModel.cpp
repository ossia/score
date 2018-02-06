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
  FloatSlider { id: sl; min: 10; max: 100; }

  function onTick(oldtime, time, position, offset) {
    out1.value = in1.value + sl.value * Math.random();
  }
}
)_");
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::setScript(const QString& script)
{

  delete m_dummyObject;
  m_dummyObject = nullptr;
  m_dummyComponent.reset();
  m_dummyComponent = std::make_unique<QQmlComponent>(&m_dummyEngine);

  m_script = script;

  if(script.trimmed().startsWith("import"))
  {
    m_dummyComponent->setData(script.trimmed().toUtf8(), QUrl());
    const auto& errs = m_dummyComponent->errors();
    if(!errs.empty())
    {
      const auto& err = errs.first();
      qDebug() << err.line() << err.toString();
      emit scriptError(err.line(), err.toString());
    }
    else
    {
      std::vector<State::AddressAccessor> oldInletAddresses, oldOutletAddresses;
      std::vector<std::vector<Path<Process::Cable>>> oldInletCable, oldOutletCable;
      for(Process::Inlet* in : m_inlets)
      {
        oldInletAddresses.push_back(in->address());
        oldInletCable.push_back(in->cables());
      }
      for(Process::Outlet* in : m_outlets)
      {
        oldOutletAddresses.push_back(in->address());
        oldOutletCable.push_back(in->cables());
      }

      qDeleteAll(m_inlets);
      m_inlets.clear();
      qDeleteAll(m_outlets);
      m_outlets.clear();

      m_dummyObject = m_dummyComponent->create();

      {
        auto cld_inlet = m_dummyObject->findChildren<Inlet*>();
        int i = 0;
        for(auto n : cld_inlet) {
          auto port = n->make(Id<Process::Port>(i++), this);
          port->setCustomData(n->objectName());
          m_inlets.push_back(port);
        }
      }

      {
        auto cld_outlet = m_dummyObject->findChildren<Outlet*>();
        int i = 0;
        for(auto n : cld_outlet) {
          auto port = n->make(Id<Process::Port>(i++), this);
          port->setCustomData(n->objectName());
          m_outlets.push_back(port);
        }
      }
      emit scriptOk();

      std::size_t i = 0;
      for(Process::Inlet* in : m_inlets)
      {
        if(i < oldInletAddresses.size())
        {
          in->setAddress(oldInletAddresses[i]);
          for(const auto& cbl : oldInletCable[i])
            in->addCable(cbl);
        }
        i++;
      }
      i = 0;
      for(Process::Outlet* in : m_outlets)
      {
        if(i < oldOutletAddresses.size())
        {
          in->setAddress(oldOutletAddresses[i]);
          for(const auto& cbl : oldOutletCable[i])
            in->addCable(cbl);
        }
      }
      emit scriptChanged(script);
      emit inletsChanged();
      emit outletsChanged();

    }
  }

}
}
