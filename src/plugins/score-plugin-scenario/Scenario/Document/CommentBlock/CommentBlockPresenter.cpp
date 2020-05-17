// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CommentBlockPresenter.hpp"

#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockView.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::CommentBlockPresenter)

namespace Scenario
{
CommentBlockPresenter::CommentBlockPresenter(
    const CommentBlockModel& model,
    QGraphicsItem* parentView,
    QObject* parent)
    : QObject{parent}, m_model{model}, m_view{new CommentBlockView{*this, parentView}}
{
  con(m_model.selection,
      &Selectable::changed,
      this,
      [&](bool b)
      // ensure that connection is broken when presenter is delete
      // (may crash otherwise)
      { m_view->setSelected(b); });

  con(m_model,
      &CommentBlockModel::contentChanged,
      this,
      [&](QString s)
      // ensure that connection is broken when presenter is delete
      // (may crash otherwise)
      { m_view->setHtmlContent(s); });

  m_view->setHtmlContent(m_model.content());
}

CommentBlockPresenter::~CommentBlockPresenter() { }

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
  m_view->setPos(m_model.date().toPixels(newRatio), m_view->pos().y());
  m_view->update();
}
}
