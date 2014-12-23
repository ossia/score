#pragma once
#include <core/tools/IdentifiedObject.hpp>

class ConstraintModel;
class ConstraintViewModelInterface : public IdentifiedObject
{
		Q_OBJECT
	public:
		ConstraintViewModelInterface(int id,
									 QString name,
									 ConstraintModel* model,
									 QObject* parent):
			IdentifiedObject{id, name, parent},
			m_model{model}
		{

		}

		ConstraintModel* model() const
		{ return m_model; }

	signals:
		void boxCreated(int boxId);
		void boxRemoved(int boxId);

	private:
		// A view model cannot exist without a model hence we are safe with a pointer
		ConstraintModel* m_model{};
};
