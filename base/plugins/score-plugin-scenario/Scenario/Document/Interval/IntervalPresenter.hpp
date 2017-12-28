#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <QPoint>
#include <QString>
#include <QTimer>
#include <score/model/Identifier.hpp>
#include <score_plugin_scenario_export.h>
#include <nano_signal_slot.hpp>

#include <Scenario/Document/Interval/SlotPresenter.hpp>

namespace Process
{
struct ProcessPresenterContext;
class ProcessModel;
class LayerPresenter;
class LayerView;
}

namespace Scenario
{
class IntervalHeader;
class IntervalModel;
class IntervalView;

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


class SCORE_PLUGIN_SCENARIO_EXPORT IntervalPresenter : public QObject,
                                                          public Nano::Observer
{
  Q_OBJECT

public:
  IntervalPresenter(
      const IntervalModel& model,
      IntervalView* view,
      IntervalHeader* header,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent);
  virtual ~IntervalPresenter();
  virtual void updateScaling();

  bool isSelected() const;

  const IntervalModel& model() const;

  IntervalView* view() const;

  virtual void on_zoomRatioChanged(ZoomRatio val);
  ZoomRatio zoomRatio() const
  {
    return m_zoomRatio;
  }

  const std::vector<SlotPresenter>& getSlots() const {
    return m_slots;
  }

  const Id<IntervalModel>& id() const;

  void on_minDurationChanged(const TimeVal&);
  void on_maxDurationChanged(const TimeVal&);

  double on_playPercentageChanged(double t);

  virtual void selectedSlot(int) const;
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
  const IntervalModel& m_model;

  ZoomRatio m_zoomRatio{};
  IntervalView* m_view{};
  IntervalHeader* m_header{};

  const Process::ProcessPresenterContext& m_context;

  void updateChildren();
  void updateBraces();
  std::vector<SlotPresenter> m_slots;
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
