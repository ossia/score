#pragma once
#include <ossia/detail/logger.hpp>
#include <ossia/network/value/format_value.hpp>

#include <dno/DeNoiser.hpp>

namespace Nodes
{

struct NoiseFilter
{
  NoiseFilter()
      : dno_i{}
      , dno_v{
            dno::DeNoiser<double>{}, dno::DeNoiser<double>{}, dno::DeNoiser<double>{},
            dno::DeNoiser<double>{}}
  {
  }

  dno::DeNoiser<double> dno_i;
  std::vector<dno::DeNoiser<double>> dno_v;

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
            nFilt->dno_v[0](t[0]), nFilt->dno_v[1](t[1]), nFilt->dno_v[2](t[2]));
      }
      ossia::value operator()(const ossia::vec4f& t) const
      {
        return ossia::make_vec(
            nFilt->dno_v[0](t[0]), nFilt->dno_v[1](t[1]), nFilt->dno_v[2](t[2]),
            nFilt->dno_v[3](t[3]));
      }

      ossia::value operator()(const std::vector<ossia::value>& t) const
      {
        std::vector<ossia::value> ret = t;
        while(nFilt->dno_v.size() < ret.size())
          nFilt->dno_v.push_back(nFilt->dno_v.front());

        for(std::size_t k = 0; k < ret.size(); k++)
          ret[k] = (float)nFilt->dno_v[k](ossia::convert<float>(ret[k]));

        return ret;
      }

      ossia::value operator()(const ossia::value_map_type& t) const
      {
        // FIXME
        return t;
      }

      NoiseFilter* nFilt{};

      explicit vis(NoiseFilter* parent)
          : nFilt{parent}
      {
      }
    };

    try
    {
      return ossia::apply(vis{this}, val.v);
    }
    catch(std::exception& e)
    {
      ossia::logger().error("{}", e.what());
    }
    catch(...)
    {
      ossia::logger().error("error");
    }

    return val;
  }

  void set_amount(float amt)
  {
    dno_i.set_amount(amt);
    for(auto& f : dno_v)
      f.set_amount(amt);
  }

  void set_type(const dno::type& t = dno::OneEuro)
  {
    dno_i.set_type(t);
    for(auto& f : dno_v)
      f.set_type(t);
  }

  void set_freq(double freq) // 1e & LP
  {
    dno_i.set_freq(freq);
    for(auto& f : dno_v)
      f.set_freq(freq);
  }

  void set_cutoff(double cutoff) // 1e & LP
  {
    dno_i.set_cutoff(cutoff);
    for(auto& f : dno_v)
      f.set_cutoff(cutoff);
  }

  void set_beta(double beta) // 1e only
  {
    dno_i.set_1e_beta(beta);
    for(auto& f : dno_v)
      f.set_1e_beta(beta);
  }
};

class NoiseState
{
public:
  ossia::value filter(
      const ossia::value& value, dno::type type, float amount, float freq, float cutoff,
      float beta)
  {
    if(type != prev_type)
    {
      nf.set_type(type);
      prev_type = type;

      // Reset the values to make sure they get updated once the filter has been changed
      prev_amount = amount;
      prev_freq = freq;
      prev_cutoff = cutoff;
      prev_beta = beta;

      nf.set_amount(amount);
      nf.set_freq(freq);
      nf.set_cutoff(cutoff);
      nf.set_beta(beta);
    }
    if(amount != prev_amount)
    {
      nf.set_amount(amount);
      prev_amount = amount;
    }
    if(freq != prev_freq)
    {
      nf.set_freq(freq);
      prev_freq = freq;
    }
    if(cutoff != prev_cutoff)
    {
      nf.set_cutoff(cutoff);
      prev_cutoff = cutoff;
    }
    if(beta != prev_beta)
    {
      nf.set_beta(beta);
      prev_beta = beta;
    }

    return nf.filter(value);
  }

  ossia::value last;

private:
  dno::type prev_type{dno::OneEuro};
  double prev_amount{1. / SCALED_AMOUNT}, prev_freq{INIT_FREQ}, prev_cutoff{INIT_CUTOFF},
      prev_beta{INIT_BETA};

  NoiseFilter nf{};
};
}
