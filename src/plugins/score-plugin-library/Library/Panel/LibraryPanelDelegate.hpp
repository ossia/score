#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
class QTabWidget;
namespace Library
{
class ProjectLibraryWidget;
class SystemLibraryWidget;
class FileSystemModel;
class UserPanel final : public score::PanelDelegate
{
public:
  UserPanel(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;

  QWidget* m_widget{};
};

class ProjectPanel final : public score::PanelDelegate
{
public:
  ProjectPanel(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
      override;

  ProjectLibraryWidget* m_widget{};
};

class ProcessPanel final : public score::PanelDelegate
{
public:
  ProcessPanel(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;

  QWidget* m_widget{};
};
}
