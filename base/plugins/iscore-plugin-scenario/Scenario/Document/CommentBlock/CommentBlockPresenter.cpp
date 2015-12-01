#include "CommentBlockPresenter.hpp"

#include <iscore/widgets/GraphicsItem.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockView.hpp>

CommentBlockPresenter::CommentBlockPresenter(
        const CommentBlockModel& model,
        QGraphicsObject* parentView,
        QObject* parent):
    NamedObject{"CommentBlock", parent},
    m_model{model},
    m_view{new CommentBlockView{*this, parentView}}
{

}

CommentBlockPresenter::~CommentBlockPresenter()
{
    deleteGraphicsObject(m_view);
}

const Id<CommentBlockModel>&CommentBlockPresenter::id() const
{
    return m_model.id();
}

const TimeValue&CommentBlockPresenter::date() const
{
    return model().date();
}

void CommentBlockPresenter::pressed(const QPointF& pos)
{
    m_clickedPoint = pos;
}

