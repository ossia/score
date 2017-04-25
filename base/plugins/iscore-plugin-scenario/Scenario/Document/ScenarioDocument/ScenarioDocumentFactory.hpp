#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>

namespace iscore
{
class DocumentModel;
class DocumentPresenter;
class DocumentView;
} // namespace iscore
struct VisitorVariant;

namespace Scenario
{
class ScenarioDocumentFactory final : public iscore::DocumentDelegateFactory
{
  ISCORE_CONCRETE("2bca3373-d858-4288-b054-5960d3e5902c")

  iscore::DocumentDelegateView*
  makeView(const iscore::GUIApplicationContext& ctx, QObject* parent) override;

  iscore::DocumentDelegatePresenter* makePresenter(
      const iscore::DocumentContext& ctx,
      iscore::DocumentPresenter* parent_presenter,
      const iscore::DocumentDelegateModel& model,
      iscore::DocumentDelegateView& view) override;

  iscore::DocumentDelegateModel* make(
      const iscore::DocumentContext& ctx,
      iscore::DocumentModel* parent) override;

  iscore::DocumentDelegateModel* load(
      const VisitorVariant&,
      const iscore::DocumentContext& ctx,
      iscore::DocumentModel* parent) override;
};
}
