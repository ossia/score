#pragma once
#include "node.hpp"

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    ScreenUBO
{
  float clipSpaceCorrMatrix[16]{};
  float texcoordAdjust[2]{};

  float renderSize[2]{};
};

#if defined(_MSC_VER)
#pragma pack()
#endif

struct MeshBuffers {
  QRhiBuffer* mesh{};
  QRhiBuffer* index{};
};

struct Renderer
{
  std::vector<NodeModel*> nodes;
  std::vector<RenderedNode*> renderedNodes;

  RenderState state;
  QSize lastSize{};

  // Mesh
  ossia::flat_map<const Mesh*, MeshBuffers> m_vertexBuffers;
  MeshBuffers initMeshBuffer(const Mesh& mesh);

  // Material
  ScreenUBO screenUBO;
  QRhiBuffer* m_rendererUBO{};

  QRhiTexture* m_emptyTexture{};

  bool ready{};

  void init();
  void release();

  void render();

  void update(QRhiResourceUpdateBatch& res);

  void maybeRebuild();

private:
  ossia::small_vector<std::pair<const Mesh* const, MeshBuffers>, 4> buffersToUpload;
};
