#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Interval/SlotPresenter.hpp>

#include <score/model/Identifier.hpp>

#include <QPoint>

#include <nano_signal_slot.hpp>
#include <score_plugin_scenario_export.h>

#include <verdigris>

namespace Process
{
struct Context;
}

namespace Scenario
{
class IntervalHeader;
class IntervalModel;
class IntervalView;

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalPresenter : public QObject, public Nano::Observer
{
  W_OBJECT(IntervalPresenter)

public:
  IntervalPresenter(
      ZoomRatio zoom,
      const IntervalModel& model,
      IntervalView* view,
      IntervalHeader* header,
      const Process::Context& ctx,
      QObject* parent);
  virtual ~IntervalPresenter();
  virtual void updateScaling();

  bool isSelected() const;

  const IntervalModel& model() const;

  IntervalView* view() const;
  IntervalHeader* header() const { return m_header; }

  virtual void on_zoomRatioChanged(ZoomRatio val);
  ZoomRatio zoomRatio() const { return m_zoomRatio; }

  const std::vector<SlotPresenter>& getSlots() const { return m_slots; }

  const Id<IntervalModel>& id() const;

  const Process::Context& context() const { return m_context; }

  void on_minDurationChanged(const TimeVal&);
  void on_maxDurationChanged(const TimeVal&);

  double on_playPercentageChanged(double t);

  virtual void startSlotDrag(int slot, QPointF) const = 0;
  virtual void stopSlotDrag() const = 0;

  virtual void selectedSlot(int) const = 0;
  virtual void requestSlotMenu(int slot, QPoint pos, QPointF sp) const = 0;

  void updateAllSlots() const;

public:
  void pressed(QPointF arg_1) const E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(QPointF arg_1) const E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(QPointF arg_1) const E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

  void askUpdate() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, askUpdate)
  void heightChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT,
                                heightChanged) // The vertical size
  void heightPercentageChanged() E_SIGNAL(
      SCORE_PLUGIN_SCENARIO_EXPORT,
      heightPercentageChanged) // The vertical position

protected:
  void updateChildren();
  void updateBraces();

  // Process presenters are in the slot presenters.
  const IntervalModel& m_model;

  ZoomRatio m_zoomRatio{};
  IntervalView* m_view{};
  IntervalHeader* m_header{};
  const Process::Context& m_context;

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
