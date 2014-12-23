#pragma once
#include "Document/Constraint/AbstractConstraintViewModel.hpp"


class ConstraintModel;
// TODO might be different in temporal vs logical view. Same for Event.
class TemporalConstraintViewModel : public AbstractConstraintViewModel
{
		Q_OBJECT
		friend class ConstraintModel;
		friend QDataStream& operator <<(QDataStream& s, const TemporalConstraintViewModel& vm);
		friend QDataStream& operator >>(QDataStream& s, TemporalConstraintViewModel& vm);

	protected:
		virtual void serialize(QDataStream& s) const override;

	private:
		// Can only be constructed from ConstraintModel::makeViewModel
		/**
		 * @brief TemporalConstraintViewModel
		 * @param id identifier
		 * @param model Pointer to the corresponding model object
		 * @param parent Parent object (most certainly ScenarioProcessViewModel)
		 */
		TemporalConstraintViewModel(int id,
									ConstraintModel* model,
									QObject* parent);
		TemporalConstraintViewModel(QDataStream& s,
									ConstraintModel* model,
									QObject* parent);
};
