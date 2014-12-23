#pragma once
#include "Document/Constraint/ConstraintViewModelInterface.hpp"


class ConstraintModel;
// TODO might be different in temporal vs logical view. Same for Event.
class TemporalConstraintViewModel : public ConstraintViewModelInterface
{
		Q_OBJECT
	public:
		/**
		 * @brief TemporalConstraintViewModel
		 * @param id identifier
		 * @param model Pointer to the corresponding model object
		 * @param parent Parent object (most certainly ScenarioProcessViewModel)
		 */
		TemporalConstraintViewModel(int id,
									ConstraintModel* model,
									QObject* parent);


	private:
		bool m_boxIsPresent{};
		int m_idOfDisplayedBox{};
};
