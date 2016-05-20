#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Scenario
{
class ConstraintModel;
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
            static const constexpr auto corresponding_member = &ConstraintDurations::minDuration;

        SetMinDuration(const ConstraintModel& cst, TimeValue newval, bool isMinNull):
        m_path{cst},
        m_oldVal{cst.duration.minDuration()},
        m_newVal{newval},
        m_oldMinNull{cst.duration.isMinNul()},
        m_newMinNull{isMinNull}
        {
        }

        void update(const ConstraintModel& cst, TimeValue newval, bool isMinNull)
        {
            m_newVal = newval;
            auto& cstrDuration = cst.duration;
            if(m_newVal < TimeValue::zero())
                m_newVal = TimeValue::zero();
            if(m_newVal > cstrDuration.defaultDuration())
                m_newVal = cstrDuration.defaultDuration();
        }

        void undo() const override
        {
            auto& cstrDuration = m_path.find().duration;
            cstrDuration.setMinNull(m_oldMinNull);
            cstrDuration.setMinDuration(m_newVal);
        }

        void redo() const override
        {
            auto& cstrDuration = m_path.find().duration;
            cstrDuration.setMinNull(m_newMinNull);
            cstrDuration.setMinDuration(m_newVal);
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_path << m_oldVal << m_newVal << m_oldMinNull << m_newMinNull;
        }
        void deserializeImpl(DataStreamOutput& s) override
        {
            s >> m_path >> m_oldVal >> m_newVal >> m_oldMinNull >> m_newMinNull;
        }

    private:
        Path<ConstraintModel> m_path;

        TimeValue m_oldVal;
        TimeValue m_newVal;
        bool m_oldMinNull, m_newMinNull;
};

}
}
