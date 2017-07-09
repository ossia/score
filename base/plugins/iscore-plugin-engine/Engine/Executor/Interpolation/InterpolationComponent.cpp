#include "InterpolationComponent.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <ossia/network/base/device.hpp>

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
    ::Engine::Execution::ConstraintComponent& parentConstraint,
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
{
  m_ossia_process = std::make_shared<ossia::automation>();

  con(element, &Interpolation::ProcessModel::addressChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Interpolation::ProcessModel::startChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Interpolation::ProcessModel::endChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Interpolation::ProcessModel::tweenChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Interpolation::ProcessModel::curveChanged,
      this, [this] () { this->recompute(); });

  recompute();
}

void Component::recompute()
{
  auto dest = Engine::iscore_to_ossia::makeDestination(
            system().devices.list(), process().address());
  if(dest)
  {
    auto& d = *dest;
    auto addressType = d.address().get_value_type();
    auto curve =  process().tween()
                  ? on_curveChanged(addressType, d)
                  : on_curveChanged(addressType, {});

    if(curve)
    {
      system().executionQueue.enqueue(
            [proc=std::dynamic_pointer_cast<ossia::automation>(m_ossia_process)
            ,curve
            ,d_=*dest]
      {
        proc->set_destination(std::move(d_));
        proc->set_behavior(curve);
      });
      return;
    }
  }

  // If something did not work out
  system().executionQueue.enqueue(
        [proc=std::dynamic_pointer_cast<ossia::automation>(m_ossia_process)]
  {
    proc->clean();
  });
}

template <typename Y_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl(
    double min, double max, double start, double end, const optional<ossia::Destination>& d)
{
  using namespace ossia;

  auto scale_x = [](double val) -> double { return val; };
  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    if(d)
    {
      return Engine::iscore_to_ossia::scalable_curve<double, Y_T>(
            min, max, end, scale_x, segt_data, *d);
    }
    else
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
        curve->set_x0(0);
        curve->set_y0(start);
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
  }
  else
  {
    return {};
  }

  return {};
}

ossia::behavior Component::on_curveChanged(
    ossia::val_type type,
    const optional<ossia::Destination>& d)
{
  auto start = process().start();
  auto end = process().end();

  auto source_unit = process().sourceUnit();
  auto dest_unit = process().address().qualifiers.get().unit;
  if (source_unit.get() != dest_unit)
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
          std::min(start_v, end_v), std::max(start_v, end_v), start_v, end_v, d);
    }
    case ossia::val_type::FLOAT:
    {
      const auto start_v = ossia::convert<double>(start);
      const auto end_v = ossia::convert<double>(end);
      return on_curveChanged_impl<float>(
          std::min(start_v, end_v), std::max(start_v, end_v), start_v, end_v, d);
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
        auto dest = d;
        if(dest)
        {
          dest->index.push_back(i);
        }
        // Take the type of the value of the start state.
        switch (start_v[i].getType())
        {
          case ossia::val_type::INT:
          {
            int start_v_i = ossia::convert<int>(start_v[i]);
            int end_v_i = ossia::convert<int>(end_v[i]);
            t.push_back(on_curveChanged_impl<int>(
                std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
                start_v_i, end_v_i, dest));
            break;
          }
          case ossia::val_type::FLOAT:
          {
            float start_v_i = ossia::convert<float>(start_v[i]);
            float end_v_i = ossia::convert<float>(end_v[i]);
            t.push_back(on_curveChanged_impl<float>(
                std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
                start_v_i, end_v_i, dest));
            break;
          }
          default:
            // TODO handle recursive case
            // Default case : we use a constant value.
            t.push_back(ossia::curve_ptr(std::make_shared<ossia::constant_curve>(start)));
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
        auto dest = d;
        if(dest)
        {
          dest->index.push_back(i);
        }

        float start_v_i = start_v[i];
        float end_v_i = end_v[i];
        t.push_back(on_curveChanged_impl<float>(
            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
            start_v_i, end_v_i, dest));
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
        auto dest = d;
        if(dest)
        {
          dest->index.push_back(i);
        }

        float start_v_i = start_v[i];
        float end_v_i = end_v[i];
        t.push_back(on_curveChanged_impl<float>(
            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
            start_v_i, end_v_i, dest));
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
        auto dest = d;
        if(dest)
        {
          dest->index.push_back(i);
        }

        float start_v_i = start_v[i];
        float end_v_i = end_v[i];
        t.push_back(on_curveChanged_impl<float>(
            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
            start_v_i, end_v_i, dest));
      }
      return t;
    }
    default:
      return ossia::curve_ptr{std::make_shared<ossia::constant_curve>(start)};
  }
}
}
}
