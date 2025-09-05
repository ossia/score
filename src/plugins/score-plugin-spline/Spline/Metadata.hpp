#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

namespace Spline
{
class ProcessModel;
}

PROCESS_METADATA(
    , Spline::ProcessModel, "931a6356-2fca-4f3b-9c65-2de051ef4903", "Spline",
    "2D Automation", Process::ProcessCategory::Automation, "Automations",
    "Automation following a 2D curve", "ossia score",
    (QStringList{"Curve", "Automation", "2D"}), {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    QUrl("https://ossia.io/score-docs/processes/2Dspline.html#2d-spline-x-y-automation"),
    Process::ProcessFlags::SupportsTemporal)
