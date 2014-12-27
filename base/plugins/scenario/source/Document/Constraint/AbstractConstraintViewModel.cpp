#include "AbstractConstraintViewModel.hpp"

QDataStream& operator <<(QDataStream& s,
						 const AbstractConstraintViewModel& p)
{
	// It will be deserialized by the constructor.
	s << static_cast<const IdentifiedObject&>(p);
	s << p.m_boxIsShown
	  << p.m_idOfShownBox;

	p.serialize(s);
	return s;
}

QDataStream& operator >>(QDataStream& s,
						 AbstractConstraintViewModel& p)
{
	s >> p.m_boxIsShown
	  >> p.m_idOfShownBox;

	return s;
}

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

AbstractConstraintViewModel::AbstractConstraintViewModel(int id,
														 QString name,
														 ConstraintModel* model,
														 QObject* parent):
	IdentifiedObject{id, name, parent},
	m_model{model}
{
}

AbstractConstraintViewModel::AbstractConstraintViewModel(QDataStream& s,
														 ConstraintModel* model,
														 QObject* parent):
	IdentifiedObject{s, parent},
	m_model{model}
{
	s >> *this;
}
