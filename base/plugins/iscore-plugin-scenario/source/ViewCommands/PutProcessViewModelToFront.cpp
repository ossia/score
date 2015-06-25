#include "PutProcessViewModelToFront.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

PutProcessViewModelToFront::PutProcessViewModelToFront(
        ObjectPath&& deckPath,
        const id_type<ProcessViewModel>& pid):
    m_deckPath{std::move(deckPath)},
    m_pid{pid}
{

}

void PutProcessViewModelToFront::redo()
{
    m_deckPath.find<DeckModel>().putToFront(m_pid);
}
