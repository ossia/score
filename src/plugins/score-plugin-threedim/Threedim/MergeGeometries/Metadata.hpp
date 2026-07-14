#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::MergeGeometries
{
class Model;
}

PROCESS_METADATA(
    , Gfx::MergeGeometries::Model, "e8f7a6b5-c4d3-4e2f-1a0b-9c8d7e6f5a4b",
    "mergegeometries",
    "Merge Geometries",
    Process::ProcessCategory::Visual,
    "Visuals/3D/Scene",
    "Concatenate N upstream geometry_specs into one for a single downstream renderer",
    "ossia team",
    (QStringList{"gfx", "geometry", "merge", "3d", "scene"}),
    {},
    {},
    QUrl{},
    Process::ProcessFlags::SupportsAll
)
