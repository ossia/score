#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/network/value/format_value.hpp>

namespace Nodes::RateLimiter
{

struct Node
{
  struct RateLimiter
  {
  };

  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Rate Limiter";
    static const constexpr auto objectKey = "RateLimiter";
    static const constexpr auto category = "Control/Mappings";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description
        = "Limit and quantize the rate of a value stream";
    static const uuid_constexpr auto uuid
        = make_uuid("76cfd504-7c10-4bdb-a1b4-fbe449cc06f0");

    static const constexpr auto controls = tuplet::make_tuple(
        Control::Widgets::QuantificationChooser(),
        Control::IntSlider{"ms.", 0, 1000, 10});

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
  };

  class State
  {
  public:
    bool should_output(
        float quantif,
        const int64_t& ms,
        const ossia::token_request& t,
        const ossia::exec_state_facade& st)
    {
      if (quantif != 0.)
        return true;
      else if (t.date.impl >= (last_time + ms * flicks_per_ms))
      {
        last_time = t.date.impl;
        return true;
      }
      return false;
    }

  private:
    static const constexpr int64_t flicks_per_ms = 705'600;
    int64_t last_time{};
  };

  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::value_port& in,
      float quantif,
      uint64_t ms,
      ossia::value_port& out,
      ossia::token_request t,
      ossia::exec_state_facade st,
      State& self)
  {
    for (const ossia::timed_value& v : in.get_data())
    {

      if (quantif <= 0.)
      {
        if (self.should_output(quantif, ms, t, st))
          out.write_value(
              v.value, v.timestamp); // TODO fix accuracy of timestamp
      }
      else
      {
        if (auto time = t.get_physical_quantification_date(
                1. / quantif, st.modelToSamples()))
          out.write_value(v.value, *time);
      }
    }
  }

  static void item(
      Process::ComboBox& quantif,
      Process::IntSlider& ms,
      const Process::ProcessModel& process,
      QGraphicsItem& parent,
      QObject& context,
      const Process::Context& doc)
  {
    using namespace Process;
    const Process::PortFactoryList& portFactory
        = doc.app.interfaces<Process::PortFactoryList>();
    const auto cMarg = 15;
    const auto w = 165;

    auto c1_bg = new score::BackgroundItem{&parent};
    c1_bg->setRect({0., 0, w, 45.});

    auto quant_item = makeControl(
        tuplet::get<0>(Metadata::controls),
        quantif,
        parent,
        context,
        doc,
        portFactory);
    quant_item.root.setPos(5, 0);
    quant_item.control.setPos(cMarg, cMarg);

    auto ms_item = makeControl(
        tuplet::get<1>(Metadata::controls),
        ms,
        parent,
        context,
        doc,
        portFactory);
    ms_item.root.setPos(100, 0);
    ms_item.control.setPos(0, cMarg);
  }
};
}
