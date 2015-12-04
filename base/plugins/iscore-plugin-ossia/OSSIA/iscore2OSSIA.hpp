#pragma once
#include <API/Headers/Editor/Curve.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentLinear.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentPower.h>
#include <API/Headers/Editor/TimeValue.h>
#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <State/Expression.hpp>
#include <QStringList>
#include <memory>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <State/Value.hpp>
#include <iscore_plugin_ossia_export.h>
class StateModel;
class DeviceList;
namespace OSSIA {
class Address;
class Device;
class Expression;
class Message;
class Node;
class State;
class Value;
}  // namespace OSSIA
namespace iscore {
struct FullAddressSettings;
struct Message;
}  // namespace iscore

namespace iscore
{
namespace convert
{
// Gets a node from an address in a device.
// Creates it if necessary.
//// Device-related functions
// OSSIA::Node* might be null.
ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Node* findNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::Node> findNodeFromPath(
        const QStringList& path,
        std::shared_ptr<OSSIA::Device> dev);

// OSSIA::Node* won't be null.
ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Node* getNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Node* createNodeFromPath(
        const QStringList& path,
        OSSIA::Device* dev);

ISCORE_PLUGIN_OSSIA_EXPORT void createOSSIAAddress(
        const iscore::FullAddressSettings& settings,
        OSSIA::Node* node);
ISCORE_PLUGIN_OSSIA_EXPORT void updateOSSIAAddress(
        const iscore::FullAddressSettings& settings,
        const std::shared_ptr<OSSIA::Address>& addr);
ISCORE_PLUGIN_OSSIA_EXPORT void removeOSSIAAddress(
        OSSIA::Node*); // Keeps the Node.
ISCORE_PLUGIN_OSSIA_EXPORT void updateOSSIAValue(
        const iscore::ValueImpl& data,
        OSSIA::Value& val);

ISCORE_PLUGIN_OSSIA_EXPORT OSSIA::Value* toOSSIAValue(
        const iscore::Value&);

ISCORE_PLUGIN_OSSIA_EXPORT void setValue(
        OSSIA::Address& addr,
        const iscore::Value& val);

//// Other conversions
ISCORE_PLUGIN_OSSIA_EXPORT inline OSSIA::TimeValue time(const TimeValue& t)
{
    return t.isInfinite()
            ? OSSIA::Infinite
            : OSSIA::TimeValue{t.msec()};
}

ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::State> state(
        std::shared_ptr<OSSIA::State> ossia_state,
        const StateModel& iscore_state,
        const DeviceList& deviceList);
ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::State> state(
        const StateModel& iscore_state,
        const DeviceList& deviceList);


ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::Message> message(
        const iscore::Message& mess,
        const DeviceList&);

ISCORE_PLUGIN_OSSIA_EXPORT std::shared_ptr<OSSIA::Expression> expression(
        const iscore::Expression& expr,
        const DeviceList&);


template<typename X_T, typename Y_T, typename XScaleFun, typename YScaleFun, typename Segments>
std::shared_ptr<OSSIA::CurveAbstract> curve(
        XScaleFun scale_x,
        YScaleFun scale_y,
        const Segments& segments)
{
    auto curve = OSSIA::Curve<X_T, Y_T>::create();
    if(segments[0].start.x() == 0.)
    {
        curve->setInitialValue(scale_y(segments[0].start.y()));
    }

    for(const auto& iscore_segment : segments)
    {
        if(iscore_segment.type == LinearCurveSegmentData::key())
        {
            curve->addPoint(
                        OSSIA::CurveSegmentLinear<Y_T>::create(curve),
                        scale_x(iscore_segment.end.x()),
                        scale_y(iscore_segment.end.y()));
        }
        else if(iscore_segment.type == PowerCurveSegmentData::key())
        {
            auto val = iscore_segment.specificSegmentData.template value<PowerCurveSegmentData>();

            if(val.gamma == 12.05)
            {
                curve->addPoint(
                            OSSIA::CurveSegmentLinear<Y_T>::create(curve),
                            scale_x(iscore_segment.end.x()),
                            scale_y(iscore_segment.end.y()));
            }
            else
            {
                auto powSegment = OSSIA::CurveSegmentPower<Y_T>::create(curve);
                powSegment->setPower(12.05 - val.gamma); // TODO document this somewhere.

                curve->addPoint(
                            powSegment,
                            scale_x(iscore_segment.end.x()),
                            scale_y(iscore_segment.end.y()));
            }
        }
    }
    return curve;
}

}
}

