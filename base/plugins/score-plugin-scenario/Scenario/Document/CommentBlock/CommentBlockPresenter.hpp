#pragma once

#include <Process/TimeValue.hpp>
#include <QPoint>
#include <score/model/Identifier.hpp>
#include <score/widgets/GraphicsItem.hpp>

class QGraphicsItem;
class QTextDocument;

namespace Scenario
{
class CommentBlockView;
class CommentBlockModel;
class CommentBlockPresenter final : public QObject
{
  Q_OBJECT
public:
  CommentBlockPresenter(
      const CommentBlockModel& model,
      QGraphicsItem* parentView,
      QObject* parent);

  ~CommentBlockPresenter();

  const Id<CommentBlockModel>& id() const;
  int32_t id_val() const
  {
    return id().val();
  }

  const CommentBlockModel& model() const
  {
    return m_model;
  }

  CommentBlockView* view() const
  {
    return m_view;
  }

  const TimeVal& date() const;

  void on_zoomRatioChanged(ZoomRatio newRatio);
Q_SIGNALS:
  void moved(const QPointF&);
  void released(const QPointF&);
  void selected();
  void editFinished(QString);

private:
  const CommentBlockModel& m_model;
  graphics_item_ptr<CommentBlockView> m_view;
};
}
