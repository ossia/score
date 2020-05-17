#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/PropertyCommand.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Video/Metadata.hpp>
#include <Video/VideoDecoder.hpp>
namespace Gfx::Video
{
using video_decoder = ::Video::VideoDecoder;
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Video::Model)
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

  QString path() const noexcept { return m_path; }
  void setPath(const QString& f);
  void pathChanged(const QString& f) W_SIGNAL(pathChanged, f);
  PROPERTY(QString, path READ path WRITE setPath NOTIFY pathChanged)

  const std::shared_ptr<video_decoder>& decoder() const noexcept
  {
    return m_decoder;
  }

private:
  QString prettyName() const noexcept override;
  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  QString m_path;
  std::shared_ptr<video_decoder> m_decoder;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::Video::Model>;

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("be66d573-571f-4c33-9f60-0791f53c7266")

  QSet<QString> acceptedFiles() const noexcept override;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("12d1ed39-0fac-43da-8520-b7e32f9fad7d")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  std::vector<ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override;
};

}

PROPERTY_COMMAND_T(Gfx, ChangeVideo, Video::Model::p_path, "Change video")
SCORE_COMMAND_DECL_T(Gfx::ChangeVideo)
