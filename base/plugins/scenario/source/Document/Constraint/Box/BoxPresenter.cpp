#include "BoxPresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/utilsCPP11.hpp>

#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <QGraphicsScene>

BoxPresenter::BoxPresenter(BoxModel* model,
                           BoxView* view,
                           QObject* parent):
    NamedObject {"BoxPresenter", parent},
    m_model {model},
    m_view {view}
{
    for(auto& deckModel : m_model->decks())
    {
        on_deckCreated_impl(deckModel);
    }

    m_duration = model->constraint()->defaultDuration();
    m_view->setText( QString{"Box.%1"}.arg(m_model->id_val()) );

    on_askUpdate();

    connect(m_model,	&BoxModel::deckCreated,
    this,		&BoxPresenter::on_deckCreated);
    connect(m_model,	&BoxModel::deckRemoved,
    this,		&BoxPresenter::on_deckRemoved);
    connect(m_model,	&BoxModel::deckPositionsChanged,
    this,		&BoxPresenter::on_deckPositionsChanged);

    connect(m_model,	&BoxModel::on_durationChanged,
    this,		&BoxPresenter::on_durationChanged);
}

BoxPresenter::~BoxPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

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

    for(auto deck : m_decks)
    {
        deck->setWidth(m_view->boundingRect().width());
    }
}

id_type<BoxModel> BoxPresenter::id() const
{
    return m_model->id();
}

void BoxPresenter::setDisabledDeckState()
{
    for(auto& deck : decks())
    {
        deck->disable();
    }
}

void BoxPresenter::setEnabledDeckState()
{
    for(auto& deck : decks())
    {
        deck->enable();
    }
}

void BoxPresenter::on_durationChanged(TimeValue duration)
{
    m_duration = duration;
    on_askUpdate();
}

void BoxPresenter::on_deckCreated(id_type<DeckModel> deckId)
{
    on_deckCreated_impl(m_model->deck(deckId));
    on_askUpdate();
}

#include "Process/Temporal/TemporalScenarioPresenter.hpp"
void BoxPresenter::on_deckCreated_impl(DeckModel* deckModel)
{
    auto deckPres = new DeckPresenter {deckModel,
                                       m_view,
                                       this};
    m_decks.push_back(deckPres);
    deckPres->on_zoomRatioChanged(m_zoomRatio);

    connect(deckPres, &DeckPresenter::askUpdate,
            this,     &BoxPresenter::on_askUpdate);


    // Set the correct view for the deck graphically if we're in a scenario
    auto scenario = dynamic_cast<TemporalScenarioPresenter*>(this->parent()->parent());
    if(!scenario)
        return;

    if(scenario->stateMachine().currentState() == ScenarioStateMachine::State::MoveDeck)
    {
        deckPres->disable();
    }
}

void BoxPresenter::on_deckRemoved(id_type<DeckModel> deckId)
{
    removeFromVectorWithId(m_decks, deckId);
    on_askUpdate();
}

void BoxPresenter::updateShape()
{
    using namespace std;
    // Vertical shape
    m_view->setHeight(height());

    // Set the decks position graphically in order.
    int currentDeckY = 20;

    for(auto& deckId : m_model->decksPositions())
    {
        auto deckPres = findById(m_decks, deckId);
        deckPres->setWidth(width());
        deckPres->setVerticalPosition(currentDeckY);
        currentDeckY += deckPres->height() + 5; // Separation between decks
    }

    // Horizontal shape
    setWidth(m_duration.toPixels(m_zoomRatio));

    for(DeckPresenter* deck : m_decks)
    {
        deck->on_parentGeometryChanged();
    }
}

void BoxPresenter::on_askUpdate()
{
    updateShape();
    emit askUpdate();
}

void BoxPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    on_askUpdate();

    // We have to change the width of the decks aftewards
    // because their width depend on the box width
    // TODO this smells.
    for(DeckPresenter* deck : m_decks)
    {
        deck->on_zoomRatioChanged(m_zoomRatio);
    }
}

void BoxPresenter::on_deckPositionsChanged()
{
    updateShape();
}
