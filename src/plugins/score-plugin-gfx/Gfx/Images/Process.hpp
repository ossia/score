#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/PropertyCommand.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Graph/imagenode.hpp>
#include <Gfx/Images/Metadata.hpp>
namespace Gfx::Images
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Images::Model)
  W_OBJECT(Model)

public:
    constexpr bool hasExternalUI() { return false; }
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  const std::vector<Image>& images() const noexcept { return m_images; }
  void setImages(const std::vector<Image>& f);
  void addImage(const Image& im);
  void removeImage(int i);
  void imagesChanged() W_SIGNAL(imagesChanged);
  PROPERTY(std::vector<Image>, images READ images WRITE setImages NOTIFY imagesChanged)

private:
  QString prettyName() const noexcept override;

  std::vector<Image> m_images;
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
  std::vector<ProcessDrop> drop(
      const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept override;
};
/*
class AddImage final : public score::Command
{
  SCORE_ COMMAND_DECL(
      CommandFactoryName(),
      AddImage,
      "Add Image")
public:
  ChangeSpline(
      const ProcessModel& autom,
      const ossia::spline_data& newval)
      : m_path{autom}, m_old{autom.spline()}, m_new{newval}
  {
  }

public:
  void undo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).setSpline(m_old);
  }
  void redo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).setSpline(m_new);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_old << m_new;
  }
  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_old >> m_new;
  }

private:
  Path<ProcessModel> m_path;
  ossia::spline_data m_old, m_new;
};
*/
}

W_REGISTER_ARGTYPE(Gfx::Image)
PROPERTY_COMMAND_T(Gfx, ChangeImages, Images::Model::p_images, "Change images")
SCORE_COMMAND_DECL_T(Gfx::ChangeImages)
