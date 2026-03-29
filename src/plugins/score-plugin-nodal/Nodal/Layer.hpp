#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Nodal/Presenter.hpp>
#include <Nodal/Process.hpp>
#include <Nodal/View.hpp>

#include <score_plugin_nodal_export.h>
namespace Nodal
{

// Callback type for creating patcher windows (registered by score-plugin-js)
using PatcherWindowFactory = QWidget* (*)(
    Process::ProcessModel&, const score::DocumentContext&, QWidget*);

class SCORE_PLUGIN_NODAL_EXPORT LayerFactory final
    : public Process::LayerFactory_T<Nodal::Model, Nodal::Presenter, Nodal::View>
{
public:
  // Called by score-plugin-js at startup to register the patcher window factory
  static void setPatcherWindowFactory(PatcherWindowFactory f) noexcept;

  bool hasExternalUI(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx) const noexcept override;

  QWidget* makeExternalUI(
      Process::ProcessModel& proc, const score::DocumentContext& ctx,
      QWidget* parent) const override;
};
}
