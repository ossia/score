#pragma once
#include "Document/Constraint/AbstractConstraintViewModel.hpp"


class ConstraintModel;
// TODO might be different in temporal vs logical view. Same for Event.
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



	public slots:
		virtual void on_boxRemoved(id_type<BoxModel> boxId) override;

};
