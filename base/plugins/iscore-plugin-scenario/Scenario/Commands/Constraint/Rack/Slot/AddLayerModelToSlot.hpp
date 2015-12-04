#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QByteArray>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
class DataStreamInput;
class DataStreamOutput;
class LayerModel;
class Process;
class SlotModel;

namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddLayerToSlot class
         *
         * Adds a process view to a slot.
         */
        class ISCORE_PLUGIN_SCENARIO_EXPORT AddLayerModelToSlot final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddLayerModelToSlot, "Add a layer to a slot")
            public:
                // Use this constructor when the process already exists
                AddLayerModelToSlot(
                    Path<SlotModel>&& slot,
                    Path<Process>&& process);

                // Use this constructor when the process isn't created yet
                AddLayerModelToSlot(
                    Path<SlotModel>&& slot,
                    Path<Process>&& process,
                    const QByteArray& processConstructionData);

                AddLayerModelToSlot(
                        Path<SlotModel>&& slot,
                        const Id<LayerModel>& layerid,
                        Path<Process>&& process,
                        const QByteArray& processConstructionData);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<SlotModel> m_slotPath;
                Path<Process> m_processPath;

                QByteArray m_processData;

                Id<LayerModel> m_createdLayerId {};
        };
    }
}
