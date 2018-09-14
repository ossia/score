#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
class QTabWidget;
namespace Library
{
class ProjectLibraryWidget;
class SystemLibraryWidget;
class FileSystemModel;
class PanelDelegate final : public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;
  void on_modelChanged(
      score::MaybeDocument oldm, score::MaybeDocument newm) override;

  QTabWidget* m_widget{};

  FileSystemModel* m_projectModel{};
  ProjectLibraryWidget* m_projectView{};
};
}
