#pragma once
#include <Curve/CurveModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Address.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QList>

namespace score
{
class CommandStackFacade;
struct DocumentContext;
}

namespace Scenario
{
class IntervalModel;
namespace Command
{
void InterpolateStates(
    const std::vector<const IntervalModel*>&,
    const score::CommandStackFacade&);
}

struct value_size
{
  template <std::size_t N>
  std::size_t operator()(const std::array<float, N>& arr)
  {
    return N;
  }

  std::size_t operator()(const std::vector<ossia::value>& arr)
  {
    return arr.size();
  }

  std::size_t operator()() { return 0; }

  template <typename T>
  std::size_t operator()(const T& other)
  {
    throw std::runtime_error("invalid type, should not happen");
  }
};

struct get_curve_domain
{
  const State::AddressAccessor& address;
  const ossia::destination_index& idx;
  const Device::Node& rootNode;

  template <std::size_t N>
  Curve::CurveDomain operator()(
      const std::array<float, N>& start,
      const std::array<float, N>& end)
  {
    SCORE_ASSERT(!idx.empty());
    const auto i = idx[0];
    Curve::CurveDomain d{start[i], end[i]};

    if (auto node = Device::try_getNodeFromAddress(rootNode, address.address))
    {
      const Device::AddressSettings& as = node->get<Device::AddressSettings>();

      if (auto dom = as.domain.get().v.target<ossia::vecf_domain<N>>())
      {
        if (auto min_v = dom->min[i])
          d.min = std::min(d.min, (double)*min_v);
        if (auto max_v = dom->max[i])
          d.max = std::max(d.max, (double)*max_v);
      }
    }

    return d;
  }

  Curve::CurveDomain operator()(
      const std::vector<ossia::value>& start,
      const std::vector<ossia::value>& end)
  {
    SCORE_ASSERT(!idx.empty());
    const auto i = (std::size_t)idx[0];
    Curve::CurveDomain d{ossia::convert<double>(start[i]),
                         ossia::convert<double>(end[i])};

    if (auto node = Device::try_getNodeFromAddress(rootNode, address.address))
    {
      const Device::AddressSettings& as = node->get<Device::AddressSettings>();

      if (auto dom = as.domain.get().v.target<ossia::vector_domain>())
      {
        if (dom->min.size() > i)
          d.min = std::min(d.min, ossia::convert<double>(dom->min[i]));
        if (dom->max.size() > i)
          d.max = std::max(d.max, ossia::convert<double>(dom->max[i]));
      }
    }

    return d;
  }

  template <typename T>
  Curve::CurveDomain operator()(const T& start, const T& end)
  {
    Curve::CurveDomain d{(double)start, (double)end};

    if (auto node = Device::try_getNodeFromAddress(rootNode, address.address))
    {
      const Device::AddressSettings& as = node->get<Device::AddressSettings>();

      d.refine(as.domain.get());
    }

    return d;
  }

  Curve::CurveDomain operator()(ossia::impulse start, ossia::impulse end)
  {
    return {};
  }

  Curve::CurveDomain
  operator()(const std::string& start, const std::string& end)
  {
    return {};
  }

  template <typename T, typename U>
  Curve::CurveDomain operator()(const T& start, const U& end)
  {
    return {};
  }
};

struct get_start_end
{
  const ossia::destination_index& idx;

  template <std::size_t N>
  std::pair<double, double> operator()(
      const std::array<float, N>& start,
      const std::array<float, N>& end)
  {
    SCORE_ASSERT(!idx.empty());
    const auto i = idx[0];
    return {(double)start[i], (double)end[i]};
  }

  std::pair<double, double> operator()(
      const std::vector<ossia::value>& start,
      const std::vector<ossia::value>& end)
  {
    SCORE_ASSERT(!idx.empty());
    const auto i = idx[0];
    return {ossia::convert<double>(start[i]), ossia::convert<double>(end[i])};
  }

  template <typename T>
  std::pair<double, double> operator()(const T& start, const T& end)
  {
    return {(double)start, (double)end};
  }

  std::pair<double, double>
  operator()(ossia::impulse start, ossia::impulse end)
  {
    return {0., 1.};
  }

  std::pair<double, double>
  operator()(const std::string& start, const std::string& end)
  {
    return {0., 1.};
  }

  template <typename T, typename U>
  std::pair<double, double> operator()(const T& start, const U& end)
  {
    return {0., 1.};
  }
};
}
