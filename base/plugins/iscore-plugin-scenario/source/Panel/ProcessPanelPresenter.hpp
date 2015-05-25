#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include "ProcessPanelGraphicsProxy.hpp"

#include <ProcessInterface/ZoomHelper.hpp>
class BaseElementModel;
class ProcessView;
class ProcessPresenter;
class ProcessViewModel;
class ProcessPanelPresenter : public iscore::PanelPresenter
{
        Q_OBJECT
    public:
        ProcessPanelPresenter(iscore::Presenter* parent_presenter,
                              iscore::PanelView* view);

        int panelId() const override;
        void on_modelChanged() override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

    private slots:
        void on_focusedViewModelChanged(const ProcessViewModel*);
        void on_sizeChanged(const QSize& size);
        void on_zoomChanged(ZoomRatio ratio);

    private:
        void cleanup();

        ProcessPanelGraphicsProxy* m_obj{};
        const ProcessViewModel* m_processViewModel{};
        ProcessPresenter* m_processPresenter{};
        ProcessView* m_processView{};

        ZoomRatio m_zoomRatio{};
};
