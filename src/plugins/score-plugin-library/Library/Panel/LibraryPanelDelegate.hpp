#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>

#include <score_plugin_library_export.h>
class QTabWidget;
class QStackedWidget;
namespace Library
{
class ProjectLibraryWidget;
class SystemLibraryWidget;
class ProcessWidget;
class FileSystemModel;
class RemoteLibraryWidget;
class UserPanel final : public score::PanelDelegate
{
public:
  UserPanel(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;

  //! The library is a place on a machine, and which machine depends on the
  //! document: a score that runs elsewhere has its media there, not here.
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;

  QStackedWidget* m_widget{};
  SystemLibraryWidget* m_local{};
  RemoteLibraryWidget* m_remote{};
};

class ProjectPanel final : public score::PanelDelegate
{
public:
  ProjectPanel(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;

  ProjectLibraryWidget* m_widget{};
};

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessPanel final : public score::PanelDelegate
{
public:
  ProcessPanel(const score::GUIApplicationContext& ctx);
  ProcessWidget& processWidget() const noexcept;

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;

  //! The panel is one, documents are many, and what is available is a property
  //! of the document: a score that runs on another machine can only use that
  //! machine's processes. So the list is rebuilt from this build's factories
  //! whenever a document that runs here becomes visible -- and left alone for
  //! one that does not, since whatever mirrored the other machine owns it then.
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;

  QWidget* m_widget{};
};
}
