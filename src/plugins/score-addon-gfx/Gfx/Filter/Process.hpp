#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/PropertyCommand.hpp>

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
  Model(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

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

  const isf::descriptor& isfDescriptor() const noexcept
  { return m_isfDescriptor; }

  PROPERTY(
      QString,
      fragment READ fragment WRITE setFragment NOTIFY fragmentChanged)
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

PROPERTY_COMMAND_T(
    Gfx,
    ChangeFragmentShader,
    Filter::Model::p_fragment,
    "Change fragment shader")
SCORE_COMMAND_DECL_T(Gfx::ChangeFragmentShader)
