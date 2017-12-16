#pragma once
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score_plugin_scenario_export.h>

namespace score
{
class DocumentModel;
class DocumentPresenter;
class DocumentView;
} // namespace score
struct VisitorVariant;

namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentFactory final : public score::DocumentDelegateFactory
{
  SCORE_CONCRETE("2bca3373-d858-4288-b054-5960d3e5902c")

  score::DocumentDelegateView*
  makeView(const score::GUIApplicationContext& ctx, QObject* parent) override;

  score::DocumentDelegatePresenter* makePresenter(
      const score::DocumentContext& ctx,
      score::DocumentPresenter* parent_presenter,
      const score::DocumentDelegateModel& model,
      score::DocumentDelegateView& view) override;

  void make(
      const score::DocumentContext& ctx,
      score::DocumentDelegateModel*& ptr,
      score::DocumentModel* parent) override;

  void load(
      const VisitorVariant&,
      const score::DocumentContext& ctx,
      score::DocumentDelegateModel*& ptr,
      score::DocumentModel* parent) override;
};
}
