#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <ossia/network/value/format_value.hpp>
#include <ossia/detail/logger.hpp>

#include <libossia/3rdparty/dno/DeNoiser.hpp>

namespace Nodes::ValueFilter
{
using namespace dno;

struct Node
{
  struct NoiseFilter
  {
    NoiseFilter()
        : dno_i{DeNoiser<int>{}}
        , dno_v{
              DeNoiser<double>{},
              DeNoiser<double>{},
              DeNoiser<double>{},
              DeNoiser<double>{}}
    {
    }

    DeNoiser<int> dno_i;
    std::vector<DeNoiser<double>> dno_v;

    ossia::value filter(const ossia::value& val)
    {
      struct vis
      {
        ossia::value operator()() const { return {}; }
        ossia::value operator()(const ossia::impulse&) const { return {}; }
        ossia::value operator()(int i) const { return nFilt->dno_i(i); }
        ossia::value operator()(float f) const { return nFilt->dno_v[0](f); }
        ossia::value operator()(bool b) const { return b; }
        ossia::value operator()(const std::string& s) const { return s; }
        ossia::value operator()(const ossia::vec2f& t) const
        {
          return ossia::make_vec(nFilt->dno_v[0](t[0]), nFilt->dno_v[1](t[1]));
        }
        ossia::value operator()(const ossia::vec3f& t) const
        {
          return ossia::make_vec(
              nFilt->dno_v[0](t[0]),
              nFilt->dno_v[1](t[1]),
              nFilt->dno_v[2](t[2]));
        }
        ossia::value operator()(const ossia::vec4f& t) const
        {
          return ossia::make_vec(
              nFilt->dno_v[0](t[0]),
              nFilt->dno_v[1](t[1]),
              nFilt->dno_v[2](t[2]),
              nFilt->dno_v[3](t[3]));
        }
        ossia::value operator()(const std::vector<ossia::value>& t) const
        {
          return t;
        }

        NoiseFilter* nFilt;

        vis(NoiseFilter* parent)
            : nFilt{parent}
        {}
      };

      try
      {
        return ossia::apply(vis{this}, val.v);
      }
      catch (std::exception& e)
      {
        ossia::logger().error("{}", e.what());
      }
      catch (...)
      {
        ossia::logger().error("error");
      }

      return val;
    }

    void set_amount(float amt)
    {
      dno_i.set_amount(amt);
      for (auto& f : dno_v)
        f.set_amount(amt);
    }

    void set_type(const type& t = OneEuro)
    {
      dno_i.set_type(t);
      for (auto& f : dno_v)
        f.set_type(t);
    }

    void set_freq(double freq) // 1e & LP
    {
      dno_i.set_freq(freq);
      for (auto& f : dno_v)
        f.set_freq(freq);
    }

    void set_cutoff(double cutoff) // 1e & LP
    {
      dno_i.set_cutoff(cutoff);
      for (auto& f : dno_v)
        f.set_cutoff(cutoff);
    }

    void set_beta(double beta) // 1e only
    {
      dno_i.set_1e_beta(beta);
      for (auto& f : dno_v)
        f.set_1e_beta(beta);
    }
  };

  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Value filter";
    static const constexpr auto objectKey = "ValueFilter";
    static const constexpr auto category = "Mappings";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description
        = "Filter and limit noisy value stream";
    static const uuid_constexpr auto uuid
        = make_uuid("809c014d-7d02-45dc-8849-de7a7db5fe67");

    static const constexpr auto controls = std::make_tuple(
        Control::Toggle{"smooth", true},
        Control::make_enum(
            "Type",
            0U,
            ossia::make_array("OneEuro", "LowPass", "Average", "Median")),
        Control::FloatKnob{"amount", 0., 1., 0.1},
        Control::FloatSlider{"freq (1e/LP)", 0., 300., 120.},
        Control::FloatSlider{"cutoff (1e/LP)", 0., 10., 1.},
        Control::FloatSlider{"beta (1e only)", 0., 10., 1.},
        Control::Toggle{"rate", false},
        Control::IntSlider{"ms.", 0, 1000, 10},
        Control::Widgets::QuantificationChooser());

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
  };

  class State
  {
  public:
    ossia::value filter(
        const bool smooth,
        const ossia::value& value,
        const std::string& type,
        const float& amount,
        const float& freq,
        const float& cutoff,
        const float& beta)
    {
      if (smooth)
      {
        if (type != prev_type)
        {
          nf.set_type(GetFilterType(type));
          prev_type = type;
        }
        if (amount != prev_amount)
        {
          nf.set_amount(amount);
          prev_amount = amount;
        }
        if (freq != prev_freq)
        {
          nf.set_freq(freq);
          prev_freq = freq;
        }
        if (cutoff != prev_cutoff)
        {
          nf.set_cutoff(cutoff);
          prev_cutoff = cutoff;
        }
        if (beta != prev_beta)
        {
          nf.set_beta(beta);
          prev_beta = beta;
        }

        return nf.filter(value);
      }
      else
        return value;
    }

  private:
    std::string prev_type{"OneEuro"};
    float prev_amount{1 / SCALED_AMOUNT},
    prev_freq{INIT_FREQ},
    prev_cutoff{INIT_CUTOFF},
    prev_beta{INIT_BETA};

    NoiseFilter nf{};

    type GetFilterType(std::string_view str) noexcept
    {
      if (str == "LowPass")
        return LowPass;
      else if (str == "Average")
        return Average;
      else if (str == "Median")
        return Median;
      return OneEuro;
    }
  };

  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::value_port& in,
      bool smooth,
      const std::string& type,
      float amount,
      float freq,
      float cutoff,
      float beta,
      bool rate,
      int ms,
      float quantif,
      ossia::value_port& out,
      ossia::token_request t,
      ossia::exec_state_facade st,
      State& self)
  {
    for (const ossia::timed_value& v : in.get_data())
    {
      if (rate)
      {
        out.write_value(
            self.filter(smooth, v.value, type, amount, freq, cutoff, beta),
            v.timestamp);
      }
      else
      {
        out.write_value(
            self.filter(smooth, v.value, type, amount, freq, cutoff, beta),
            v.timestamp);
      }
    }
  }

  static void item(
      Process::Toggle& smooth,
      Process::Enum& type,
      Process::FloatKnob& amount,
      Process::FloatSlider& freq,
      Process::FloatSlider& cutoff,
      Process::FloatSlider& beta,
      Process::Toggle& rate,
      Process::IntSlider& ms,
      Process::ComboBox& quantif,
      const Process::ProcessModel& process,
      QGraphicsItem& parent,
      QObject& context,
      const Process::Context& doc)
  {
    using namespace Process;
    const Process::PortFactoryList& portFactory
        = doc.app.interfaces<Process::PortFactoryList>();
    const auto tMarg = 10;
    const auto cMarg = 15;
    const auto h = 125;
    const auto w = 220;

    auto c0_bg = new score::BackgroundItem{&parent};
    c0_bg->setRect({0., 0., w, h});
    auto c1_bg = new score::BackgroundItem{&parent};
    c1_bg->setRect({0., h, w, 45.});

    auto smooth_item = makeControl(
        std::get<0>(Metadata::controls),
        smooth,
        parent,
        context,
        doc,
        portFactory);
    smooth_item.root.setPos(tMarg, 0);
    smooth_item.control.setPos(0, tMarg);

    auto type_item = makeControl(
        std::get<1>(Metadata::controls),
        type,
        parent,
        context,
        doc,
        portFactory);
    type_item.root.setPos(70, 0);
    type_item.control.rows = 4;
    type_item.control.columns = 1;
    type_item.control.setRect(QRectF{0, 0, 60, 105});

    auto amount_item = makeControl(
        std::get<2>(Metadata::controls),
        amount,
        parent,
        context,
        doc,
        portFactory);
    amount_item.root.setPos(tMarg, 40);
    amount_item.control.setPos(0, cMarg);

    auto freq_item = makeControl(
        std::get<3>(Metadata::controls),
        freq,
        parent,
        context,
        doc,
        portFactory);
    freq_item.root.setPos(140, 0);
    freq_item.control.setPos(0, cMarg);

    auto cutoff_item = makeControl(
        std::get<4>(Metadata::controls),
        cutoff,
        parent,
        context,
        doc,
        portFactory);
    cutoff_item.root.setPos(140, 40);
    cutoff_item.control.setPos(0, cMarg);

    auto beta_item = makeControl(
        std::get<5>(Metadata::controls),
        beta,
        parent,
        context,
        doc,
        portFactory);
    beta_item.root.setPos(140, 80);
    beta_item.control.setPos(0, cMarg);

    auto rate_item = makeControl(
        std::get<6>(Metadata::controls),
        rate,
        parent,
        context,
        doc,
        portFactory);
    rate_item.root.setPos(tMarg, h);
    rate_item.control.setPos(0, tMarg);

    auto ms_item = makeControl(
        std::get<7>(Metadata::controls),
        ms,
        parent,
        context,
        doc,
        portFactory);
    ms_item.root.setPos(55, h);
    ms_item.control.setPos(0, cMarg);

    auto quant_item = makeControl(
        std::get<8>(Metadata::controls),
        quantif,
        parent,
        context,
        doc,
        portFactory);
    quant_item.root.setPos(130, h);
    quant_item.control.setPos(cMarg, cMarg);
  }
};
}
