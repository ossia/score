#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AbstractConstraintViewModel& cvm)
{
	// Add the constraint id since we need it for construction
	m_stream << cvm.model()->id();

	// We happily do not require a way to save the derived type, since it is known
	// at compile time and calls this function.
	// TODO readFrom(static_cast<const IdentifiedObject&>(cvm));

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


template<>
void Visitor<Reader<JSON>>::readFrom(const AbstractConstraintViewModel& cvm)
{
	m_obj["ConstraintId"] = toJsonObject(cvm.model()->id());

	// TODO readFrom(static_cast<const IdentifiedObject&>(cvm));

	m_obj["IsBoxShown"] = cvm.isBoxShown();
	m_obj["ShownBoxId"] = cvm.shownBox();
}

template<>
void Visitor<Writer<JSON>>::writeTo(AbstractConstraintViewModel& cvm)
{
	if(m_obj["IsBoxShown"].toBool())
	{
		cvm.showBox(m_obj["ShownBoxId"].toInt());
	}
	else
	{
		cvm.hideBox();
	}
}
