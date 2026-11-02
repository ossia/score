#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Threedim/GeometryViewer/Process.hpp>
#include <Threedim/GeometryViewer/SceneMonitor.hpp>

#include <memory>

class QLabel;
class QStandardItemModel;
class QTreeView;
namespace score
{
struct DocumentContext;
}

namespace Gfx::GeometryViewer
{
/**
 * The objects of the scene the viewer draws, as a tree: each node with its
 * name, each component under it with its kind and what it holds. Selecting
 * one outlines it in the viewer.
 *
 * The tree comes from the renderer through the monitor of the process, so it
 * shows while the process plays.
 */
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Model>
{
public:
  InspectorWidget(
      const Model& process, const score::DocumentContext& context, QWidget* parent);
  ~InspectorWidget() override;

protected:
  void showEvent(QShowEvent* ev) override;
  void hideEvent(QHideEvent* ev) override;
  void timerEvent(QTimerEvent* ev) override;

private:
  void refresh();
  void rebuild(const SceneSummary& s);
  void select();
  SceneMonitor* monitor() const noexcept;
  //! Counts as one of the viewers of the monitor while shown
  void watch(bool shown);

  QLabel* m_status{};
  QTreeView* m_tree{};
  QStandardItemModel* m_model{};
  std::shared_ptr<const SceneSummary> m_shown;
  std::shared_ptr<Process::Monitor> m_watched;
  int m_timer{};
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("7fa7e5ae-985c-463c-9b2b-c155170365a9")
};
}
