#include "DeckModelSerialization.hpp"
#include "DeckModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterfaceSerialization.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const DeckModel& deck)
{
	readFrom(static_cast<const IdentifiedObject&>(deck));

	m_stream << deck.editedProcessViewModel();

	auto pvms = deck.processViewModels();
	m_stream << (int) pvms.size();
	for(const ProcessViewModelInterface* pvm : pvms)
	{
		readFrom(*pvm);
	}

	m_stream << deck.height()
			 << deck.position();
}

template<> void Visitor<Writer<DataStream>>::writeTo(DeckModel& deck)
{
	int editedProcessId;
	m_stream >> editedProcessId;

	int pvm_size;
	m_stream >> pvm_size;
	for(int i = 0; i < pvm_size; i++)
	{
		auto cstr = deck.parentConstraint();
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