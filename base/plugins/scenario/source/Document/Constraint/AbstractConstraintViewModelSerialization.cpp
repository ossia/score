#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AbstractConstraintViewModel& cvm)
{
	//Ajouter l'id de la contrainte pour pouvoir le retrouver
	m_stream << cvm.model()->id();

	// We happily do not require a way to save the derived type, since it is known
	// at compile time and calls this function.
	readFrom(static_cast<const IdentifiedObject&>(cvm));

	// Save the AbstractConstraintViewModelData
	m_stream << cvm.isBoxShown();
	m_stream << cvm.shownBox();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AbstractConstraintViewModel& cvm)
{
	bool shown;
	int shown_id;

	m_stream >> shown;
	m_stream >> shown_id;

	if(shown)
	{
		cvm.showBox(shown_id);
	}
	else
	{
		cvm.hideBox();
	}
}
