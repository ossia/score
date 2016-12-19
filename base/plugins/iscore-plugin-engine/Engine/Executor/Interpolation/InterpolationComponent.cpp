#include "InterpolationComponent.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/iscore2OSSIA.hpp>

#include <Curve/CurveModel.hpp>

#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/value/value_conversion.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>

namespace Interpolation
{
namespace Executor
{
Component::Component(
    ::Engine::Execution::ConstraintElement& parentConstraint,
    ::Interpolation::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : ::Engine::Execution::
          ProcessComponent_T<Interpolation::ProcessModel, ossia::automation>{parentConstraint,
                                                                             element,
                                                                             ctx,
                                                                             id,
                                                                             "InterpolationComponent",
                                                                             parent}
    , m_deviceList{ctx.devices.list()}
{
  if (auto dest = Engine::iscore_to_ossia::makeDestination(
          m_deviceList, process().address()))
  {
    m_ossia_process = new ossia::automation{
        *dest, on_curveChanged(dest->value.get().getValueType())};
  }
}

template <typename Y_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl(
    double min, double max, double start, double end)
{
  using namespace ossia;

  auto scale_x = [=](double val) -> double { return val; };
  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    if (start < end)
    {
      return Engine::iscore_to_ossia::curve<double, Y_T>(
          scale_x,
          [=](double val) -> Y_T {
            return ossia::easing::ease{}(min, max, val);
          },
          segt_data, {});
    }
    else if (start == end)
    {
      auto curve = std::make_shared<ossia::curve<double, Y_T>>();
      curve->setInitialPointAbscissa(0);
      curve->setInitialPointOrdinate(start);
      return curve;
    }
    else // start > end
    {
      return Engine::iscore_to_ossia::
          curve<double, Y_T>( // ossia::easing::ease
              scale_x,
              [=](double val) -> Y_T { return max - val * (max - min); },
              segt_data, {});
    }
  }
  else
  {
    return {};
  }

  return {};
}

ossia::behavior Component::on_curveChanged(ossia::val_type type)
{
  auto start = Engine::iscore_to_ossia::toOSSIAValue(process().start());
  auto end = Engine::iscore_to_ossia::toOSSIAValue(process().end());

  auto source_unit = process().sourceUnit();
  auto dest_unit = process().address().qualifiers.get().unit;
  if (source_unit != dest_unit)
  {
    start = ossia::convert(start, source_unit, dest_unit);
    end = ossia::convert(end, source_unit, dest_unit);
  }

  switch (type)
  {
    case ossia::val_type::INT:
    {
      const auto start_v = ossia::convert<double>(start);
      const auto end_v = ossia::convert<double>(end);
      return on_curveChanged_impl<int>(
          std::min(start_v, end_v), std::max(start_v, end_v), start_v, end_v);
    }
    case ossia::val_type::FLOAT:
    {
      const auto start_v = ossia::convert<double>(start);
      const auto end_v = ossia::convert<double>(end);
      return on_curveChanged_impl<float>(
          std::min(start_v, end_v), std::max(start_v, end_v), start_v, end_v);
    }
    case ossia::val_type::TUPLE:
    {
      // First check the number of curves.
      const auto& start_v = ossia::convert<std::vector<ossia::value>>(start);
      const auto& end_v = ossia::convert<std::vector<ossia::value>>(end);

      const int n_curves = std::min(start_v.size(), end_v.size());
      std::vector<ossia::behavior> t;
      for (int i = 0; i < n_curves; i++)
      {
        // Take the type of the value of the start state.
        switch (start_v[i].getType())
        {
          case ossia::val_type::INT:
          {
            int start_v_i = ossia::convert<int>(start_v[i]);
            int end_v_i = ossia::convert<int>(end_v[i]);
            t.push_back(on_curveChanged_impl<int>(
                std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
                start_v_i, end_v_i));
            break;
          }
          case ossia::val_type::FLOAT:
          {
            float start_v_i = ossia::convert<float>(start_v[i]);
            float end_v_i = ossia::convert<float>(end_v[i]);
            t.push_back(on_curveChanged_impl<float>(
                std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
                start_v_i, end_v_i));
            break;
          }
          default:
            // Default case : we use a constant value.
            t.push_back(std::make_shared<ossia::constant_curve>(start));
        }
      }
      return t;
    }

    case ossia::val_type::VEC2F:
    {
      // First check the number of curves.
      const constexpr int n_curves = 2;
      const auto& start_v = ossia::convert<std::array<float, n_curves>>(start);
      const auto& end_v = ossia::convert<std::array<float, n_curves>>(end);

      std::vector<ossia::behavior> t;

      for (int i = 0; i < n_curves; i++)
      {
        float start_v_i = start_v[i];
        float end_v_i = end_v[i];
        t.push_back(on_curveChanged_impl<float>(
            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
            start_v_i, end_v_i));
      }
      return t;
    }
    case ossia::val_type::VEC3F:
    {
      // First check the number of curves.
      const constexpr int n_curves = 3;
      const auto& start_v = ossia::convert<std::array<float, n_curves>>(start);
      const auto& end_v = ossia::convert<std::array<float, n_curves>>(end);

      std::vector<ossia::behavior> t;

      for (int i = 0; i < n_curves; i++)
      {
        float start_v_i = start_v[i];
        float end_v_i = end_v[i];
        t.push_back(on_curveChanged_impl<float>(
            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
            start_v_i, end_v_i));
      }
      return t;
    }
    case ossia::val_type::VEC4F:
    {
      // First check the number of curves.
      const constexpr int n_curves = 4;
      const auto& start_v = ossia::convert<std::array<float, n_curves>>(start);
      const auto& end_v = ossia::convert<std::array<float, n_curves>>(end);

      std::vector<ossia::behavior> t;

      for (int i = 0; i < n_curves; i++)
      {
        float start_v_i = start_v[i];
        float end_v_i = end_v[i];
        t.push_back(on_curveChanged_impl<float>(
            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
            start_v_i, end_v_i));
      }
      return t;
    }
    default:
      return std::make_shared<ossia::constant_curve>(start);
  }
}
}
}
