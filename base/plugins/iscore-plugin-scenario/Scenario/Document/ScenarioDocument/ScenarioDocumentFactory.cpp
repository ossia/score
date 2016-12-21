#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <algorithm>
#include <core/document/DocumentModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

#include "ScenarioDocumentFactory.hpp"

namespace iscore
{
class DocumentPresenter;
} // namespace iscore
struct VisitorVariant;

namespace Scenario
{
iscore::DocumentDelegateView* ScenarioDocumentFactory::makeView(
    const iscore::ApplicationContext& ctx, QObject* parent)
{
  return new ScenarioDocumentView{ctx, parent};
}

iscore::DocumentDelegatePresenter*
ScenarioDocumentFactory::makePresenter(
    iscore::DocumentPresenter* parent_presenter,
    const iscore::DocumentDelegateModel& model,
    iscore::DocumentDelegateView& view)
{
  return new ScenarioDocumentPresenter{parent_presenter, model, view};
}

iscore::DocumentDelegateModel* ScenarioDocumentFactory::make(
    const iscore::DocumentContext& ctx, iscore::DocumentModel* parent)
{
  return new ScenarioDocumentModel{ctx, parent};
}

iscore::DocumentDelegateModel* ScenarioDocumentFactory::load(
    const VisitorVariant& vis,
    const iscore::DocumentContext& ctx,
    iscore::DocumentModel* parent)
{

  return deserialize_dyn(vis, [&](auto&& deserializer) {
    return new ScenarioDocumentModel{deserializer, ctx, parent};
  });
}
}
