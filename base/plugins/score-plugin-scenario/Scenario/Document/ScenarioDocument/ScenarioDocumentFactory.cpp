// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <algorithm>
#include <core/document/DocumentModel.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include "ScenarioDocumentFactory.hpp"

namespace Scenario
{
score::DocumentDelegateView* ScenarioDocumentFactory::makeView(
    const score::GUIApplicationContext& ctx, QObject* parent)
{
  return new ScenarioDocumentView{ctx, parent};
}

score::DocumentDelegatePresenter*
ScenarioDocumentFactory::makePresenter(
    const score::DocumentContext& ctx,
    score::DocumentPresenter* parent_presenter,
    const score::DocumentDelegateModel& model,
    score::DocumentDelegateView& view)
{
  return new ScenarioDocumentPresenter{ctx, parent_presenter, model, view};
}

void ScenarioDocumentFactory::make(
    const score::DocumentContext& ctx,
    score::DocumentDelegateModel*& ptr,
    score::DocumentModel* parent)
{
  std::allocator<ScenarioDocumentModel> alloc;
  auto res = alloc.allocate(1);
  ptr = res;
  alloc.construct(res, ctx, parent);
}

void ScenarioDocumentFactory::load(
    const VisitorVariant& vis,
    const score::DocumentContext& ctx,
    score::DocumentDelegateModel*& ptr,
    score::DocumentModel* parent)
{
  std::allocator<ScenarioDocumentModel> alloc;
  auto res = alloc.allocate(1);
  ptr = res;
  score::deserialize_dyn(vis, [&](auto&& deserializer) {
    alloc.construct(res, deserializer, ctx, parent);
    return ptr;
  });
}
}
