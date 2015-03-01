#include "BoxPresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/utilsCPP11.hpp>

#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <QGraphicsScene>

BoxPresenter::BoxPresenter(BoxModel* model,
                           BoxView* view,
                           QObject* parent) :
    NamedObject {"BoxPresenter", parent},
            m_model {model},
m_view {view}
{
    for(auto& deckModel : m_model->decks())
    {
        on_deckCreated_impl(deckModel);
    }

    m_duration = model->constraint()->defaultDuration();

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

void BoxPresenter::on_durationChanged(TimeValue duration)
{
    m_duration = duration;
    on_askUpdate();
}

void BoxPresenter::on_deckCreated(id_type<DeckModel> deckId)
{
    on_deckCreated_impl(m_model->deck(deckId));
}

void BoxPresenter::on_deckCreated_impl(DeckModel* deckModel)
{
    auto deckView = new DeckView {m_view};
    deckView->setPos(0, 0);

    auto deckPres = new DeckPresenter {deckModel,
                                       deckView,
                                       this
                                      };
    m_decks.push_back(deckPres);
    deckPres->on_horizontalZoomChanged(m_horizontalZoomSliderVal);


    connect(deckPres, &DeckPresenter::submitCommand,
            this,     &BoxPresenter::submitCommand);
    connect(deckPres, &DeckPresenter::newSelection,
            this,     &BoxPresenter::newSelection);

    connect(deckPres, &DeckPresenter::askUpdate,
            this,     &BoxPresenter::on_askUpdate);

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
    double secPerPixel = millisecondsPerPixel(m_horizontalZoomSliderVal);
    setWidth(m_duration.msec() / secPerPixel);

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

void BoxPresenter::on_horizontalZoomChanged(int val)
{
    m_horizontalZoomSliderVal = val;

    on_askUpdate();

    // We have to change the width of the decks aftewards
    // because their width depend on the box width
    for(auto deck : m_decks)
    {
        deck->on_horizontalZoomChanged(val);
    }
}

void BoxPresenter::on_deckPositionsChanged()
{
    updateShape();
}
