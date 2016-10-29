#pragma once
#include <ossia/editor/dataspace/dataspace.hpp>
namespace Scenario
{
class ProcessModel;
namespace Command
{
class AddLayerModelToSlot;
class AddOnlyProcessToConstraint;
}
}
namespace Curve
{
class Model;
class PointArraySegment;
}
namespace Explorer
{
class DeviceExplorerModel;
}

namespace Recording
{
struct RecordData
{
        RecordData(
                Scenario::Command::AddOnlyProcessToConstraint* cmd_proc,
                Scenario::Command::AddLayerModelToSlot* cmd_lay,
                Curve::Model& cm,
                Curve::PointArraySegment& seg,
                ossia::unit_t u):
            addProcCmd{cmd_proc},
            addLayCmd{cmd_lay},
            curveModel{cm},
            segment{seg},
            unit{u}
        { }

        Scenario::Command::AddOnlyProcessToConstraint* addProcCmd{};
        Scenario::Command::AddLayerModelToSlot* addLayCmd{};

        Curve::Model& curveModel;
        Curve::PointArraySegment& segment;

        ossia::unit_t unit;
};
}
