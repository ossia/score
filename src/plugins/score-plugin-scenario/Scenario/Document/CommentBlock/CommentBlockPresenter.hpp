#pragma once

#include <Process/TimeValue.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>

#include <QPoint>

#include <score_plugin_scenario_export.h>

#include <verdigris>

class QGraphicsItem;
class QTextDocument;

namespace Scenario
{
class CommentBlockView;
class CommentBlockModel;
class SCORE_PLUGIN_SCENARIO_EXPORT CommentBlockPresenter final : public QObject
{
  W_OBJECT(CommentBlockPresenter)
public:
  CommentBlockPresenter(
      const CommentBlockModel& model,
      QGraphicsItem* parentView,
      QObject* parent);

  ~CommentBlockPresenter();

  const Id<CommentBlockModel>& id() const;
  int32_t id_val() const { return id().val(); }

  const CommentBlockModel& model() const { return m_model; }

  CommentBlockView* view() const { return m_view; }

  const TimeVal& date() const;

  void on_zoomRatioChanged(ZoomRatio newRatio);

public:
  void moved(const QPointF& arg_1) W_SIGNAL(moved, arg_1);
  void released(const QPointF& arg_1) W_SIGNAL(released, arg_1);
  void selected() W_SIGNAL(selected);
  void editFinished(QString arg_1) W_SIGNAL(editFinished, arg_1);

private:
  const CommentBlockModel& m_model;
  graphics_item_ptr<CommentBlockView> m_view;
};
}
