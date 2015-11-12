#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

class ConstraintModel;
namespace Scenario
{
namespace Command
{
/**
 * @brief The SetMinDuration class
 *
 * Sets the Min duration of a Constraint
 */
class SetMinDuration final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetMinDuration, "Set constraint minimum")
    public:

        SetMinDuration(Path<ConstraintModel>&& path, const TimeValue& newval):
            iscore::SerializableCommand{
                factoryName(), commandName(), description()},
        m_path{std::move(path)},
        m_oldVal{m_path.find().duration.minDuration()},
        m_newVal{newval}
        {
        }

        void update(const Path<ConstraintModel>&, const TimeValue &newval)
        {
            m_newVal = newval;
        }

        void undo() const override
        {
            m_path.find().duration.setMinDuration(m_oldVal);
        }

        void redo() const override
        {
            m_path.find().duration.setMinDuration(m_newVal);
        }

    protected:
        void serializeImpl(QDataStream& s) const override
        {
            s << m_path << m_oldVal << m_newVal;
        }
        void deserializeImpl(QDataStream& s) override
        {
            s >> m_path >> m_oldVal >> m_newVal;
        }

    private:
        Path<ConstraintModel> m_path;

        TimeValue m_oldVal;
        TimeValue m_newVal;
};

}
}
