#pragma once
#include "node.hpp"
#include "renderer.hpp"

#include <ossia/detail/algorithms.hpp>
struct OutputNode;
class Window;
struct Graph
{
  std::vector<NodeModel*> nodes;
  std::vector<Edge*> edges;

  void addNode(NodeModel* n) { nodes.push_back(n); }

  void removeNode(NodeModel* n)
  {
    if (auto it = ossia::find(nodes, n); it != nodes.end())
    {
      nodes.erase(it);
    }
  }

  void maybeRebuild(Renderer& r);

  std::shared_ptr<Renderer> createRenderer(OutputNode*, RenderState state);

  void setupOutputs(GraphicsApi graphicsApi);

  void relinkGraph();

  ~Graph();

private:
  std::vector<OutputNode*> outputs;
  std::vector<std::shared_ptr<Renderer>> renderers;

  std::vector<std::shared_ptr<Window>> unused_windows;

#if QT_CONFIG(vulkan)
  QVulkanInstance vulkanInstance;
  bool vulkanInstanceCreated{};
#endif
};
