#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "iscore/tools/SettableIdentifier.hpp"

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

/*
 * Command for vertical move so it does'nt have to resize anything on time axis
 * */

namespace Scenario
{
    namespace Command
    {
        class MoveConstraint final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveConstraint, "Move a constraint")
            public:
                MoveConstraint(
                        Path<Scenario::ScenarioModel>&& scenarioPath,
                    const Id<ConstraintModel>& id,
                    const TimeValue& date,
                    double y);

                void update(const Path<Scenario::ScenarioModel>&,
                            const Id<ConstraintModel>& ,
                            const TimeValue& date,
                            double height);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<Scenario::ScenarioModel> m_path;
                Id<ConstraintModel> m_constraint;
                double m_oldHeight{},
                       m_newHeight{};
        };
    }
}
