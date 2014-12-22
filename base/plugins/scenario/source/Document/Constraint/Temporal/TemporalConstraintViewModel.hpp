#pragma once

#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
// TODO might be different in temporal vs logical view. Same for Event.
class TemporalConstraintViewModel : public IdentifiedObject
{
	public:
		/**
		 * @brief TemporalConstraintViewModel
		 * @param id identifier
		 * @param model Pointer to the corresponding model object
		 * @param parent Parent object (most certainly ScenarioProcessViewModel)
		 */
		TemporalConstraintViewModel(int id, ConstraintModel* model, QObject* parent);

	private:
		ConstraintModel* m_model{}; // A view model cannot exist without a model


		bool m_boxIsPresent{};
		int m_idOfDisplayedBox{};
};
