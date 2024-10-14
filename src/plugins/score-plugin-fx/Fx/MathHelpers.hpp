#pragma once

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/math/math_expression.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes
{

template <typename State>
static void setMathExpressionTiming(
    State& self, int64_t input_time, int64_t prev_time, std::integral auto parent_dur)
    = delete;

template <typename State>
static void setMathExpressionTiming(
    State& self, int64_t input_time, int64_t prev_time,
    std::floating_point auto parent_dur)
{
  self.cur_time = input_time;
  self.cur_deltatime = (input_time - prev_time);
  self.cur_pos = parent_dur > 0 ? double(input_time) / parent_dur : 0;
}

template <typename State>
static void setMathExpressionTiming(
    State& self, ossia::time_value input_time, ossia::time_value prev_time,
    ossia::time_value parent_dur, double modelToSamples)
{
  setMathExpressionTiming(
      self, input_time.impl * modelToSamples, prev_time.impl * modelToSamples,
      parent_dur.impl * modelToSamples);
}

// template <typename State>
// static void setMathExpressionTiming(State& self, const ossia::token_request& tk, ossia::exec_state_facade st)
// {
//   setMathExpressionTiming(self, tk.date, tk.prev_date, tk.parent_duration, st.modelToSamples());
// }

template <typename State>
static void setMathExpressionTiming(State& self, const halp::tick_flicks& tk)
{
  self.cur_time = tk.end_in_flicks;
  self.cur_deltatime = tk.end_in_flicks - tk.start_in_flicks;
  self.cur_pos = tk.parent_duration > 0 ? tk.relative_position : 0.;
}

#if FX_UI
static void miniMathItem(const tuplet::tuple<Control::LineEdit>& controls, Process::LineEdit& edit, const Process::ProcessModel& process, QGraphicsItem& parent, QObject& context, const Process::Context& doc)
{
  using namespace Process;
  using namespace std;
  using namespace tuplet;
  const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();

  auto edit_item = makeControlNoText(get<0>(controls), edit, parent, context, doc, portFactory);
  edit_item.control.setTextWidth(100);
  edit_item.control.setPos(15, 0);

  if(auto obj = dynamic_cast<score::ResizeableItem*>(&parent))
  {
    QObject::connect(&edit_item.control, &score::QGraphicsLineEdit::sizeChanged, obj, &score::ResizeableItem::childrenSizeChanged);
  }
}

static void miniMathItem(const tuplet::tuple<Control::LineEdit, Control::IntSpinBox>& controls, Process::LineEdit& edit, Process::IntSpinBox& count, const Process::ProcessModel& process, QGraphicsItem& parent, QObject& context, const Process::Context& doc)
{
  using namespace Process;
  using namespace std;
  using namespace tuplet;
  const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();

  auto count_item = makeControlNoText(get<1>(controls), count, parent, context, doc, portFactory);
  count_item.control.setPos(15, 0);
  auto edit_item = makeControlNoText(get<0>(controls), edit, parent, context, doc, portFactory);
  edit_item.control.setTextWidth(100);
  edit_item.control.setPos(15, 25);

  if(auto obj = dynamic_cast<score::ResizeableItem*>(&parent))
  {
    QObject::connect(&edit_item.control, &score::QGraphicsLineEdit::sizeChanged, obj, &score::ResizeableItem::childrenSizeChanged);
  }
}
static void mathItem(const tuplet::tuple<Control::LineEdit, Control::FloatSlider, Control::FloatSlider, Control::FloatSlider>& controls, Process::LineEdit& edit, Process::FloatSlider& a, Process::FloatSlider& b, Process::FloatSlider& c, const Process::ProcessModel& process, QGraphicsItem& parent, QObject& context, const Process::Context& doc)
{
  using namespace Process;
  using namespace std;
  using namespace tuplet;
  const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();

  const auto c0 = 10;

  auto c0_bg = new score::BackgroundItem{&parent};
  c0_bg->setRect({0., 0., 300., 200.});

  auto edit_item = makeControl(get<0>(controls), edit, parent, context, doc, portFactory);
  edit_item.control.setTextWidth(280);
  edit_item.root.setPos(c0, 40);
  /*
  ((QGraphicsProxyWidget&)edit_item.control).setMinimumWidth(200);
  ((QGraphicsProxyWidget&)edit_item.control).setMaximumWidth(200);
  ((QGraphicsProxyWidget&)edit_item.control).widget()->setMinimumWidth(200);
  ((QGraphicsProxyWidget&)edit_item.control).widget()->setMaximumWidth(200);
  */

  auto a_item = makeControl(get<1>(controls), a, parent, context, doc, portFactory);
  a_item.root.setPos(c0, 5);
  auto b_item = makeControl(get<2>(controls), b, parent, context, doc, portFactory);
  b_item.root.setPos(c0 + 70, 5);
  auto c_item = makeControl(get<3>(controls), c, parent, context, doc, portFactory);
  c_item.root.setPos(c0 + 140, 5);
}
#endif
}
