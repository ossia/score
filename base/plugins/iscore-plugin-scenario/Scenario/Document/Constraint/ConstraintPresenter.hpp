#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <QPoint>
#include <QString>
#include <QTimer>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
#include <nano_signal_slot.hpp>

namespace Process
{
struct ProcessPresenterContext;
class ProcessModel;
class LayerPresenter;
class LayerView;
}

namespace Scenario
{
class ConstraintHeader;
class ConstraintModel;
class ConstraintView;

struct LayerData
{
  LayerData() = default;
  LayerData(const LayerData&) = default;
  LayerData(LayerData&&) = default;
  LayerData& operator=(const LayerData&) = default;
  LayerData& operator=(LayerData&&) = default;
  LayerData(
      const Process::ProcessModel* m,
      Process::LayerPresenter* p,
      Process::LayerView* v)
      : model(m), presenter(p), view(v)
  {
  }

  const Process::ProcessModel* model{};
  Process::LayerPresenter* presenter{};
  Process::LayerView* view{};
};


class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintPresenter : public QObject,
                                                          public Nano::Observer
{
  Q_OBJECT

public:
  ConstraintPresenter(
      const ConstraintModel& model,
      ConstraintView* view,
      ConstraintHeader* header,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent);
  virtual ~ConstraintPresenter();
  virtual void updateScaling();

  bool isSelected() const;

  const ConstraintModel& model() const;

  ConstraintView* view() const;

  virtual void on_zoomRatioChanged(ZoomRatio val);
  ZoomRatio zoomRatio() const
  {
    return m_zoomRatio;
  }

  const Id<ConstraintModel>& id() const;

  void on_minDurationChanged(const TimeVal&);
  void on_maxDurationChanged(const TimeVal&);

  void on_playPercentageChanged(double t);

signals:
  void pressed(QPointF) const;
  void moved(QPointF) const;
  void released(QPointF) const;

  void slotHandlePressed(QPointF) const;
  void slotHandleMoved(QPointF) const;
  void slotHandleReleased(QPointF) const;

  void askUpdate();
  void heightChanged();           // The vertical size
  void heightPercentageChanged(); // The vertical position

protected:
  // Process presenters are in the slot presenters.
  const ConstraintModel& m_model;

  ZoomRatio m_zoomRatio{};
  ConstraintView* m_view{};
  ConstraintHeader* m_header{};

  const Process::ProcessPresenterContext& m_context;

  void updateChildren();
  void updateBraces();
};

template <typename T>
const typename T::view_type* view(const T* obj)
{
  return static_cast<const typename T::view_type*>(obj->view());
}

template <typename T>
typename T::view_type* view(T* obj)
{
  return static_cast<typename T::view_type*>(obj->view());
}

template <typename T>
typename T::view_type& view(const T& obj)
{
  return static_cast<typename T::view_type&>(*obj.view());
}

template <typename T>
typename T::view_type& view(T& obj)
{
  return static_cast<typename T::view_type&>(*obj.view());
}
}
