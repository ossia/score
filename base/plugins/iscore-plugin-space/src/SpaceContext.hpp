#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <Device/Node/DeviceNode.hpp>

class SpaceModel;
class DeviceDocumentPlugin;
namespace Space
{
struct AreaContext
{
    const iscore::DocumentContext& doc;
    const SpaceModel& space;
    DeviceDocumentPlugin& devices;
};
}
