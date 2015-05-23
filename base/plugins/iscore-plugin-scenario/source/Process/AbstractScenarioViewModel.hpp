#pragma once
#include "ProcessInterface/ProcessViewModel.hpp"
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
class ScenarioModel;
class AbstractConstraintViewModel;
class ConstraintModel;
class TimeNodeModel;

class EventModel;

class AbstractScenarioViewModel : public ProcessViewModel
{
        Q_OBJECT
    public:
        using model_type = ScenarioModel;

        virtual void makeConstraintViewModel(
                const id_type<ConstraintModel>& constraintModelId,
                const id_type<AbstractConstraintViewModel>& constraintViewModelId) = 0;

        void removeConstraintViewModel(
                const id_type<AbstractConstraintViewModel>& constraintViewModelId);

        // Access to elements
        // A given constraint can be represented only once in a given scenario VM, hence...
        AbstractConstraintViewModel& constraint(
                const id_type<ConstraintModel>& constraintModelid) const;
        AbstractConstraintViewModel& constraint(
                const id_type<AbstractConstraintViewModel>& constraintViewModelid) const;
        QVector<AbstractConstraintViewModel*> constraints() const;

    signals:
        void constraintViewModelCreated(const id_type<AbstractConstraintViewModel>& constraintViewModelid);
        void constraintViewModelRemoved(const id_type<AbstractConstraintViewModel>& constraintViewModelid);

        void eventCreated(const id_type<EventModel>& eventId);
        void eventDeleted(const id_type<EventModel>& eventId);
        void timeNodeDeleted(const id_type<TimeNodeModel>& timeNodeId);
        void eventMoved(const id_type<EventModel>& eventId);
        void timeNodeCreated(const id_type<TimeNodeModel>& timeNodeId);
        void constraintMoved(const id_type<ConstraintModel>& constraintId);

    public slots:
        virtual void on_constraintRemoved(const id_type<ConstraintModel>& constraintId) = 0;

    protected:
        AbstractScenarioViewModel(const id_type<ProcessViewModel>& viewModelId,
                                  const QString& name,
                                  ProcessModel& sharedProcess,
                                  QObject* parent) :
            ProcessViewModel {viewModelId,
                                      name,
                                      sharedProcess,
                                      parent
        }
        {
        }

        // Copy
        AbstractScenarioViewModel(const AbstractScenarioViewModel& source,
                                  const id_type<ProcessViewModel>& viewModelId,
                                  const QString& name,
                                  ProcessModel& sharedProcess,
                                  QObject* parent) :
            ProcessViewModel {viewModelId,
                                      name,
                                      sharedProcess,
                                      parent
        }
        {
        }

        // Load
        template<typename Impl>
        AbstractScenarioViewModel(Deserializer<Impl>& vis,
                                  ProcessModel& sharedProcess,
                                  QObject* parent) :
            ProcessViewModel {vis,
                                       sharedProcess,
                                       parent
        }
        {
            // No data to save (the constraints vector will be rebuilt by the subclass accordingly.)
        }

        QVector<AbstractConstraintViewModel*> m_constraints;
};

template<typename T>
typename T::constraint_view_model_type& constraintViewModel(
        const T& scenarioViewModel,
        const id_type<AbstractConstraintViewModel>& cvm_id)
{
    return static_cast<typename T::constraint_view_model_type&>(scenarioViewModel.constraint(cvm_id));
}


template<typename T>
QVector<typename T::constraint_view_model_type*> constraintsViewModels(const T& scenarioViewModel)
{
    QVector<typename T::constraint_view_model_type*> v;

    for(auto& elt : scenarioViewModel.constraints())
    {
        v.push_back(static_cast<typename T::constraint_view_model_type*>(elt));
    }

    return v;
}

#include "Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp"
void createConstraintViewModels(const ConstraintViewModelIdMap& idMap,
                                const id_type<ConstraintModel>& constraint,
                                const ScenarioModel& scenario);
