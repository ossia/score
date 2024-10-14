#pragma once
#include <Fx/NoiseFilter.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
namespace Nodes::Smooth::v2
{
struct Node : NoiseState
{
  halp_meta(name, "Smooth")
  halp_meta(c_name, "ValueFilter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/smooth.html#smooth")
  halp_meta(description, "Filter noisy value stream")
  halp_meta(uuid, "bf603921-5a48-4aa5-9bc1-48a762be6467")

  struct
  {
    // FIXME all incorrect when token_request smaller than tick
    halp::val_port<"in", std::optional<ossia::value>> port{};
    halp::enum_t<dno::type, "Type"> type;
    halp::knob_f32<"Amount", halp::range{0., 1., 0.1}> amount;
    halp::log_hslider_f32<"Freq (1e/LP)", halp::range{0.001, 300., 120.}> freq;
    halp::log_hslider_f32<"Cutoff (1e/LP)", halp::range{0.001, 10., 1.}> cutoff;
    halp::hslider_f32<"Beta (1e only)", halp::range{0.001, 10., 1.}> beta;
    halp::toggle<"Continuous", halp::toggle_setup{}> continuous;
  } inputs;
  struct
  {
    value_out port{};
  } outputs;
  void operator()()
  {
    if(auto& v = this->inputs.port.value)
    {
      auto filtered = this->filter(
          *v, inputs.type, inputs.amount, inputs.freq, inputs.cutoff, inputs.beta);
      outputs.port(std::move(filtered)); // TODO fix accuracy of timestamp
      this->last = *v;
    }
    else
    {
      // FIXME this is slightly imprecise as the filters expect regular input
      // and if we have split ticks that's not what happens
      if(inputs.continuous && this->last.valid())
      {
        auto filtered = this->filter(
            this->last, inputs.type, inputs.amount, inputs.freq, inputs.cutoff,
            inputs.beta);
        outputs.port(std::move(filtered));
      }
    }
  }

#if FX_UI
  static void item(
      Process::Enum& type, Process::FloatKnob& amount, Process::LogFloatSlider& freq,
      Process::LogFloatSlider& cutoff, Process::FloatSlider& beta, Process::Toggle& cont,
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

    auto cont_item = makeControl(
        tuplet::get<5>(Metadata::controls), cont, parent, context, doc, portFactory);
    cont_item.root.setPos(tMarg, 0);
    //cont_item.control.setPos(10, cMarg);
  }
#endif
};
}
