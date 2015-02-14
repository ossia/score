#include "DeckModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "source/ProcessInterfaceSerialization/ProcessViewModelInterfaceSerialization.hpp"
#include <interface/serialization/JSONVisitor.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>

template<> void Visitor<Reader<DataStream>>::readFrom(const DeckModel& deck)
{
	readFrom(static_cast<const IdentifiedObject<DeckModel>&>(deck));

	m_stream << deck.editedProcessViewModel();

	auto pvms = deck.processViewModels();
	m_stream << (int) pvms.size();
	for(auto pvm : pvms)
	{
		readFrom(*pvm);
	}

	m_stream << deck.height();

	insertDelimiter();
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
	m_stream >> height;
	deck.setHeight(height);

	deck.selectForEdition(editedProcessId);

	checkDelimiter();
}





template<> void Visitor<Reader<JSON>>::readFrom(const DeckModel& deck)
{
	readFrom(static_cast<const IdentifiedObject<DeckModel>&>(deck));

	m_obj["EditedProcess"] = toJsonObject(deck.editedProcessViewModel());
	m_obj["Height"] = deck.height();

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

	id_type<ProcessViewModelInterface> editedPvm;
	fromJsonObject(m_obj["EditedProcess"].toObject(), editedPvm);
	deck.selectForEdition(editedPvm);
}
