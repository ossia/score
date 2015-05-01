#pragma once
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class UndoPresenter : public iscore::PanelPresenterInterface
{
    public:
    UndoPresenter(iscore::Presenter *parent_presenter,
                       iscore::PanelViewInterface *view);

    QString modelObjectName() const override;

    void on_modelChanged() override;

};
