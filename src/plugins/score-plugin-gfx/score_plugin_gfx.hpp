#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <utility>
#include <vector>

/*!
 * \namespace score::gfx
 * \brief Graphics rendering pipeline for ossia score
 *
 * This namespace provides a rendering pipeline for GPU visuals, by the
 * way of a rendering graph. The implementation uses the Qt RHI:
 *
 * https://www.qt.io/blog/graphics-in-qt-6.0-qrhi-qt-quick-qt-quick-3d
 *
 * It is mainly designed for rendering a succession of visual effects, not rendering
 * arbitrary 3D.
 * Shaders are written in the usual glslang -> spirv-cross fashion, with GLSL #version 450.
 * They are then translated automatically to SPIRV, Metal, GLSL or D3D11 shaders.
 *
 * The overall design is as follows :
 *
 * - Elements to render are represented as nodes to a graph: score::gfx::Node
 * * Input ports are the uniforms and input textures & audio data.
 * * Output ports generally are just the texture we are rendering to.
 *
 * - score::gfx::Graph contains all the nodes, and relationships between these nodes.
 * - For each output sink (a window surface, an NDI or Spout output...),
 *   a score::gfx::RenderList is created. It contains all the nodes to render for a given output.
 * - For each score::gfx::Node, the score::gfx::RenderList creates a score::gfx::NodeRenderer
 *   which contains the actual GPU resource handles for the sink.
 *
 * Given the following graph :
 * ```
 * [ video ] -> [ shader 1 ] -> [ screen 1 ]
 *                           \
 *                            -> [ shader 2] -> [ Spout output ]
 * ```
 *
 * Two render lists are created, the first being
 * ```
 * [ video ] -> [ shader 1 ] -> [ screen 1 ]
 * ```
 *
 * and the second being
 * ```
 * [ video ] -> [ shader 1 ] -> [ shader 2] -> [ Spout output ]
 * ```
 *
 * Rendering for these two RenderList is entirely independent - they can render at different
 * sizes and potentially use different graphic APIs, e.g. one OpenGL and the other Vulkan.
 * This is also done to enable threaded rendering for each of these.
 *
 * We provide the most essential nodes for building interesting visual effects:
 * - A window output node (score::gfx::ScreenNode).
 * - Shader filters using Interactive Shader Format (https://isf.video).
 * - Video (score::gfx::VideoNode) and image (score::gfx::ImagesNode) renderers.
 * - The video renderer tries to do as much of the decoding as possible on the GPU:
 *   for instance YUV420, HAP, ... are decoded on the GPU. Otherwises it falls back on libavcodec
 *   to guarantee compatibility with a wide range of video formats.
 *
 * Other plug-ins may provide other useful things, such as NDI I/O or particle effects.
 *
 * See GfxPlugins for information on how to create your custom nodes, visual effects and video decoders.
 */

/*!
 * \namespace Gfx
 * \brief Binds the rendering pipeline to ossia processes.
 *
 * This namespace contains the machinery that ties the rendering features provided
 * by score::gfx to the ossia execution engine and data model.
 *
 * It works as follows:
 *
 * * Every time processes execute, visual processes will mark themselves as executed,
 *   by notifying Gfx::GfxExecutionAction.
 *
 *   This is necessary because the visual graph is dynamic: processes can come on and off
 *   at any point during execution.
 *
 * * After all the ossia execution graph nodes have executed, the new edges between nodes
 *   are sent to the graphics processing thread, to Gfx::GfxContext.
 *
 * * This thread regularly updates the graph with the incoming information, either before VSync
 *   if VSync is enabled or with a timer otherwise:
 *   - Edges are updated with the currently running nodes in the score
 *   - Parameters, audio data... sent to the ossia processes
 *     in the execution graph are converted into uniforms corresponding to the shaders used,
 *     and uploaded to the GPU.
 */
namespace Gfx { }

class score_plugin_gfx final
    : public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
    , public score::CommandFactory_QtInterface
    , public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "11f76f02-11a4-4803-858d-a744ccdc0a7e")

public:
  score_plugin_gfx();
  ~score_plugin_gfx() override;

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;
  std::vector<score::PluginKey> required() const override;
};
