#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::SceneFilter
{
class Model;
}

PROCESS_METADATA(
    , Gfx::SceneFilter::Model, "c2d8e9a4-3f5b-4e7c-9a1d-6b7e8c2f1a3b",
    "scenefilter",
    "Scene Filter",
    Process::ProcessCategory::Visual,
    "Visuals/3D/Scene",
    "Filter the hierarchy of a scene_spec (visibility, layers, names)",
    "ossia team",
    (QStringList{"gfx", "scene", "filter", "3d", "hierarchy"}),
    {},
    {},
    QUrl{},
    Process::ProcessFlags::SupportsAll
)
