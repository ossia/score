#include "InterpolationComponent.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/CurveConversion.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/Executor/ConstraintElement.hpp>

#include <Curve/CurveModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <ossia/ossia.hpp>

namespace Interpolation
{
namespace Executor
{
Component::Component(
        ::Engine::Execution::ConstraintElement& parentConstraint,
        ::Interpolation::ProcessModel& element,
        const ::Engine::Execution::Context& ctx,
        const Id<iscore::Component>& id,
        QObject *parent):
    ::Engine::Execution::ProcessComponent_T<Interpolation::ProcessModel, ossia::automation>{
          parentConstraint, element, ctx, id, "InterpolationComponent", parent},
    m_deviceList{ctx.devices.list()}
{
    recreate();
}

template<typename Y_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl(
        double min, double max, double start, double end)
{
    using namespace ossia;

    auto scale_x = [=] (double val) -> double { return val; };
    auto segt_data = process().curve().sortedSegments();
    if(segt_data.size() != 0)
    {
        if(start < end)
        {
            return Engine::iscore_to_ossia::curve<double, Y_T>(
                  scale_x, [=] (double val) -> Y_T { return min + val * (max - min); }, segt_data, {});
        }
        else if(start == end)
        {
            auto curve = std::make_shared<ossia::curve<double, Y_T>>();
            curve->setInitialPointAbscissa(0);
            curve->setInitialPointOrdinate(start);
            return curve;
        }
        else // start > end
        {
            return Engine::iscore_to_ossia::curve<double, Y_T>(
                  scale_x, [=] (double val) -> Y_T { return max - val * (max - min); }, segt_data, {});
        }
    }
    else
    {
        return {};
    }

    return {};
}

ossia::value Component::on_curveChanged(ossia::val_type type)
{
    const auto start = process().start();
    const auto end = process().end();

    switch(type)
    {
        case ossia::val_type::INT:
        {
            const auto start_v = State::convert::value<double>(start);
            const auto end_v = State::convert::value<double>(end);
            return ossia::Behavior{on_curveChanged_impl<int>(std::min(start_v, end_v), std::max(start_v, end_v), start_v, end_v)};
        }
        case ossia::val_type::FLOAT:
        {
            const auto start_v = State::convert::value<double>(start);
            const auto end_v = State::convert::value<double>(end);
            return ossia::Behavior{on_curveChanged_impl<float>(std::min(start_v, end_v), std::max(start_v, end_v), start_v, end_v)};
        }
        case ossia::val_type::TUPLE:
        {
            // First check the number of curves.
            const auto& start_v = State::convert::value<State::tuple_t>(start);
            const auto& end_v = State::convert::value<State::tuple_t>(end);

            int n_curves = std::min(start_v.size(), end_v.size());
            ossia::Tuple t;
            for(int i = 0; i < n_curves; i++)
            {
                // Take the type of the value of the start state.
                switch(start_v[i].which())
                {
                    case State::ValueType::Int:
                    {
                        int start_v_i = State::convert::value<int>(start_v[i]);
                        int end_v_i = State::convert::value<int>(end_v[i]);
                        t.value.push_back(
                                    ossia::Behavior{
                                        on_curveChanged_impl<int>(
                                            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
                                            start_v_i, end_v_i)});
                        break;
                    }
                    case State::ValueType::Float:
                    {
                        float start_v_i = State::convert::value<float>(start_v[i]);
                        float end_v_i = State::convert::value<float>(end_v[i]);
                        t.value.push_back(
                                    ossia::Behavior{
                                        on_curveChanged_impl<float>(
                                            std::min(start_v_i, end_v_i), std::max(start_v_i, end_v_i),
                                            start_v_i, end_v_i)});
                        break;
                    }
                    default:
                        // Default case : we use a constant value.
                        t.value.push_back(
                              ossia::Behavior{
                                std::make_shared<ossia::constant_curve>(
                                  Engine::iscore_to_ossia::toOSSIAValue(start))});
                }

            }
            return t;
        }

        case ossia::val_type::VEC2F:
        {
            // First check the number of curves.
            const constexpr int n_curves = 2;
            const auto& start_v = State::convert::value<std::array<float, n_curves>>(start);
            const auto& end_v = State::convert::value<std::array<float, n_curves>>(end);

            ossia::Tuple t;

            for(int i = 0; i < n_curves; i++)
            {
                float start_v_i = start_v[i];
                float end_v_i = end_v[i];
                t.value.push_back(
                            ossia::Behavior{
                                on_curveChanged_impl<float>(
                                    std::min(start_v_i, end_v_i),
                                    std::max(start_v_i, end_v_i),
                                    start_v_i, end_v_i)});
            }
            return t;
        }
        case ossia::val_type::VEC3F:
        {
            // First check the number of curves.
            const constexpr int n_curves = 3;
            const auto& start_v = State::convert::value<std::array<float, n_curves>>(start);
            const auto& end_v = State::convert::value<std::array<float, n_curves>>(end);

            ossia::Tuple t;

            for(int i = 0; i < n_curves; i++)
            {
                float start_v_i = start_v[i];
                float end_v_i = end_v[i];
                t.value.push_back(
                            ossia::Behavior{
                                on_curveChanged_impl<float>(
                                    std::min(start_v_i, end_v_i),
                                    std::max(start_v_i, end_v_i),
                                    start_v_i, end_v_i)});
            }
            return t;
        }
        case ossia::val_type::VEC4F:
        {
            // First check the number of curves.
            const constexpr int n_curves = 4;
            const auto& start_v = State::convert::value<std::array<float, n_curves>>(start);
            const auto& end_v = State::convert::value<std::array<float, n_curves>>(end);

            ossia::Tuple t;

            for(int i = 0; i < n_curves; i++)
            {
                float start_v_i = start_v[i];
                float end_v_i = end_v[i];
                t.value.push_back(
                            ossia::Behavior{
                                on_curveChanged_impl<float>(
                                    std::min(start_v_i, end_v_i),
                                    std::max(start_v_i, end_v_i),
                                    start_v_i, end_v_i)});
            }
            return t;
        }
        default:
            return ossia::Behavior{
                    std::make_shared<ossia::constant_curve>(
                      Engine::iscore_to_ossia::toOSSIAValue(start))};
    }
}

void Component::recreate()
{
    m_ossia_process = nullptr;

    // Add the real address
    auto address = Engine::iscore_to_ossia::findAddress(
          m_deviceList,
          process().address().address);

    if(address)
    {
        auto autom = new ossia::automation(
                    *address,
              on_curveChanged(address->getValueType()));
        autom->setUnit(process().address().qualifiers.unit);
        m_ossia_process = autom;

    }
}

}
}
