#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <core/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>

namespace iscore {
class DocumentModel;
class DocumentPresenter;
class DocumentView;
}  // namespace iscore
struct VisitorVariant;

// TODO ScenarioDocumentFactory instead?
class ScenarioDocument final : public iscore::DocumentDelegateFactoryInterface
{
    public:
        iscore::DocumentDelegateViewInterface* makeView(
                const iscore::ApplicationContext& ctx,
                iscore::DocumentView* parent) override;

        iscore::DocumentDelegatePresenterInterface* makePresenter(
                iscore::DocumentPresenter* parent_presenter,
                const iscore::DocumentDelegateModelInterface& model,
                iscore::DocumentDelegateViewInterface& view) override;

        iscore::DocumentDelegateModelInterface* makeModel(
                iscore::DocumentModel* parent) override;

        iscore::DocumentDelegateModelInterface* loadModel(
                const VisitorVariant&,
                iscore::DocumentModel* parent) override;
};
