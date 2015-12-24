#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Scenario/Commands/Constraint/SetRigidity.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <iscore/document/DocumentInterface.hpp>



#include <State/Expression.hpp>

class TimeNodeModel;

namespace Scenario
{
namespace Command
{
template<typename Scenario_T>
class AddTrigger final : public iscore::SerializableCommand
{
    public:
        const CommandParentFactoryKey& parentKey() const override
        { return CommandFactoryName<Scenario_T>(); }
        const CommandFactoryKey& key() const override
        { return static_key(); }
        QString description() const override
        { return QObject::tr("Add a trigger"); }
        static const CommandFactoryKey& static_key()
        {
            static const CommandFactoryKey kagi{"AddTrigger_"_CS + Scenario_T::className};
            return kagi;
        }

        AddTrigger() = default;

        AddTrigger(Path<TimeNodeModel>&& timeNodePath):
            m_path{std::move(timeNodePath)}
        {

        }

        void undo() const override
        {
            auto& tn = m_path.find();
            tn.trigger()->setActive(false);

            for (const auto& cmd : m_cmds)
            {
                cmd.undo();
            }

            m_cmds.clear();
        }

        void redo() const override
        {
            auto& tn = m_path.find();
            tn.trigger()->setActive(true);

            Scenario_T* scenar = safe_cast<Scenario_T*>(tn.parent());

            for (const auto& cstrId : constraintsBeforeTimeNode(*scenar, tn.id()))
            {
                m_cmds.emplace_back(scenar->constraint(cstrId), false);
                m_cmds.back().redo();
            }
        }

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_path;
            s << (int32_t) m_cmds.size();

            for(const auto& cmd : m_cmds)
            {
                s << cmd.serialize();
            }
        }

        void deserializeImpl(DataStreamOutput& s) override
        {
            int32_t n;
            s >> m_path;
            s >> n;
            m_cmds.resize(n);
            for(int i = 0; i < n; i++)
            {
                QByteArray a;
                s >> a;
                m_cmds[i].deserialize(a);
            }
        }

    private:
        Path<TimeNodeModel> m_path;
        mutable std::vector<SetRigidity> m_cmds; // TODO investigate mutable

};

}
}

#include <Scenario/Process/ScenarioModel.hpp>
ISCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<Scenario::ScenarioModel>)

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
ISCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<BaseScenario>)
