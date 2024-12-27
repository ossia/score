#pragma once
#include <Threedim/TinyObj.hpp>
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/algorithms/create/platonic.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <vcg/complex/algorithms/update/texture.h>

namespace Threedim
{
using namespace vcg;
class TFace;
class TVertex;

struct TUsedTypes
    : public vcg::UsedTypes<vcg::Use<TVertex>::AsVertexType, vcg::Use<TFace>::AsFaceType>
{
};

class TVertex
    : public Vertex<
          TUsedTypes,
          vertex::BitFlags,
          vertex::Coord3f,
          vertex::Normal3f,
          vertex::TexCoord2f,
          vertex::Mark>
{
};

class TFace
    : public Face<
          TUsedTypes,
          face::VertexRef,
          face::Normal3f,
          face::WedgeTexCoord2f,
          face::BitFlags,
          face::FFAdj>
{
};

class TMesh : public vcg::tri::TriMesh<std::vector<TVertex>, std::vector<TFace>>
{
};

void loadTriMesh(TMesh& mesh, std::vector<float>& complete, PrimitiveOutputs& outputs);
}
