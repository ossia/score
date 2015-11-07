#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
#include <API/Headers/Editor/Value.h>
#include <API/Headers/Editor/Domain.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>

#include <API/Headers/Editor/TimeValue.h>
#include <Process/TimeValue.hpp>
#include <State/Message.hpp>
#include <State/Expression.hpp>

#include <API/Headers/Editor/CurveSegment/CurveSegmentLinear.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentPower.h>
#include <API/Headers/Editor/Curve.h>

#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>

#include <Device/Protocol/DeviceList.hpp>
#include <Process/State/MessageNode.hpp>
namespace iscore
{
namespace convert
{
// Gets a node from an address in a device.
// Creates it if necessary.
//// Device-related functions
// OSSIA::Node* might be null.
OSSIA::Node* findNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
std::shared_ptr<OSSIA::Node> findNodeFromPath(
        const QStringList& path,
        std::shared_ptr<OSSIA::Device> dev);

// OSSIA::Node* won't be null.
OSSIA::Node* getNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
OSSIA::Node* createNodeFromPath(
        const QStringList& path,
        OSSIA::Device* dev);

void createOSSIAAddress(
        const iscore::FullAddressSettings& settings,
        OSSIA::Node* node);
void updateOSSIAAddress(
        const iscore::FullAddressSettings& settings,
        const std::shared_ptr<OSSIA::Address>& addr);
void removeOSSIAAddress(
        OSSIA::Node*); // Keeps the Node.
void updateOSSIAValue(
        const iscore::ValueImpl& data,
        OSSIA::Value& val);

OSSIA::Value* toValue(
        const iscore::Value&);

//// Other conversions
inline OSSIA::TimeValue time(const TimeValue& t)
{
    return t.isInfinite()
            ? OSSIA::Infinite
            : OSSIA::TimeValue{t.msec()};
}

std::shared_ptr<OSSIA::State> state(std::shared_ptr<OSSIA::State> ossia_state,
        const MessageNode& iscore_state,
        const DeviceList& deviceList);


std::shared_ptr<OSSIA::Message> message(
        const iscore::Message& mess,
        const DeviceList&);

std::shared_ptr<OSSIA::Expression> expression(
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
        if(iscore_segment.type == "Linear")
        {
            curve->addPoint(
                        OSSIA::CurveSegmentLinear<Y_T>::create(curve),
                        scale_x(iscore_segment.end.x()),
                        scale_y(iscore_segment.end.y()));
        }
        else if(iscore_segment.type == "Power")
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

