#pragma once
#include "ProcessInterface/ProcessViewModelInterface.hpp"

class AbstractConstraintViewModel;
// TODO Serialize this.
class AbstractScenarioProcessViewModel : public ProcessViewModelInterface
{
		Q_OBJECT
	public:

		virtual void makeConstraintViewModel(int constraintModelId,
											 int constraintViewModelId) = 0;

		void removeConstraintViewModel(int constraintViewModelId);

		// Access to elements
		AbstractConstraintViewModel* constraint(int constraintViewModelid) const;
		QVector<AbstractConstraintViewModel*> constraints() const;

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
		AbstractScenarioProcessViewModel(int viewModelId,
										 QString name,
										 ProcessSharedModelInterface* sharedProcess,
										 QObject* parent):
			ProcessViewModelInterface{viewModelId,
									  name,
									  sharedProcess,
									  parent}
		{
		}

		AbstractScenarioProcessViewModel(QDataStream& s,
										 ProcessSharedModelInterface* sharedProcess,
										 QObject* parent):
			ProcessViewModelInterface{s,
									  sharedProcess,
									  parent}
		{
			// In derived classes's constructors, do s >> *this; (This one has nothing to save)
			// They have to reconstruct the m_constraints vector with the right view model classes.
		}

		virtual void makeConstraintViewModel(QDataStream& s) = 0;

		QVector<AbstractConstraintViewModel*> m_constraints;
};


// TODO put this in a pattern (and also do for constraintvminterface, event, etc...)
template<typename T>
QVector<typename T::constraint_view_model_type*> constraintsViewModels(const T* scenarioViewModel)
{
	QVector<typename T::constraint_view_model_type*> v;
	for(auto& elt : scenarioViewModel->constraints())
		v.push_back(static_cast<typename T::constraint_view_model_type*>(elt));
	return std::move(v);
}

template<typename T>
typename T::constraint_view_model_type* constraintViewModel(const T* scenarioViewModel, int cvm_id)
{
	return static_cast<typename T::constraint_view_model_type*>(scenarioViewModel->constraint(cvm_id));
}



template<typename T>
QVector<typename T::constraint_view_model_type*> constraintsViewModels(const T& scenarioViewModel)
{
	QVector<typename T::constraint_view_model_type*> v;
	for(auto& elt : scenarioViewModel.constraints())
		v.push_back(static_cast<typename T::constraint_view_model_type*>(elt));
	return std::move(v);
}
