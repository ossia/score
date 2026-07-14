#pragma once
#include <Threedim/TinyObj.hpp>

#include <string_view>

namespace Threedim
{

// vcglib bridges. Load a 3D file via vcg::tri::io::Importer<Mesh>::Open
// and convert the loaded mesh into the same flat float_vec + mesh record
// format that TinyObjFromFile / PlyFromFile produce, so downstream
// `sceneStateFromMeshes` (or GeometryLoader's `rebuild_geometry`)
// consumes them uniformly.
//
// Adds STL and OFF support (the two remaining generally-useful formats
// vcglib offers that we weren't already covering via tinyobj / miniply).
// COLLADA (DAE) is a candidate for a follow-up — it carries scene
// hierarchy + materials + skinning, and deserves a richer conversion
// path than "dump meshes into one flat buffer".

std::vector<Threedim::mesh> StlFromFile(
    std::string_view filename, Threedim::float_vec& buffer);

std::vector<Threedim::mesh> OffFromFile(
    std::string_view filename, Threedim::float_vec& buffer);

}
