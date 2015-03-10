#pragma once
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class BaseElementModel;
class ProcessPanelPresenter : public iscore::PanelPresenterInterface
{
        Q_OBJECT
    public:
        using iscore::PanelPresenterInterface::PanelPresenterInterface;

        QString modelObjectName() const override;
        void on_modelChanged() override;

    private slots:
        void on_focusedProcessChanged();

    private:
        BaseElementModel* m_baseElementModel{};
};
