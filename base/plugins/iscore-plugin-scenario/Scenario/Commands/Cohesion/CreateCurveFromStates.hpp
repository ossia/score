#pragma once
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Address.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <utility>
#include <vector>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Process/ProcessList.hpp>
#include <Automation/AutomationProcessMetadata.hpp>

class DataStreamInput;
class DataStreamOutput;
namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
template <typename Object> class Path;

namespace Scenario
{
class ConstraintModel;
class SlotModel;
namespace Command
{
template<typename ProcessMetadata_T>
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateProcessAndLayers : public iscore::SerializableCommand
{
    public:
        CreateProcessAndLayers() = default;
        CreateProcessAndLayers(
                Path<ConstraintModel>&& constraint,
                const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>& slotList,
                const Id<Process::ProcessModel>& procId):
            m_addProcessCmd{
                std::move(constraint),
                procId,
                ProcessMetadata_T::factoryKey()}
        {
            auto proc = m_addProcessCmd.constraintPath().extend(ProcessMetadata_T::processObjectName(), procId);

            m_slotsCmd.reserve(slotList.size());

            auto fact = context.components.factory<Process::ProcessList>().list().get(ProcessMetadata_T::factoryKey());
            ISCORE_ASSERT(fact);
            auto procData = fact->makeStaticLayerConstructionData();

            for(const auto& elt : slotList)
            {
                m_slotsCmd.emplace_back(
                            Path<SlotModel>(elt.first),
                            elt.second,
                            Path<Process::ProcessModel>{proc},
                            procData);
            }
        }

        void undo() const final override
        {
            for(const auto& cmd : m_slotsCmd)
                cmd.undo();
            m_addProcessCmd.undo();
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_addProcessCmd.serialize();
            s << (int32_t)m_slotsCmd.size();
            for(const auto& elt : m_slotsCmd)
            {
                s << elt.serialize();
            }
        }

        void deserializeImpl(DataStreamOutput& s) override
        {
            QByteArray a;
            s >> a;
            m_addProcessCmd.deserialize(a);

            int32_t n = 0;
            s >> n;
            m_slotsCmd.resize(n);
            for(int i = 0; i < n; i++)
            {
                QByteArray b;
                s >> b;
                m_slotsCmd.at(i).deserialize(b);
            }
        }

        AddOnlyProcessToConstraint m_addProcessCmd;
        std::vector<Scenario::Command::AddLayerModelToSlot> m_slotsCmd;
};


class ISCORE_PLUGIN_SCENARIO_EXPORT CreateCurveFromStates final : public CreateProcessAndLayers<Automation::ProcessMetadata>
{
         ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateCurveFromStates, "CreateCurveFromStates")
    public:
        CreateCurveFromStates(
                Path<ConstraintModel>&& constraint,
                const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>>& slotList,
                const Id<Process::ProcessModel>& curveId,
                const State::Address &address,
                double start,
                double end,
                double min, double max);

        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        State::Address m_address;

        double m_start{}, m_end{};
        double m_min{}, m_max{};

};
}
}
