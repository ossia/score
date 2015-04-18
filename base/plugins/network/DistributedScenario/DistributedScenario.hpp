#pragma once
#include "Group.hpp"
#include <source/Process/ScenarioModel.hpp>
#include <unordered_map>
// The distribution algorithm must take into account the choices
// that are to be applied either to the whole group,
// or independently in each case.

class DistributedScenario : public ProcessSharedModelInterface
{
    public:
        DistributedScenario()
        {

        }

    private:
        std::unordered_map<id_type<EventModel>, id_type<Group>> m_eventMap;
        std::unordered_map<id_type<ConstraintModel>, id_type<Group>> m_eventMap;
        ScenarioModel* m_model{};

    public:
        ProcessSharedModelInterface*clone(id_type<ProcessSharedModelInterface> newId, QObject* newParent)
        {
            return m_model->clone(newId, newParent);
        }

        QString processName() const
        {
            return "DistributedScenario";
        }

        void setDurationAndScale(TimeValue newDuration)
        {
            m_model->setDurationAndScale(newDuration);
        }

        void setDurationAndGrow(TimeValue newDuration)
        {
            m_model->setDurationAndGrow(newDuration);
        }

        void setDurationAndShrink(TimeValue newDuration)
        {
            m_model->setDurationAndShrink(newDuration);
        }

        ProcessStateDataInterface*startState() const
        {
            return m_model->startState();
        }

        ProcessStateDataInterface*endState() const
        {
            return m_model->endState();
        }

        Selection selectableChildren() const
        {
            return m_model->selectableChildren();
        }

        Selection selectedChildren() const
        {
            return m_model->selectedChildren();
        }

        void setSelection(const Selection& s)
        {
            m_model->setSelection(s);
        }

        void serialize(SerializationIdentifier identifier, void* data) const
        {
            m_model->serialize(identifier, data);

            // TODO Serialize our own info.
        }
};
