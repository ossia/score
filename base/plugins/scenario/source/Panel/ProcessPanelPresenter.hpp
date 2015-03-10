#pragma once
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class BaseElementModel;
class ProcessViewInterface;
class ProcessPresenterInterface;
class ProcessSharedModelInterface;
class ProcessPanelPresenter : public iscore::PanelPresenterInterface
{
        Q_OBJECT
    public:
        using iscore::PanelPresenterInterface::PanelPresenterInterface;

        QString modelObjectName() const override;
        void on_modelChanged() override;

    private slots:
        void on_focusedProcessChanged();
        void on_sizeChanged(const QSize& size);

    private:
        BaseElementModel* m_baseElementModel{};
        ProcessSharedModelInterface* m_processModel{};
        ProcessPresenterInterface* m_processPresenter{};
        ProcessViewInterface* m_processView{};
};
