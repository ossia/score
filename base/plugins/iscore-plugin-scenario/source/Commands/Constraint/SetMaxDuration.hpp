#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include "Document/Constraint/ConstraintModel.hpp"


class ConstraintModel;
namespace Scenario
{
namespace Command
{
/**
 * @brief The SetMaxDuration class
 *
 * Sets the Max duration of a Constraint
*/
class SetMaxDuration : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("ScenarioControl", "SetMaxDuration", "Set constraint maximum")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SetMaxDuration)

        SetMaxDuration(Path<ConstraintModel>&& path, const TimeValue& newval):
            iscore::SerializableCommand{
                factoryName(), commandName(), description()},
        m_path{std::move(path)},
        m_oldVal{m_path.find().duration.maxDuration()},
        m_newVal{newval}
        {

        }

        void update(const Path<ConstraintModel>&, const TimeValue &newval)
        {
            m_newVal = newval;
        }

        virtual void undo() override
        {
            m_path.find().duration.setMaxDuration(m_oldVal);
        }

        virtual void redo() override
        {
            m_path.find().duration.setMaxDuration(m_newVal);
        }

    protected:
        virtual void serializeImpl(QDataStream& s) const override
        {
            s << m_path << m_oldVal << m_newVal;
        }
        virtual void deserializeImpl(QDataStream& s) override
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
