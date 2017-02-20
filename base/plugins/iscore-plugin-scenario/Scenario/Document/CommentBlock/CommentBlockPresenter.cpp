#include "CommentBlockPresenter.hpp"

#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockView.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

namespace Scenario
{
CommentBlockPresenter::CommentBlockPresenter(
    const CommentBlockModel& model, QQuickPaintedItem* parentView, QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_view{new CommentBlockView{*this, parentView}}
{
  con(m_model.selection, &Selectable::changed, this, [&](bool b)
      // ensure that connection is broken when presenter is delete
      // (may crash otherwise)
      { m_view->setSelected(b); });

  con(m_model, &CommentBlockModel::contentChanged, this, [&](QString s)
      // ensure that connection is broken when presenter is delete
      // (may crash otherwise)
      { m_view->setHtmlContent(s); });

  m_view->setHtmlContent(m_model.content());
}

CommentBlockPresenter::~CommentBlockPresenter()
{
}

const Id<CommentBlockModel>& CommentBlockPresenter::id() const
{
  return m_model.id();
}

const TimeVal& CommentBlockPresenter::date() const
{
  return model().date();
}

void CommentBlockPresenter::on_zoomRatioChanged(ZoomRatio newRatio)
{
  m_view->setPosition({m_model.date().toPixels(newRatio), m_view->y()});
  m_view->update();
}
}
