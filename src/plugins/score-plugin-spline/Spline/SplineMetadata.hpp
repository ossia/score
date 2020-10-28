#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

namespace Spline
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    Spline::ProcessModel,
    "931a6356-2fca-4f3b-9c65-2de051ef4903",
    "Spline",
    "Automation (XY)",
    Process::ProcessCategory::Automation,
    "Automations",
    "Automation following a 2D curve",
    "ossia score",
    (QStringList{"Curve", "Automation", "2D"}),
    {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
