#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>

namespace iscore {
class DocumentModel;
class DocumentPresenter;
class DocumentView;
}  // namespace iscore
struct VisitorVariant;

namespace Scenario
{
class ScenarioDocumentFactory final : public iscore::DocumentDelegateFactoryInterface
{
    public:
        iscore::DocumentDelegateViewInterface* makeView(
                const iscore::ApplicationContext& ctx,
                QObject* parent) override;

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
}
