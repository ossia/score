#include "score_plugin_threedim.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <Avnd/Factories.hpp>
#include <Threedim/ArrayToGeometry.hpp>
#include <Threedim/ModelDisplay/Executor.hpp>
#include <Threedim/ModelDisplay/Process.hpp>
#include <Threedim/Noise.hpp>
#include <Threedim/ObjLoader.hpp>
#include <Threedim/Primitive.hpp>
#include <Threedim/StructureSynth.hpp>

#include <score_plugin_engine.hpp>

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
  Avnd::instantiate_fx<
      Threedim::ArrayToMesh,
      Threedim::Noise,
      Threedim::StrucSynth,
      Threedim::ObjLoader,
      Threedim::Plane,
      Threedim::Cube,
      Threedim::Sphere,
      Threedim::Icosahedron,
      Threedim::Cylinder,
      Threedim::Cone,
      Threedim::Torus>(fx, ctx, key);
  auto add = instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Gfx::ModelDisplay::ProcessFactory>,
      FW<Library::LibraryInterface,
         Threedim::SSynthLibraryHandler,
         Threedim::OBJLibraryHandler>,
      FW<Process::ProcessDropHandler,
         Threedim::SSynthDropHandler,
         Threedim::OBJDropHandler>,
      FW<Execution::ProcessComponentFactory,
         Gfx::ModelDisplay::ProcessExecutorComponentFactory>>(ctx, key);
  fx.insert(
      fx.end(),
      std::make_move_iterator(add.begin()),
      std::make_move_iterator(add.end()));
  return fx;
}

std::vector<score::PluginKey> score_plugin_threedim::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_threedim)
