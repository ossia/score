#pragma once
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include <tools/SettableIdentifierAlternative.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>
class ScenarioProcessSharedModel;
class AbstractConstraintViewModel;
class ConstraintModel;
class TimeNodeModel;

class EventModel;

class AbstractScenarioProcessViewModel : public ProcessViewModelInterface
{
		Q_OBJECT
	public:
		using model_type = ScenarioProcessSharedModel;

		virtual void makeConstraintViewModel(id_type<ConstraintModel> constraintModelId,
											 id_type<AbstractConstraintViewModel> constraintViewModelId) = 0;

		void removeConstraintViewModel(id_type<AbstractConstraintViewModel> constraintViewModelId);

		// Access to elements
		AbstractConstraintViewModel* constraint(id_type<AbstractConstraintViewModel> constraintViewModelid) const;
		QVector<AbstractConstraintViewModel*> constraints() const;

	signals:
		void constraintViewModelCreated(id_type<AbstractConstraintViewModel> constraintViewModelid);
		void constraintViewModelRemoved(id_type<AbstractConstraintViewModel> constraintViewModelid);

		// TODO transform in order to refer to view models instead
		void eventCreated(id_type<EventModel> eventId);
		void eventDeleted(id_type<EventModel> eventId);
		void eventMoved(id_type<EventModel> eventId);
		void timeNodeCreated(id_type<TimeNodeModel> timeNodeId);
		void constraintMoved(id_type<ConstraintModel> constraintId);

	public slots:
		virtual void on_constraintRemoved(id_type<ConstraintModel> constraintId) = 0;

	protected:
		AbstractScenarioProcessViewModel(id_type<ProcessViewModelInterface> viewModelId,
										 QString name,
										 ProcessSharedModelInterface* sharedProcess,
										 QObject* parent):
			ProcessViewModelInterface{viewModelId,
									  name,
									  sharedProcess,
									  parent}
		{
		}

		template<typename Impl>
		AbstractScenarioProcessViewModel(Deserializer<Impl>& vis,
										 ProcessSharedModelInterface* sharedProcess,
										 QObject* parent):
			ProcessViewModelInterface{vis,
									  sharedProcess,
									  parent}
		{
			// No data to save (the constraints vector will be rebuilt by the subclass accordingly.
		}

		QVector<AbstractConstraintViewModel*> m_constraints;
};


// TODO put this in a pattern (and also do for constraintvminterface, event, etc...)
template<typename T>
QVector<typename T::constraint_view_model_type*> constraintsViewModels(const T* scenarioViewModel)
{
	QVector<typename T::constraint_view_model_type*> v;
	for(auto& elt : scenarioViewModel->constraints())
		v.push_back(static_cast<typename T::constraint_view_model_type*>(elt));
	return v;
}

template<typename T>
typename T::constraint_view_model_type* constraintViewModel(const T* scenarioViewModel, id_type<AbstractConstraintViewModel> cvm_id)
{
	return static_cast<typename T::constraint_view_model_type*>(scenarioViewModel->constraint(cvm_id));
}



template<typename T>
QVector<typename T::constraint_view_model_type*> constraintsViewModels(const T& scenarioViewModel)
{
	QVector<typename T::constraint_view_model_type*> v;
	for(auto& elt : scenarioViewModel.constraints())
		v.push_back(static_cast<typename T::constraint_view_model_type*>(elt));
	return v;
}
