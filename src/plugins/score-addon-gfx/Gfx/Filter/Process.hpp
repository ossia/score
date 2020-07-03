#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Filter/Metadata.hpp>
#include <isf.hpp>

namespace isf
{
struct descriptor;
}
namespace Gfx::Filter
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Filter::Model)
  W_OBJECT(Model)

public:
  static constexpr bool hasExternalUI() noexcept { return true; }

  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  const QString& fragment() const noexcept { return m_fragment; }
  const QString& processedFragment() const noexcept { return m_processedFragment; }
  const QString& vertex() const noexcept { return m_vertex; }
  const QString& processedVertex() const noexcept { return m_processedVertex; }
  void setVertex(QString f);
  void vertexChanged(const QString& v) W_SIGNAL(vertexChanged, v);
  void setFragment(QString f);
  void fragmentChanged(const QString& f) W_SIGNAL(fragmentChanged, f);

  const isf::descriptor& isfDescriptor() const noexcept { return m_isfDescriptor; }
  void errorMessage(int arg_1, const QString& arg_2) W_SIGNAL(errorMessage, arg_1, arg_2);

  PROPERTY(QString, vertex READ vertex WRITE setVertex NOTIFY vertexChanged)
  PROPERTY(QString, fragment READ fragment WRITE setFragment NOTIFY fragmentChanged)
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

  QString m_vertex;
  QString m_processedVertex;
  QString m_fragment;
  QString m_processedFragment;
  isf::descriptor m_isfDescriptor;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::Filter::Model>;

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("e62ed6f6-a2c1-4d27-a9c3-1c3bc576bfeb")

  QSet<QString> acceptedFiles() const noexcept override;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("d1e16bba-4c53-4d24-8b6b-71b94daef68d")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  std::vector<ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override;
};

}

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Gfx
{
class ChangeFragmentShader : public Scenario::EditScript<Filter::Model, Filter::Model::p_fragment>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeFragmentShader, "Edit a script")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Gfx::Filter::Model::p_fragment> : Gfx::ChangeFragmentShader
{
  using ChangeFragmentShader::ChangeFragmentShader;
};
}
