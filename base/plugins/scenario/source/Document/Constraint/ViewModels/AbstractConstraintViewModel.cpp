#include "AbstractConstraintViewModel.hpp"

bool AbstractConstraintViewModel::isBoxShown() const
{
	return bool(m_shownBox.val());
}

id_type<BoxModel> AbstractConstraintViewModel::shownBox() const
{
	return m_shownBox;
}

void AbstractConstraintViewModel::hideBox()
{
	m_shownBox.setVal(boost::optional<int32_t>{});
	emit boxHidden();
}

void AbstractConstraintViewModel::showBox(id_type<BoxModel> boxId)
{
	if(boxId.val().is_initialized())
	{
		m_shownBox = boxId;

		emit boxShown(m_shownBox);
	}
	else
	{
		hideBox();
	}
}

void AbstractConstraintViewModel::on_boxRemoved(id_type<BoxModel> boxId)
{
	if(shownBox() == boxId)
	{
		hideBox();
		emit boxRemoved();
	}
}


AbstractConstraintViewModel::AbstractConstraintViewModel(id_type<AbstractConstraintViewModel> id,
														 QString name,
														 ConstraintModel* model,
														 QObject* parent):
	IdentifiedObject<AbstractConstraintViewModel>{id, name, parent},
	m_model{model}
{
}
