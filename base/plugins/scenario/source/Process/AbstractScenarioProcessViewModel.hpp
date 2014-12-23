#pragma once
#include "ProcessInterface/ProcessViewModelInterface.hpp"

class ConstraintViewModelInterface;
class AbstractScenarioProcessViewModel : public ProcessViewModelInterface
{
		Q_OBJECT
	public:
		using ProcessViewModelInterface::ProcessViewModelInterface;

		virtual void createConstraintViewModel(int constraintModelId, int constraintViewModelId) = 0;
		void removeConstraintViewModel(int constraintViewModelId);

		// Access to elements
		ConstraintViewModelInterface* constraint(int constraintViewModelid) const;
		QVector<ConstraintViewModelInterface*> constraints() const;

	signals:
		void constraintViewModelCreated(int constraintViewModelid);
		void constraintViewModelRemoved(int constraintViewModelid);

		// TODO transform in order to refer to view models instead
		void eventCreated(int eventId);
		void eventDeleted(int eventId);
		void eventMoved(int eventId);
		void constraintMoved(int constraintId);

	public slots:
		virtual void on_constraintRemoved(int constraintId) = 0;

	protected:
		QVector<ConstraintViewModelInterface*> m_constraints;
};


// TODO put this in a pattern (and also do for constraintvminterface, event, etc...)
template<typename T>
QVector<typename T::constraint_view_model_type*> constraintsViewModels(T* scenarioViewModel)
{
	QVector<typename T::constraint_view_model_type*> v;
	for(auto& elt : scenarioViewModel->constraints())
		v.push_back(static_cast<typename T::constraint_view_model_type*>(elt));
	return std::move(v);
}

template<typename T>
typename T::constraint_view_model_type* constraintViewModel(T* scenarioViewModel, int cvm_id)
{
	return static_cast<typename T::constraint_view_model_type*>(scenarioViewModel->constraint(cvm_id));
}
