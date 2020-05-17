#pragma once
#include "mesh.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include <isf.hpp>
#include <list>

struct ISFNode : NodeModel
{
  static const inline QString defaultVert =
      R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 isf_FragNormCoord;

void main(void) {
  gl_Position = vec4( position, 0.0, 1.0 );
  isf_FragNormCoord = vec2((gl_Position.x+1.0)/2.0, (gl_Position.y+1.0)/2.0);
}
      )_";

  ISFNode(const isf::descriptor& desc, QString frag);
  ISFNode(const isf::descriptor& desc, QString vert, QString frag, const Mesh* mesh);

  virtual ~ISFNode();
  const Mesh& mesh() const noexcept;

  RenderedNode* createRenderer() const noexcept;

  // Texture format: 1 row = 1 channel of N samples
  std::list<AudioTexture> audio_textures;

private:
  const Mesh* m_mesh{};
};
