#include "score_plugin_threedim.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <Avnd/Factories.hpp>
#include <Threedim/ArrayToGeometry.hpp>
#include <Threedim/ArrayToTexture.hpp>
#include <Threedim/BufferToGeometry.hpp>
#include <Threedim/GeometryInfo.hpp>
#include <Threedim/GeometryPacker.hpp>
#include <Threedim/GeometryToBuffer.hpp>
#include <Threedim/ModelDisplay/Executor.hpp>
#include <Threedim/ModelDisplay/Process.hpp>
#include <Threedim/Noise.hpp>
#include <Threedim/ObjLoader.hpp>
#include <Threedim/PCLToGeometry.hpp>
#include <Threedim/Primitive.hpp>
#include <Threedim/RenderPipeline/Executor.hpp>
#include <Threedim/RenderPipeline/Layer.hpp>
#include <Threedim/RenderPipeline/Process.hpp>
#include <Threedim/StructureSynth.hpp>
#include <Threedim/TextureToBuffer.hpp>
#include <avendish/examples/Gpu/ArrayToBuffer.hpp>
#include <avendish/examples/Gpu/BufferToArray.hpp>

#include <score_plugin_engine.hpp>
#include <score_plugin_threedim_commands_files.hpp>

namespace Threedim
{
class SSynthLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("623f6c12-f661-474d-9bba-dee208e9a6a4")

  QSet<QString> acceptedFiles() const noexcept override { return {"es"}; }

  Library::Subcategories categories;

  using proc = oscr::ProcessModel<StrucSynth>;
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, proc>::get();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }

    categories.init("Structure Synth", node, ctx);
  }

  void addPath(std::string_view path) override
  {
    QFileInfo file{QString::fromUtf8(path.data(), path.length())};
    QFile f{file.absoluteFilePath()};
    if (!f.open(QIODevice::ReadOnly))
      return;

    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();
    pdata.key = Metadata<ConcreteKey_k, proc>::get();
    pdata.customData = score::readFileAsQString(f);
    categories.add(file, std::move(pdata));
  }
};

class SSynthDropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("8b7892e1-f8ac-4579-bbf5-d71eb9810598")

  QSet<QString> fileExtensions() const noexcept override { return {"es"}; }

  using proc = oscr::ProcessModel<StrucSynth>;
  void dropData(
      std::vector<ProcessDrop>& vec,
      const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    const auto& [filename, content] = data;

    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, proc>::get();
      p.creation.prettyName = filename.basename;
      p.setup = [s = content](Process::ProcessModel& m, score::Dispatcher& disp) mutable
      {
        auto& pp = static_cast<proc&>(m);
        auto& inl = *safe_cast<Process::ControlInlet*>(pp.inlets()[0]);
        disp.submit(new Process::SetControlValue{inl, s.toStdString()});
      };
      vec.push_back(std::move(p));
    }
  }
};

class OBJLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("da4af155-3cb6-41df-8c10-5a002b9d97ca")

  QSet<QString> acceptedFiles() const noexcept override { return {"obj", "ply"}; }

  Library::Subcategories categories;

  using proc = oscr::ProcessModel<ObjLoader>;
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, proc>::get();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
      return;

    categories.init("Object Loader", node, ctx);
  }

  void addPath(std::string_view path) override
  {
    auto p = QString::fromUtf8(path.data(), path.length());
    QFileInfo file{p};

    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();
    pdata.key = Metadata<ConcreteKey_k, proc>::get();
    pdata.customData = p;
    categories.add(file, std::move(pdata));
  }
};

class OBJDropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("1d6cac56-2059-4fb8-9cef-19301a1fba3d")

  QSet<QString> fileExtensions() const noexcept override { return {"obj", "ply"}; }

  using proc = oscr::ProcessModel<ObjLoader>;
  void dropData(
      std::vector<ProcessDrop>& vec,
      const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    const auto& [filename, content] = data;

    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, proc>::get();
      p.creation.prettyName = filename.basename;
      p.setup = [s = filename.relative](
                    Process::ProcessModel& m, score::Dispatcher& disp) mutable
      {
        auto& pp = static_cast<proc&>(m);
        auto& inl = *safe_cast<Process::ControlInlet*>(pp.inlets()[0]);
        disp.submit(new Process::SetControlValue{inl, s.toStdString()});
      };
      vec.push_back(std::move(p));
    }
  }
};

}
/**
 * This file instantiates the classes that are provided by this plug-in.
 */
score_plugin_threedim::score_plugin_threedim() = default;
score_plugin_threedim::~score_plugin_threedim() = default;

std::vector<score::InterfaceBase*> score_plugin_threedim::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  std::vector<score::InterfaceBase*> fx;
  oscr::instantiate_fx<uo::ArrayToBuffer>(fx, ctx, key);
  oscr::instantiate_fx<uo::BufferToArray>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::ArrayToMesh>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::ArrayToTexture>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Noise>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::StrucSynth>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::ObjLoader>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Plane>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Cube>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Sphere>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::GeometryInfo>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Icosahedron>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Cylinder>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Cone>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::Torus>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::PCLToMesh2>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::ExtractBuffer>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::GeometryPacker>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::BuffersToGeometry>(fx, ctx, key);
  oscr::instantiate_fx<Threedim::TextureToBuffer>(fx, ctx, key);
  auto add = instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Gfx::ModelDisplay::ProcessFactory,
         Gfx::RenderPipeline::ProcessFactory>,
      FW<Process::LayerFactory, Gfx::RenderPipeline::LayerFactory>,
      FW<Library::LibraryInterface, Threedim::SSynthLibraryHandler,
         Threedim::OBJLibraryHandler>,
      FW<Process::ProcessDropHandler, Threedim::SSynthDropHandler,
         Threedim::OBJDropHandler>,
      FW<Execution::ProcessComponentFactory,
         Gfx::ModelDisplay::ProcessExecutorComponentFactory,
         Gfx::RenderPipeline::ProcessExecutorComponentFactory>>(ctx, key);
  fx.insert(
      fx.end(),
      std::make_move_iterator(add.begin()),
      std::make_move_iterator(add.end()));
  return fx;
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_threedim::make_commands()
{
  using namespace Gfx;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_threedim_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
std::vector<score::PluginKey> score_plugin_threedim::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_threedim)
