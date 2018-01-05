#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score_plugin_media_export.h>

namespace Process
{
class Port;
class Inlet;
}
namespace Media::VST
{
class VSTEffectModel;
class VSTControlInlet;
class SCORE_PLUGIN_MEDIA_EXPORT SetVSTControl final : public score::Command
{
    SCORE_COMMAND_DECL(
        CommandFactoryName(), SetVSTControl, "Set a control")

public:
    SetVSTControl(const VSTControlInlet& obj, float newval);
    virtual ~SetVSTControl();

    void undo(const score::DocumentContext& ctx) const final override;
    void redo(const score::DocumentContext& ctx) const final override;
    void update(const VSTControlInlet& obj, float newval);

protected:
    void serializeImpl(DataStreamInput& stream) const final override;
    void deserializeImpl(DataStreamOutput& stream) final override;

  private:
    Path<VSTControlInlet> m_path;
    float m_old, m_new;
};

class SCORE_PLUGIN_MEDIA_EXPORT CreateVSTControl final : public score::Command
{
    SCORE_COMMAND_DECL(
        CommandFactoryName(), CreateVSTControl, "Create a control")

public:
    CreateVSTControl(const VSTEffectModel& obj, int fxNum, float value);
    virtual ~CreateVSTControl();
    void undo(const score::DocumentContext& ctx) const final override;
    void redo(const score::DocumentContext& ctx) const final override;

protected:
    void serializeImpl(DataStreamInput& stream) const final override;
    void deserializeImpl(DataStreamOutput& stream) final override;

  private:
    Path<VSTEffectModel> m_path;
    int m_fxNum{};
    float m_val{};
};

class SCORE_PLUGIN_MEDIA_EXPORT RemoveVSTControl final : public score::Command
{
    SCORE_COMMAND_DECL(
        CommandFactoryName(), RemoveVSTControl, "Remove a control")

public:
    RemoveVSTControl(const VSTEffectModel& obj, Id<Process::Port> id);
    virtual ~RemoveVSTControl();
    void undo(const score::DocumentContext& ctx) const final override;
    void redo(const score::DocumentContext& ctx) const final override;

protected:
    void serializeImpl(DataStreamInput& stream) const final override;
    void deserializeImpl(DataStreamOutput& stream) final override;

  private:
    Path<VSTEffectModel> m_path;
    Id<Process::Port> m_id;
    QByteArray m_control;
};

}
