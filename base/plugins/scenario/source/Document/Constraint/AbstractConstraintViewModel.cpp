#include "AbstractConstraintViewModel.hpp"

bool AbstractConstraintViewModel::isBoxShown() const
{
	return m_shownBox.val().is_initialized();
}

::id_type<BoxModel> AbstractConstraintViewModel::shownBox() const
{
	return m_shownBox;
}

void AbstractConstraintViewModel::hideBox()
{
	m_shownBox.val().reset();
	emit boxHidden();
}

void AbstractConstraintViewModel::showBox(::id_type<BoxModel> boxId)
{
	m_shownBox = boxId;

	emit boxShown(m_shownBox);
}

AbstractConstraintViewModel::AbstractConstraintViewModel(id_type id,
														 QString name,
														 ConstraintModel* model,
														 QObject* parent):
	IdentifiedObjectAlternative<AbstractConstraintViewModel>{id, name, parent},
	m_model{model}
{
}
