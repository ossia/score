#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"


template<>
void Visitor<Reader<DataStream>>::visit(const AbstractConstraintViewModel& cvm)
{
	//Ajouter l'id de la contrainte pour pouvoir le retrouver
	m_stream << cvm.model()->id();

	// We happily do not require a way to save the derived type, since it is known
	// at compile time and calls this function.
	visit(static_cast<const IdentifiedObject&>(cvm));

	// Save the AbstractConstraintViewModelData
	m_stream << cvm.isBoxShown();
	m_stream << cvm.shownBox();
}

template<>
void Visitor<Writer<DataStream>>::visit(AbstractConstraintViewModel& cvm)
{
	bool shown;
	int shown_id;

	m_stream >> shown;
	m_stream >> shown_id;

	int __warn;
	// TODO
	// cvm.m_boxIsShown = shown;
	// cvm.m_idOfShownBox = shown_id;
}

QDataStream& operator <<(QDataStream& s,
						 const AbstractConstraintViewModel& p)
{
	/*
	// It will be deserialized by the constructor.
	s << static_cast<const IdentifiedObject&>(p);
	s << p.m_boxIsShown
	  << p.m_idOfShownBox;

	p.serialize(s);
	return s;
	*/
}

QDataStream& operator >>(QDataStream& s,
						 AbstractConstraintViewModel& p)
{
	/* TODO
	s >> p.m_boxIsShown
	  >> p.m_idOfShownBox;
	return s;

	*/
}
