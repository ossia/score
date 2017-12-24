#pragma once
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>
namespace Media::VST
{

class SCORE_PLUGIN_MEDIA_EXPORT SetVSTControl final : public score::Command
{
    SCORE_COMMAND_DECL(
        CommandFactoryName(), SetVSTControl, "Set a control")

public:
    SetVSTControl(const VSTControlInlet& obj, float newval)
        : m_path{obj}
        , m_old{obj.value()}
        , m_new{newval}
    {
    }

    virtual ~SetVSTControl() { }

    void undo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).setValue(m_old);
    }

    void redo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).setValue(m_new);
    }

    void update(const VSTControlInlet& obj, float newval)
    {
      m_new = newval;
    }

protected:
    void serializeImpl(DataStreamInput& stream) const final override
    {
      stream << m_path << m_old << m_new;
    }
    void deserializeImpl(DataStreamOutput& stream) final override
    {
      stream >> m_path >> m_old >> m_new;
    }

  private:
    Path<VSTControlInlet> m_path;
    float m_old, m_new;
};

class SCORE_PLUGIN_MEDIA_EXPORT CreateVSTControl final : public score::Command
{
    SCORE_COMMAND_DECL(
        CommandFactoryName(), CreateVSTControl, "Create a control")

public:
    CreateVSTControl(const VSTEffectModel& obj, int fxNum, float value)
        : m_path{obj}
        , m_fxNum{fxNum}
        , m_val{value}
    {
    }

    virtual ~CreateVSTControl() { }

    void undo(const score::DocumentContext& ctx) const final override
    {
      VSTEffectModel& obj = m_path.find(ctx);
      auto it = obj.controls.find(m_fxNum);
      SCORE_ASSERT(it != obj.controls.end());
      auto ctrl = it->second;
      obj.controls.erase(it);
      for(auto it = obj.m_inlets.begin(); it != obj.m_inlets.end(); ++it)
      {
        if(*it == ctrl)
        {
          obj.m_inlets.erase(it);
          break;
        }
      }
      emit obj.controlRemoved(ctrl->id());
      delete ctrl;
    }

    void redo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).on_addControl(m_fxNum, m_val);
    }

protected:
    void serializeImpl(DataStreamInput& stream) const final override
    {
      stream << m_path << m_fxNum << m_val;
    }
    void deserializeImpl(DataStreamOutput& stream) final override
    {
      stream >> m_path >> m_fxNum >> m_val;
    }

  private:
    Path<VSTEffectModel> m_path;
    int m_fxNum{};
    float m_val{};

};

}
