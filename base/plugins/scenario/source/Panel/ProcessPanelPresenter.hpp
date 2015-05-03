#pragma once
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class BaseElementModel;
class ProcessViewInterface;
class ProcessPresenterInterface;
class ProcessViewModel;
class ProcessPanelPresenter : public iscore::PanelPresenterInterface
{
        Q_OBJECT
    public:
        ProcessPanelPresenter(iscore::Presenter* parent_presenter,
                              iscore::PanelViewInterface* view);

        int panelId() const override;
        void on_modelChanged() override;

    private slots:
        void on_focusedViewModelChanged();
        void on_sizeChanged(const QSize& size);

    private:
        QGraphicsObject* m_obj{};
        BaseElementModel* m_baseElementModel{};
        const ProcessViewModel* m_processViewModel{};
        ProcessPresenterInterface* m_processPresenter{};
        ProcessViewInterface* m_processView{};
};
