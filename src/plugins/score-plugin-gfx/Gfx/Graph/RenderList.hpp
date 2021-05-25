#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/CommonUBOs.hpp>

namespace score::gfx
{

struct MeshBuffers
{
  QRhiBuffer* mesh{};
  QRhiBuffer* index{};
};

struct OutputNode;
struct SCORE_PLUGIN_GFX_EXPORT RenderList
{
  void init();
  void release();

  void render(QRhiCommandBuffer& commands);
  void update(QRhiResourceUpdateBatch& res);

  MeshBuffers initMeshBuffer(const Mesh& mesh);
  void maybeRebuild();

  QRhiTexture* textureTargetForInputPort(Port& port);

  using Buffers = std::pair<const Mesh* const, MeshBuffers>;
  ossia::small_vector<Buffers, 4> buffersToUpload;


  std::vector<score::gfx::Node*> nodes;
  std::vector<score::gfx::NodeRenderer*> renderedNodes;
  OutputNode* output{};

  RenderState state;
  QSize lastSize{};

  // Mesh
  ossia::flat_map<const Mesh*, MeshBuffers> m_vertexBuffers;

  // Material
  ScreenUBO screenUBO;
  QRhiBuffer* m_rendererUBO{};

  QRhiTexture* m_emptyTexture{};

  bool ready{};
};
}
