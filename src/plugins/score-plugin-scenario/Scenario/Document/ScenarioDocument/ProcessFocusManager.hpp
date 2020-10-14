#pragma once
#include <Process/Focus/FocusDispatcher.hpp>

#include <QObject>
#include <QPointer>

#include <score_plugin_scenario_export.h>

#include <verdigris>
namespace score
{
struct FocusManager;
}
namespace Process
{
class LayerPresenter;
}
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class ScenarioDocumentPresenter;
}
// Keeps the focused elements in memory for use by the scenario application
// plugin.
// Note : focus should not be lost when switching documents. Hence, this
// should more be part of the per-document part.
namespace Process
{
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessFocusManager final : public QObject
{
  W_OBJECT(ProcessFocusManager)

public:
  ProcessFocusManager(score::FocusManager& fmgr);
  ~ProcessFocusManager();

  ProcessModel* focusedModel() const;
  LayerPresenter* focusedPresenter() const;

  void focus(QPointer<Process::LayerPresenter>);
  void focus(Scenario::ScenarioDocumentPresenter*);

  void focusNothing();

public:
  void sig_focusedPresenter(LayerPresenter* arg_1) W_SIGNAL(sig_focusedPresenter, arg_1);
  void sig_defocusedPresenter(LayerPresenter* arg_1) W_SIGNAL(sig_defocusedPresenter, arg_1);

  void sig_defocusedViewModel(const ProcessModel* arg_1) W_SIGNAL(sig_defocusedViewModel, arg_1);
  void sig_focusedViewModel(const ProcessModel* arg_1) W_SIGNAL(sig_focusedViewModel, arg_1);

  void sig_focusedRoot() W_SIGNAL(sig_focusedRoot);

private:
  void focusPresenter(LayerPresenter*);
  void defocusPresenter(LayerPresenter*);

  score::FocusManager& m_mgr;

  QPointer<ProcessModel> m_currentModel{};
  QPointer<LayerPresenter> m_currentPresenter{};

  QMetaObject::Connection m_deathConnection{};
};
}
