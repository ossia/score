#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace Device
{
class DeviceList;
}
namespace State
{
struct Address;
}

namespace Gfx::Filter
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("e62ed6f6-a2c1-4d27-a9c3-1c3bc576bfeb")

  QSet<QString> acceptedFiles() const noexcept override;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;

  void addPath(std::string_view path) override;
  QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept override;
  QWidget*
  previewWidget(const Process::Preset& path, QWidget* parent) const noexcept override;

  Library::Subcategories categories;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("d1e16bba-4c53-4d24-8b6b-71b94daef68d")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropPath(
      std::vector<ProcessDrop>&, const score::FilePath& data,
      const score::DocumentContext& ctx) const noexcept override;

  void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept override;
};

struct VideoTextureDropHandler : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("e9bf6cf8-c872-4638-b98a-ed76edc8e2dd")

public:
  QSet<QString> mimeTypes() const noexcept override;

  bool create(
      std::vector<ProcessDrop>& drops,
      const std::vector<State::Address>& addresses) const;

  bool isTexture(const State::Address& addr, const Device::DeviceList& devicelist) const noexcept;

  void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept final override;
};

}
