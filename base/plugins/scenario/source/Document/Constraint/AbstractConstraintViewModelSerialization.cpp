#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AbstractConstraintViewModel& cvm)
{
	// Add the constraint id since we need it for construction
	m_stream << cvm.model()->id();

	// We happily do not require a way to save the derived type, since it is known
	// at compile time and calls this function.
	readFrom(static_cast<const IdentifiedObject<AbstractConstraintViewModel>&>(cvm));

	// Save the AbstractConstraintViewModelData
	m_stream << cvm.shownBox();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AbstractConstraintViewModel& cvm)
{
	id_type<BoxModel> id;
	m_stream >> id;

	if(id.val().is_initialized())
	{
		cvm.showBox(id);
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

	readFrom(static_cast<const IdentifiedObject<AbstractConstraintViewModel>&>(cvm));

	m_obj["ShownBox"] = toJsonObject(cvm.shownBox());
}

template<>
void Visitor<Writer<JSON>>::writeTo(AbstractConstraintViewModel& cvm)
{
	id_type<BoxModel> id;
	fromJsonObject(m_obj["ShownBox"].toObject(), id);

	if(id.val().is_initialized())
	{
		cvm.showBox(id);
	}
	else
	{
		cvm.hideBox();
	}
}
