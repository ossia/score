#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

class ConstraintModel;
/**
 * @brief The FullViewConstraintViewModel class
 *
 * The ViewModel of a Constraint shown in full view.
 * It should show a TimeBar.
 */
class FullViewConstraintViewModel : public AbstractConstraintViewModel
{
		Q_OBJECT

	public:

		/**
		 * @brief FullViewConstraintViewModel
		 * @param id identifier
		 * @param model Pointer to the corresponding model object
		 * @param parent Parent object (most certainly ScenarioProcessViewModel)
		 */
		FullViewConstraintViewModel(id_type<AbstractConstraintViewModel> id,
									ConstraintModel* model,
									QObject* parent);

		template<typename DeserializerVisitor>
		FullViewConstraintViewModel(DeserializerVisitor&& vis,
									ConstraintModel* model,
									QObject* parent):
			AbstractConstraintViewModel{vis, model, parent}
		{
			// Nothing to add, no vis.visit(*this);
		}
};
