#pragma once
#include <iscore/tools/ModelPath.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <State/Address.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
/** Note : this command is for internal use only, in recording **/

class AutomationModel;
class InitAutomation : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL("AutomationControl", InitAutomation, "InitAutomation")
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
        void serializeImpl(QDataStream &) const override;
        void deserializeImpl(QDataStream &) override;

    private:
        Path<AutomationModel> m_path;
        iscore::Address m_addr;
        double m_newMin;
        double m_newMax;
        std::vector<CurveSegmentData> m_segments;
};
