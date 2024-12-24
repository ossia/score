#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Graph/Scale.hpp>
#include <Gfx/Video/Metadata.hpp>
#include <Library/LibraryInterface.hpp>
#include <Video/VideoDecoder.hpp>

#include <score/command/PropertyCommand.hpp>
namespace Gfx::Video
{
using video_decoder = ::Video::VideoDecoder;
std::shared_ptr<video_decoder> makeDecoder(const std::string&) noexcept;

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Video::Model)
  W_OBJECT(Model)

public:
  Model(
      const TimeVal& duration, const QString& path, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  QString absolutePath() const noexcept;
  QString path() const noexcept { return m_path; }
  void setPath(const QString& f);
  void pathChanged(const QString& f) W_SIGNAL(pathChanged, f);

  double nativeTempo() const noexcept;
  void setNativeTempo(double);
  void nativeTempoChanged(double t) W_SIGNAL(nativeTempoChanged, t);

  score::gfx::ScaleMode scaleMode() const noexcept;
  void setScaleMode(score::gfx::ScaleMode);
  void scaleModeChanged(score::gfx::ScaleMode t) W_SIGNAL(scaleModeChanged, t);

  bool ignoreTempo() const noexcept;
  void setIgnoreTempo(bool);
  void ignoreTempoChanged(bool t) W_SIGNAL(ignoreTempoChanged, t);

  PROPERTY(
      score::gfx::ScaleMode,
      scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)

  PROPERTY(QString, path READ path WRITE setPath NOTIFY pathChanged)
  PROPERTY(
      double,
      nativeTempo READ nativeTempo WRITE setNativeTempo NOTIFY nativeTempoChanged,
      W_Final)
  PROPERTY(
      bool, ignoreTempo READ ignoreTempo WRITE setIgnoreTempo NOTIFY ignoreTempoChanged,
      W_Final)

private:
  QString m_path;
  score::gfx::ScaleMode m_scaleMode{};
  double m_nativeTempo{};
  bool m_ignoreTempo{};
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

public:
  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropPath(
      std::vector<ProcessDrop>& vec, const score::FilePath& filename,
      const score::DocumentContext& ctx) const noexcept override;
};

}

PROPERTY_COMMAND_T(Gfx, ChangeVideo, Video::Model::p_path, "Change video")
SCORE_COMMAND_DECL_T(Gfx::ChangeVideo)
PROPERTY_COMMAND_T(Gfx, ChangeTempo, Video::Model::p_nativeTempo, "Change video tempo")
SCORE_COMMAND_DECL_T(Gfx::ChangeTempo)
PROPERTY_COMMAND_T(
    Gfx, ChangeIgnoreTempo, Video::Model::p_ignoreTempo, "Ignore video tempo")
SCORE_COMMAND_DECL_T(Gfx::ChangeIgnoreTempo)
PROPERTY_COMMAND_T(
    Gfx, ChangeVideoScaleMode, Video::Model::p_scaleMode, "Video scale mode")
SCORE_COMMAND_DECL_T(Gfx::ChangeVideoScaleMode)

W_REGISTER_ARGTYPE(score::gfx::ScaleMode)
