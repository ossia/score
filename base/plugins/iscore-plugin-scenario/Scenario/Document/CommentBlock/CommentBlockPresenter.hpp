#pragma once

#include <Process/TimeValue.hpp>
#include <QPoint>
#include <iscore/model/Identifier.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

class QQuickPaintedItem;
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
      QQuickPaintedItem* parentView,
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
signals:
  void moved(const QPointF&);
  void released(const QPointF&);
  void selected();
  void editFinished(QString);

private:
  const CommentBlockModel& m_model;
  graphics_item_ptr<CommentBlockView> m_view;
};
}
