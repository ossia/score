#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"


class ConstraintModel;

/**
 * @brief The TemporalConstraintViewModel class
 *
 * The ViewModel of a Constraint shown inside a temporal view of a scenario
 */
class TemporalConstraintViewModel : public AbstractConstraintViewModel
{
		Q_OBJECT

	public:

		/**
		 * @brief TemporalConstraintViewModel
		 * @param id identifier
		 * @param model Pointer to the corresponding model object
		 * @param parent Parent object (most certainly ScenarioProcessViewModel)
		 */
		TemporalConstraintViewModel(id_type<AbstractConstraintViewModel> id,
									ConstraintModel* model,
									QObject* parent);

		template<typename DeserializerVisitor>
		TemporalConstraintViewModel(DeserializerVisitor&& vis,
									ConstraintModel* model,
									QObject* parent):
			AbstractConstraintViewModel{vis, model, parent}
		{
			// Nothing to add, no vis.visit(*this);
		}
};
