#pragma once
#include "Document/Constraint/AbstractConstraintViewModel.hpp"


class ConstraintModel;
// TODO might be different in temporal vs logical view. Same for Event.
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
		TemporalConstraintViewModel(id_type id,
									ConstraintModel* model,
									QObject* parent);


		template<typename Impl>
		TemporalConstraintViewModel(Deserializer<Impl>& vis,
									ConstraintModel* model,
									QObject* parent):
			AbstractConstraintViewModel{vis, model, parent}
		{
			// Nothing to add, no vis.visit(*this);
		}


	public slots:
		virtual void on_boxRemoved(::id_type<BoxModel> boxId) override;

};
