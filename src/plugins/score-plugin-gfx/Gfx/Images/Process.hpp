#pragma once
#include <Gfx/CommandFactory.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Images/Metadata.hpp>
#include <Library/LibraryInterface.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/PropertyCommand.hpp>

namespace Gfx
{
std::vector<score::gfx::Image> getImages(const ossia::value& val);
ossia::value fromImageSet(const gsl::span<score::gfx::Image>& images);
void releaseImages(std::vector<score::gfx::Image>& imgs);

struct ImageCache
{
public:
  std::optional<score::gfx::Image> acquire(const std::string& path);
  void release(score::gfx::Image&& img);

  static ImageCache& instance() noexcept;
private:
  std::unordered_map<std::string, std::pair<int, score::gfx::Image>> m_images;
};
}
W_REGISTER_ARGTYPE(score::gfx::Image)


namespace Gfx::Images
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Images::Model)
  W_OBJECT(Model)

public:
  constexpr bool hasExternalUI() { return false; }
  Model(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  //std::vector<score::gfx::Image> images() const noexcept;
  //void setImages(const std::vector<score::gfx::Image>& f);
  //  void imagesChanged() W_SIGNAL(imagesChanged);
  //  PROPERTY(
  //      std::vector<score::gfx::Image>,
  //      images READ images WRITE setImages NOTIFY imagesChanged)
  //
private:
  void on_imagesChanged(const ossia::value& v);
  QString prettyName() const noexcept override;
  std::vector<score::gfx::Image> m_currentImages;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::Images::Model>;

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("0916759f-a5f6-4870-a96b-4e1e5efe5885")

  QSet<QString> acceptedFiles() const noexcept override;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("f37aa176-d8be-45bc-b833-d014efba6157")
public:
  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropCustom(
      std::vector<ProcessDrop>& drops,
      const QMimeData& data,
      const score::DocumentContext& ctx) const noexcept override;
};

}
