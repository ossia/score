#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::FlattenedSceneFilter
{
class Model;
}

PROCESS_METADATA(
    , Gfx::FlattenedSceneFilter::Model, "7a1b3c5d-2e4f-4a6b-8c9d-1e2f3a4b5c6e",
    "flattenedscenefilter",
    "Flattened Scene Filter",
    Process::ProcessCategory::Visual,
    "Visuals/3D/Scene",
    "Filter a flattened scene by tag or material index, per pass",
    "ossia team",
    (QStringList{"gfx", "scene", "filter", "3d"}),
    {},
    {},
    QUrl{},
    Process::ProcessFlags::SupportsAll
)
