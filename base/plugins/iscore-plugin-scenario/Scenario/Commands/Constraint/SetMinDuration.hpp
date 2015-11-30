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

        SetMinDuration(Path<ConstraintModel>&& path, const TimeValue& newval, bool isMinNull):
        m_path{std::move(path)},
        m_oldVal{m_path.find().duration.minDuration()},
        m_newVal{newval},
        m_oldMinNull{m_path.find().duration.isMinNul()},
        m_newMinNull{isMinNull}
        {
        }

        void update(const Path<ConstraintModel>&, const TimeValue &newval, bool isMinNull)
        {
            m_newVal = newval;
        }

        void undo() const override
        {
            m_path.find().duration.setMinNull(m_oldMinNull);
            m_path.find().duration.setMinDuration(m_oldVal);
        }

        void redo() const override
        {
            m_path.find().duration.setMinNull(m_newMinNull);
            m_path.find().duration.setMinDuration(m_newVal);
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_path << m_oldVal << m_newVal << m_newMinNull;
        }
        void deserializeImpl(DataStreamOutput& s) override
        {
            s >> m_path >> m_oldVal >> m_newVal >> m_oldMinNull;
        }

    private:
        Path<ConstraintModel> m_path;

        TimeValue m_oldVal;
        TimeValue m_newVal;
        bool m_oldMinNull, m_newMinNull;
};

}
}
