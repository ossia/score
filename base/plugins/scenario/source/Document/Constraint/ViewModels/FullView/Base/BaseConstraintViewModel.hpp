#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

class ConstraintModel;
/**
 * @brief The BaseConstraintViewModel class
 *
 * The ViewModel of the Base Constraint. Provides infinity.
 */
class BaseConstraintViewModel : public AbstractConstraintViewModel
{
		Q_OBJECT

	public:

		/**
		 * @brief BaseConstraintViewModel
		 * @param id identifier
		 * @param model Pointer to the corresponding model object
		 * @param parent Parent object (most certainly ScenarioProcessViewModel)
		 */
		BaseConstraintViewModel(id_type<AbstractConstraintViewModel> id,
									ConstraintModel* model,
									QObject* parent);

		template<typename DeserializerVisitor>
		BaseConstraintViewModel(DeserializerVisitor&& vis,
									ConstraintModel* model,
									QObject* parent):
			AbstractConstraintViewModel{vis, model, parent}
		{
			// Nothing to add, no vis.visit(*this);
		}
};
