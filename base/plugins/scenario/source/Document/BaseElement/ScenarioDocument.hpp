#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

class ScenarioDocument : public iscore::DocumentDelegateFactoryInterface
{
    public:
        iscore::DocumentDelegateViewInterface* makeView(
                iscore::DocumentView* parent) override;

        iscore::DocumentDelegatePresenterInterface* makePresenter(
                iscore::DocumentPresenter* parent_presenter,
                iscore::DocumentDelegateModelInterface* model,
                iscore::DocumentDelegateViewInterface* view) override;

        iscore::DocumentDelegateModelInterface* makeModel(
                iscore::DocumentModel* parent) override;

        iscore::DocumentDelegateModelInterface* loadModel(
                const VisitorVariant&,
                iscore::DocumentModel* parent) override;
};
