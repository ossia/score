#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/PropertyCommand.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Mesh/Metadata.hpp>
#include <isf.hpp>

namespace isf
{
struct descriptor;
}
namespace Gfx::Mesh
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Mesh::Model)
  W_OBJECT(Model)

public:
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  QString fragment() const noexcept { return m_fragment; }
  QString processedFragment() const noexcept { return m_processedFragment; }
  void setFragment(QString f);
  void fragmentChanged(const QString& f) W_SIGNAL(fragmentChanged, f);

  const isf::descriptor& isfDescriptor() const noexcept { return m_isfDescriptor; }

  void setMesh(QString f);
  QString mesh() const noexcept { return m_mesh; }
  void meshChanged(const QString& f) W_SIGNAL(meshChanged, f);

  PROPERTY(QString, fragment READ fragment WRITE setFragment NOTIFY fragmentChanged)
  PROPERTY(QString, mesh READ mesh WRITE setMesh NOTIFY meshChanged)
private:
  void setupIsf(const isf::descriptor& d);
  void setupNormalShader();
  QString prettyName() const noexcept override;
  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  QString m_fragment;
  QString m_processedFragment;
  isf::descriptor m_isfDescriptor;

  QString m_mesh;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::Mesh::Model>;

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("a432eab9-be16-481b-8da4-cab2d7cc686f")

  QSet<QString> acceptedFiles() const noexcept override;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("335781aa-00bd-4e7a-b2d5-55e5b5b61800")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  std::vector<ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override;
};

}

PROPERTY_COMMAND_T(
    Gfx,
    ChangeMeshFragmentShader,
    Mesh::Model::p_fragment,
    "Change fragment shader")
SCORE_COMMAND_DECL_T(Gfx::ChangeMeshFragmentShader)

PROPERTY_COMMAND_T(Gfx, ChangeMesh, Mesh::Model::p_mesh, "Change model")
SCORE_COMMAND_DECL_T(Gfx::ChangeMesh)
