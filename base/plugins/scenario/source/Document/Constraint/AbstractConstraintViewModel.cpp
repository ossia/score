#include "AbstractConstraintViewModel.hpp"

bool AbstractConstraintViewModel::isBoxShown() const
{
	return m_boxIsShown;
}

int AbstractConstraintViewModel::shownBox() const
{
	return m_idOfShownBox;
}

void AbstractConstraintViewModel::hideBox()
{
	m_boxIsShown = false;
	emit boxHidden();
}

void AbstractConstraintViewModel::showBox(int boxId)
{
	m_boxIsShown = true;
	m_idOfShownBox = boxId;

	emit boxShown(m_idOfShownBox);
}

AbstractConstraintViewModel::AbstractConstraintViewModel(id_type id,
														 QString name,
														 ConstraintModel* model,
														 QObject* parent):
	IdentifiedObjectAlternative<AbstractConstraintViewModel>{id, name, parent},
	m_model{model}
{
}
