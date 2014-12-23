#pragma once
#include "Document/Constraint/ConstraintViewModelInterface.hpp"


class ConstraintModel;
// TODO might be different in temporal vs logical view. Same for Event.
class TemporalConstraintViewModel : public ConstraintViewModelInterface
{
		Q_OBJECT
		friend class ConstraintModel;
	public:


	protected:
		virtual void serialize(QDataStream& s) const
		{
			s << m_boxIsPresent
			  << m_idOfDisplayedBox;
		}
		virtual void deserialize(QDataStream& s)
		{
			s >> m_boxIsPresent
			  >> m_idOfDisplayedBox;
		}

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


		bool m_boxIsPresent{};
		int m_idOfDisplayedBox{};

};
