#include "BoxPresenter.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/utilsCPP11.hpp>

#include <QGraphicsScene>

BoxPresenter::BoxPresenter(BoxModel* model,
						   BoxView* view,
						   QObject* parent):
	NamedObject{"BoxPresenter", parent},
	m_model{model},
	m_view{view}
{
	for(auto& deckModel : m_model->decks())
	{
		on_deckCreated_impl(deckModel);
	}

	on_askUpdate();

	connect(m_model,	&BoxModel::deckCreated,
			this,		&BoxPresenter::on_deckCreated);
	connect(m_model,	&BoxModel::deckRemoved,
			this,		&BoxPresenter::on_deckRemoved);
}

BoxPresenter::~BoxPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

int BoxPresenter::height() const
{
	int totalHeight = 25; // No deck -> not visible ? or just "add a process" button ? Bottom bar ? How to make it visible ?
	for(auto& deck : m_decks)
	{
		totalHeight += deck->height() + 5;
	}
	return totalHeight;
}

int BoxPresenter::width() const
{
	return m_view->boundingRect().width();
}

void BoxPresenter::setWidth(int w)
{
	m_view->setWidth(w);
}

id_type<BoxModel> BoxPresenter::id() const
{
	return m_model->id();
}

void BoxPresenter::on_deckCreated(id_type<DeckModel> deckId)
{
	on_deckCreated_impl(m_model->deck(deckId));
}

void BoxPresenter::on_deckCreated_impl(DeckModel* deckModel)
{
	auto deckView = new DeckView{m_view};
	deckView->setPos(5, 5);
	auto deckPres = new DeckPresenter{deckModel,
					  deckView,
					  this};
	m_decks.push_back(deckPres);


	connect(deckPres, &DeckPresenter::submitCommand,
			this,		&BoxPresenter::submitCommand);
	connect(deckPres, &DeckPresenter::elementSelected,
			this,		&BoxPresenter::elementSelected);

	connect(deckPres, &DeckPresenter::askUpdate,
			this,		&BoxPresenter::on_askUpdate);

	on_askUpdate();
}

void BoxPresenter::on_deckRemoved(id_type<DeckModel> deckId)
{
	removeFromVectorWithId(m_decks, deckId);
	on_askUpdate();
}

void BoxPresenter::updateShape()
{
	using namespace std;
	m_view->setHeight(height());

	// 1. Make an array with the Decks stored by their positions
	vector<DeckPresenter*> sortedDecks(m_decks.size());
	for(DeckPresenter* deck : m_decks)
	{
		sortedDecks[deck->position()] = deck;
	}

	// 2. Set their position in order.
	int currentDeckY = 20;
	for(auto& deck : sortedDecks)
	{
		deck->setVerticalPosition(currentDeckY);
		currentDeckY += deck->height() + 5;
	}
}

void BoxPresenter::on_askUpdate()
{
	updateShape();
	emit askUpdate();
}
