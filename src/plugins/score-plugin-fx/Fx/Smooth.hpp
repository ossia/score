#pragma once
#include <Fx/NoiseFilter.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/logger.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Nodes::Smooth
{
using namespace dno;

namespace v1
{
struct Node : NoiseState
{
  halp_meta(name, "Smooth (old)")
  halp_meta(c_name, "ValueFilter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/smooth.html#smooth")
  halp_meta(description, "Filter noisy value stream")
  halp_meta(uuid, "809c014d-7d02-45dc-8849-de7a7db5fe67")
  halp_flag(deprecated);

  struct
  {
    // FIXME all incorrect when token_request smaller than tick
    struct : halp::val_port<"in", ossia::value>
    {
      // Messages to this port trigger a new computation cycle with updated timestamps
      halp_flag(active_port);
    } port;
    halp::enum_t<dno::type, "Type"> type;
    halp::knob_f32<"Amount", halp::range{0., 1., 0.1}> amount;
    halp::log_hslider_f32<"Freq (1e/LP)", halp::range{0.001, 300., 120.}> freq;
    halp::log_hslider_f32<"Cutoff (1e/LP)", halp::range{0.001, 10., 1.}> cutoff;
    halp::hslider_f32<"Beta (1e only)", halp::range{0.001, 10., 1.}> beta;
  } inputs;
  struct
  {
    value_out port{};
  } outputs;

  void operator()()
  {
    auto& v = this->inputs.port.value;

    auto filtered = this->filter(
        v, inputs.type, inputs.amount, inputs.freq, inputs.cutoff, inputs.beta);
    outputs.port(std::move(filtered)); // TODO fix accuracy of timestamp

    this->last = v;
  }

#if FX_UI
  static void item(
      Process::Enum& type, Process::FloatKnob& amount, Process::LogFloatSlider& freq,
      Process::LogFloatSlider& cutoff, Process::FloatSlider& beta,
      const Process::ProcessModel& process, QGraphicsItem& parent, QObject& context,
      const Process::Context& doc)
  {
    using namespace Process;
    const Process::PortFactoryList& portFactory
        = doc.app.interfaces<Process::PortFactoryList>();
    const auto tMarg = 5;
    const auto cMarg = 15;
    const auto h = 125;
    const auto w = 220;

    auto c0_bg = new score::BackgroundItem{&parent};
    c0_bg->setRect({0., 0., w, h});

    auto type_item = makeControl(
        tuplet::get<0>(Metadata::controls), type, parent, context, doc, portFactory);
    type_item.root.setPos(70, 0);
    type_item.control.rows = 4;
    type_item.control.columns = 1;
    type_item.control.setRect(QRectF{0, 0, 60, 105});

    auto amount_item = makeControl(
        tuplet::get<1>(Metadata::controls), amount, parent, context, doc, portFactory);
    amount_item.root.setPos(tMarg, 30);
    amount_item.control.setPos(0, cMarg);

    auto freq_item = makeControl(
        tuplet::get<2>(Metadata::controls), freq, parent, context, doc, portFactory);
    freq_item.root.setPos(140, 0);
    freq_item.control.setPos(0, cMarg);

    auto cutoff_item = makeControl(
        tuplet::get<3>(Metadata::controls), cutoff, parent, context, doc, portFactory);
    cutoff_item.root.setPos(140, 40);
    cutoff_item.control.setPos(0, cMarg);

    auto beta_item = makeControl(
        tuplet::get<4>(Metadata::controls), beta, parent, context, doc, portFactory);
    beta_item.root.setPos(140, 80);
    beta_item.control.setPos(0, cMarg);
  }
#endif
};
}
}
