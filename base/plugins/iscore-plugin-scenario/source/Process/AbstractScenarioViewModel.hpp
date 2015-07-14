#pragma once
#include "ProcessInterface/LayerModel.hpp"
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
class ScenarioModel;
class ConstraintViewModel;
class ConstraintModel;
class TimeNodeModel;
class StateModel;

class EventModel;

class AbstractScenarioLayer : public LayerModel
{
        Q_OBJECT
    public:
        using model_type = ScenarioModel;

        virtual void makeConstraintViewModel(
                const id_type<ConstraintModel>& constraintModelId,
                const id_type<ConstraintViewModel>& constraintViewModelId) = 0;

        void removeConstraintViewModel(
                const id_type<ConstraintViewModel>& constraintViewModelId);

        // Access to elements
        // A given constraint can be represented only once in a given scenario VM, hence...
        ConstraintViewModel& constraint(
                const id_type<ConstraintModel>& constraintModelid) const;
        ConstraintViewModel& constraint(
                const id_type<ConstraintViewModel>& constraintViewModelid) const;
        QVector<ConstraintViewModel*> constraints() const;

    signals:
        void constraintViewModelCreated(const id_type<ConstraintViewModel>& constraintViewModelid);
        void constraintViewModelRemoved(const id_type<ConstraintViewModel>& constraintViewModelid);

        void stateCreated(const id_type<StateModel>& eventId);
        void stateRemoved(const id_type<StateModel>& eventId);

        void eventCreated(const id_type<EventModel>& eventId);
        void eventRemoved(const id_type<EventModel>& eventId);

        void timeNodeCreated(const id_type<TimeNodeModel>& timeNodeId);
        void timeNodeRemoved(const id_type<TimeNodeModel>& timeNodeId);

        void eventMoved(const id_type<EventModel>& eventId);
        void constraintMoved(const id_type<ConstraintModel>& constraintId);

    public slots:
        virtual void on_constraintRemoved(const id_type<ConstraintModel>& constraintId) = 0;

    protected:
        AbstractScenarioLayer(const id_type<LayerModel>& viewModelId,
                              const QString& name,
                              ProcessModel& sharedProcess,
                              QObject* parent) :
            LayerModel {viewModelId,
                        name,
                        sharedProcess,
                        parent
}
        {
        }

        // Copy
        AbstractScenarioLayer(const AbstractScenarioLayer& source,
                              const id_type<LayerModel>& viewModelId,
                              const QString& name,
                              ProcessModel& sharedProcess,
                              QObject* parent) :
            LayerModel {viewModelId,
                        name,
                        sharedProcess,
                        parent}
        {
        }

        // Load
        template<typename Impl>
        AbstractScenarioLayer(Deserializer<Impl>& vis,
                              ProcessModel& sharedProcess,
                              QObject* parent) :
            LayerModel {vis,
                        sharedProcess,
                        parent}
        {
            // No data to save (the constraints vector will be rebuilt by the subclass accordingly.)
        }

        QVector<ConstraintViewModel*> m_constraints;
};

template<typename T>
typename T::constraint_layer_type& constraintViewModel(
        const T& scenarioViewModel,
        const id_type<ConstraintViewModel>& cvm_id)
{
    return static_cast<typename T::constraint_layer_type&>(scenarioViewModel.constraint(cvm_id));
}


template<typename T>
QVector<typename T::constraint_layer_type*> constraintsViewModels(const T& scenarioViewModel)
{
    QVector<typename T::constraint_layer_type*> v;

    for(auto& elt : scenarioViewModel.constraints())
    {
        v.push_back(static_cast<typename T::constraint_layer_type*>(elt));
    }

    return v;
}

#include "Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp"
void createConstraintViewModels(const ConstraintViewModelIdMap& idMap,
                                const id_type<ConstraintModel>& constraint,
                                const ScenarioModel& scenario);
