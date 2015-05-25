#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

class BaseElementModel;
class ProcessView;
class ProcessPresenter;
class ProcessViewModel;
class QGraphicsObject;
class ProcessPanelPresenter : public iscore::PanelPresenter
{
        Q_OBJECT
    public:
        ProcessPanelPresenter(iscore::Presenter* parent_presenter,
                              iscore::PanelView* view);

        int panelId() const override;
        void on_modelChanged() override;

    private slots:
        void on_focusedViewModelChanged(const ProcessViewModel*);
        void on_sizeChanged(const QSize& size);

    private:
        QGraphicsObject* m_obj{};
        BaseElementModel* m_baseElementModel{};
        const ProcessViewModel* m_processViewModel{};
        ProcessPresenter* m_processPresenter{};
        ProcessView* m_processView{};
};
