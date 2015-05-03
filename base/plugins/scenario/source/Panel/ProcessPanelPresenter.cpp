#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include "ProcessPanelModel.hpp"

#include <QApplication>
#include <Document/BaseElement/BaseElementModel.hpp>

#include <Document/BaseElement/Widgets/GraphicsProxyObject.hpp>

#include <ProcessInterface/ProcessList.hpp>
#include <ProcessInterface/ProcessModel.hpp>
#include <ProcessInterface/ProcessViewModel.hpp>
#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>
#include <ProcessInterface/ProcessPresenter.hpp>
#include <ProcessInterface/ProcessViewInterface.hpp>
#include <ProcessInterface/ProcessFactoryInterface.hpp>

#include <Document/BaseElement/Widgets/SizeNotifyingGraphicsView.hpp>

#include "ProcessPanelId.hpp"


ProcessPanelPresenter::ProcessPanelPresenter(iscore::Presenter* parent_presenter, iscore::PanelViewInterface* view):
    iscore::PanelPresenterInterface{parent_presenter, view},
    m_obj{new GraphicsProxyObject}
{
    auto panelview = static_cast<ProcessPanelView*>(view);
    panelview->scene()->addItem(m_obj);
}

int ProcessPanelPresenter::panelId() const
{
    return PROCESS_PANEL_ID;
}

void ProcessPanelPresenter::on_modelChanged()
{
    auto panelview = static_cast<ProcessPanelView*>(view());

    m_baseElementModel =
            iscore::IDocument::documentFromObject(model())
               ->findChild<BaseElementModel*>("BaseElementModel");

    if(!m_baseElementModel) return;

    connect(m_baseElementModel,  &BaseElementModel::focusedViewModelChanged,
            this, &ProcessPanelPresenter::on_focusedViewModelChanged);

    connect(panelview, &ProcessPanelView::sizeChanged,
            this, &ProcessPanelPresenter::on_sizeChanged);
}

void ProcessPanelPresenter::on_focusedViewModelChanged()
{
    auto panelview = static_cast<ProcessPanelView*>(view());

    auto thePVM = m_baseElementModel->focusedViewModel();
    if(thePVM != m_processViewModel)
    {
        m_processViewModel = thePVM;
        delete m_processPresenter;
        m_processPresenter = nullptr;

        if(!thePVM)
            return;

        auto fact = ProcessList::getFactory(m_processViewModel
                                              ->sharedProcessModel()
                                                .processName());

        auto proxy = m_processViewModel->make_panelProxy(this);
        delete m_obj;
        m_obj = new GraphicsProxyObject;
        panelview->scene()->addItem(m_obj);
        m_processView = fact->makeView(proxy->viewModel(),
                                       m_obj);
        m_processPresenter = fact->makePresenter(proxy->viewModel(),
                                                 m_processView,
                                                 this);


        m_processPresenter->on_zoomRatioChanged(1.0);
        on_sizeChanged(panelview->view()->size());
    }
}

void ProcessPanelPresenter::on_sizeChanged(const QSize& size)
{
    if(!m_processPresenter)
        return;

    auto panelview = static_cast<ProcessPanelView*>(view());
    m_processView->setPos(0, 0);
    m_processPresenter->setHeight(size.height());
    m_processPresenter->setWidth(size.width());
    m_processPresenter->parentGeometryChanged();

    panelview->view()->fitInView(m_processView);

}
