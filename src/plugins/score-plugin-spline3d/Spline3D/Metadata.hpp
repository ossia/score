#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QString>

namespace Spline3D
{
class ProcessModel;
}

PROCESS_METADATA(
    ,
    Spline3D::ProcessModel,
    "32d8937c-d1d6-48f9-8b95-255b357acc71",
    "Spline",
    "3D Automation",
    Process::ProcessCategory::Automation,
    "Automations",
    "Automation following a 3D curve",
    "ossia score",
    (QStringList{"Curve", "Automation", "3D"}),
    {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
