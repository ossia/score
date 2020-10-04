#pragma once
#include "node.hpp"
#include "renderer.hpp"

#include <score_plugin_gfx_export.h>
#include <ossia/detail/algorithms.hpp>
struct OutputNode;
class Window;
struct SCORE_PLUGIN_GFX_EXPORT Graph
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

  void setVSyncCallback(std::function<void()>);

  void maybeRebuild(Renderer& r);

  std::shared_ptr<Renderer> createRenderer(OutputNode*, RenderState state);

  void setupOutputs(GraphicsApi graphicsApi);

  void relinkGraph();

  ~Graph();

private:
  std::vector<OutputNode*> outputs;
  std::vector<std::shared_ptr<Renderer>> renderers;

  std::vector<std::shared_ptr<Window>> unused_windows;
  std::function<void()> vsync_callback;
};

#if QT_CONFIG(vulkan)
class QVulkanInstance;
QVulkanInstance* staticVulkanInstance();
#endif
