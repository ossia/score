#pragma once
#include <Vst3/EffectModel.hpp>
#include <Vst3/Control.hpp>


#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <QDebug>

namespace vst3
{
class Model;
class Handler : public Steinberg::Vst::IComponentHandler
{
  vst3::Model& m_model;
public:
  Handler(vst3::Model& fx)
    : m_model{fx}
  {

  }

  ~Handler()
  {
    qDebug() << "~Handler()";
  }

  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    return {};
  }
  Steinberg::uint32 addRef() override
  {
    return 1;
  }
  Steinberg::uint32 release() override
  {
    return 1;
  }

  // IComponentHandler interface
public:
  Steinberg::tresult beginEdit(Steinberg::Vst::ParamID id) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult performEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) override
  {
    if(auto ctrl = m_model.controls.find(id); ctrl != m_model.controls.end())
    {
      ctrl->second->setValue(valueNormalized);
    }
    return Steinberg::kResultOk;
  }
  Steinberg::tresult endEdit(Steinberg::Vst::ParamID id) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult restartComponent(Steinberg::int32 flags) override
  {
    return Steinberg::kResultOk;
  }
};
}
