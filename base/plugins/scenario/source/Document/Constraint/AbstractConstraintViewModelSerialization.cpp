#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AbstractConstraintViewModel& cvm)
{
	/* TODO
	// Add the constraint id since we need it for construction
	m_stream << cvm.model()->id();

	// We happily do not require a way to save the derived type, since it is known
	// at compile time and calls this function.
	readFrom(static_cast<const IdentifiedObjectAlternative<AbstractConstraintViewModel>&>(cvm));

	// Save the AbstractConstraintViewModelData
	m_stream << cvm.isBoxShown();
	m_stream << cvm.shownBox();
	*/
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AbstractConstraintViewModel& cvm)
{
	/* TODO
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
	*/
}


template<>
void Visitor<Reader<JSON>>::readFrom(const AbstractConstraintViewModel& cvm)
{

	/* TODO
	m_obj["ConstraintId"] = toJsonObject(cvm.model()->id());

	readFrom(static_cast<const IdentifiedObjectAlternative<AbstractConstraintViewModel>&>(cvm));

	m_obj["IsBoxShown"] = cvm.isBoxShown();
	m_obj["ShownBoxId"] = cvm.shownBox();
	*/
}

template<>
void Visitor<Writer<JSON>>::writeTo(AbstractConstraintViewModel& cvm)
{

	/* TODO
	if(m_obj["IsBoxShown"].toBool())
	{
		cvm.showBox(m_obj["ShownBoxId"].toInt());
	}
	else
	{
		cvm.hideBox();
	}
	*/
}
