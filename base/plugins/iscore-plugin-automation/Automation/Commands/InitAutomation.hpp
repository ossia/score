#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <State/Address.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <vector>
#include <iscore_plugin_automation_export.h>
class DataStreamInput;
class DataStreamOutput;
/** Note : this command is for internal use only, in recording **/

class AutomationModel;

class ISCORE_PLUGIN_AUTOMATION_EXPORT InitAutomation final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(AutomationCommandFactoryName(), InitAutomation, "InitAutomation")
    public:
           // Note : the segments shall be sorted from start to end.
        InitAutomation(
                Path<AutomationModel>&& path,
                const iscore::Address& newaddr,
                double newmin,
                double newmax,
                std::vector<CurveSegmentData>&& segments);

    public:
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput &) const override;
        void deserializeImpl(DataStreamOutput &) override;

    private:
        Path<AutomationModel> m_path;
        iscore::Address m_addr;
        double m_newMin;
        double m_newMax;
        std::vector<CurveSegmentData> m_segments;
};
