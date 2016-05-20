#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <State/Address.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <vector>
#include <iscore_plugin_automation_export.h>
struct DataStreamInput;
struct DataStreamOutput;
/** Note : this command is for internal use only, in recording **/

namespace Automation
{
class ProcessModel;

class ISCORE_PLUGIN_AUTOMATION_EXPORT InitAutomation final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), InitAutomation, "InitAutomation")
    public:
           // Note : the segments shall be sorted from start to end.
        InitAutomation(
                Path<ProcessModel>&& path,
                const State::Address& newaddr,
                double newmin,
                double newmax,
                std::vector<Curve::SegmentData>&& segments);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<ProcessModel> m_path;
        State::Address m_addr;
        double m_newMin;
        double m_newMax;
        std::vector<Curve::SegmentData> m_segments;
};
}
