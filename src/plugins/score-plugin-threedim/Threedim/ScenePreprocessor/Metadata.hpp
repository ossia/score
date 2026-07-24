#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Gfx::ScenePreprocessor
{
class Model;
}

PROCESS_METADATA(
    , Gfx::ScenePreprocessor::Model, "a8f2c6d0-1b4e-4c7a-9d3f-5e8b2c1a7f0d",
    "scenepreprocessor",                             // Internal name
    "Scene Preprocessor",                            // Pretty name
    Process::ProcessCategory::Visual,             // Category
    "Visuals/3D/Scene",                           // Category
    "Flattens a scene_spec hierarchy into a GPU-resident geometry_spec", // Description
    "ossia team",                                 // Author
    (QStringList{"gfx", "scene", "geometry", "3d"}), // Tags
    {},                                           // Inputs
    {},                                           // Outputs
    QUrl{},                                       // Doc url
    Process::ProcessFlags::SupportsAll            // Flags
)
