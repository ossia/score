#include "DeckModelSerialization.hpp"
#include "DeckModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "source/ProcessInterfaceSerialization/ProcessViewModelInterfaceSerialization.hpp"
#include <interface/serialization/JSONVisitor.hpp>

template<> void Visitor<Reader<DataStream>>::readFrom(const DeckModel& deck)
{
	// TODO readFrom(static_cast<const IdentifiedObject&>(deck));

	m_stream << deck.editedProcessViewModel();

	auto pvms = deck.processViewModels();
	m_stream << (int) pvms.size();
	for(auto pvm : pvms)
	{
		readFrom(*pvm);
	}

	m_stream << deck.height()
			 << deck.position();
}

template<> void Visitor<Writer<DataStream>>::writeTo(DeckModel& deck)
{
	id_type<ProcessViewModelInterface> editedProcessId;
	m_stream >> editedProcessId;

	int pvm_size;
	m_stream >> pvm_size;

	auto cstr = deck.parentConstraint();
	for(int i = 0; i < pvm_size; i++)
	{
		auto pvm = createProcessViewModel(*this, cstr, &deck);
		deck.addProcessViewModel(pvm);
	}

	int height;
	int position;
	m_stream >> height
			 >> position;
	deck.setHeight(height);
	deck.setPosition(position);

	deck.selectForEdition(editedProcessId);
}





template<> void Visitor<Reader<JSON>>::readFrom(const DeckModel& deck)
{
	// TODO  readFrom(static_cast<const IdentifiedObject&>(deck));

	// TODO m_obj["EditedProcess"] = deck.editedProcessViewModel();
	m_obj["Height"] = deck.height();
	m_obj["Position"] = deck.position();

	QJsonArray arr;
	for(auto pvm : deck.processViewModels())
	{
		arr.push_back(toJsonObject(*pvm));
	}

	m_obj["ProcessViewModels"] = arr;
}

template<> void Visitor<Writer<JSON>>::writeTo(DeckModel& deck)
{
	QJsonArray arr = m_obj["ProcessViewModels"].toArray();

	auto cstr = deck.parentConstraint();
	for(auto json_vref : arr)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		auto pvm = createProcessViewModel(deserializer,
										  cstr,
										  &deck);
		deck.addProcessViewModel(pvm);
	}

	deck.setHeight(m_obj["Height"].toInt());
	deck.setPosition(m_obj["Position"].toInt());
	// TODO deck.selectForEdition(m_obj["EditedProcess"].toInt());
}
