#pragma once
#include <Vst3/Commands.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/EffectModel.hpp>

#include <QDebug>

#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

namespace vst3
{
class Model;
class ComponentHandler
    : virtual public Steinberg::Vst::IComponentHandler
    , virtual public Steinberg::Vst::IComponentHandler2
{
  vst3::Model& m_model;

public:
  ComponentHandler(vst3::Model& fx)
      : m_model{fx}
  {
  }

  ~ComponentHandler() { }

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    using namespace Steinberg;
    if(FUID::fromTUID(_iid) == Steinberg::Vst::IComponentHandler2::iid)
    {
      *obj = static_cast<Steinberg::Vst::IComponentHandler2*>(this);
      return kResultOk;
    }
    *obj = nullptr;
    return kResultFalse;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID id) override
  {
    // TODO implement ongoingdispatcher
    if(auto ctrl = m_model.controls.find(id); ctrl == m_model.controls.end())
    {
      ossia::qt::run_async(&m_model, [&proc = m_model, id] {
        if(auto ctrl = proc.controls.find(id); ctrl == proc.controls.end())
        {
          auto& ctx = score::IDocument::documentContext(proc);
          CommandDispatcher<>{ctx.commandStack}.submit<CreateControl>(proc, id);
        }
      });
    }
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API performEdit(
      Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) override
  {
    if(auto ctrl = m_model.controls.find(id); ctrl != m_model.controls.end())
    {
      ctrl->second->setValue(valueNormalized);
    }
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID id) override
  {
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override
  {
    return m_model.restartComponent(flags);
  }

  Steinberg::tresult PLUGIN_API setDirty(Steinberg::TBool state) override
  {
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API requestOpenEditor(Steinberg::FIDString name) override
  {
    Process::setupExternalUI(m_model, score::IDocument::documentContext(m_model), true);
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API startGroupEdit() override { return Steinberg::kResultOk; }

  Steinberg::tresult PLUGIN_API finishGroupEdit() override { return Steinberg::kResultOk; }
};
}
