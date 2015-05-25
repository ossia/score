#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include "ProcessPanelModel.hpp"

#include <QApplication>
#include <Document/BaseElement/BaseElementModel.hpp>

#include <ProcessInterface/ProcessList.hpp>
#include <ProcessInterface/ProcessModel.hpp>
#include <ProcessInterface/ProcessViewModel.hpp>
#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>
#include <ProcessInterface/ProcessPresenter.hpp>
#include <ProcessInterface/ProcessView.hpp>
#include <ProcessInterface/ProcessFactory.hpp>

#include <Document/BaseElement/Widgets/SizeNotifyingGraphicsView.hpp>

#include "ProcessPanelId.hpp"
#include "Document/BaseElement/ProcessFocusManager.hpp"


ProcessPanelPresenter::ProcessPanelPresenter(iscore::Presenter* parent_presenter, iscore::PanelView* view):
    iscore::PanelPresenter{parent_presenter, view}
{
}

int ProcessPanelPresenter::panelId() const
{
    return PROCESS_PANEL_ID;
}

void ProcessPanelPresenter::on_modelChanged()
{
    if(!model())
    {
        cleanup();
        return;
    }

    auto bem = iscore::IDocument::documentFromObject(model())
                    ->findChild<BaseElementModel*>("BaseElementModel");

    if(!bem)
        return;

    connect(&bem->focusManager(),  &ProcessFocusManager::sig_focusedViewModel,
            this, &ProcessPanelPresenter::on_focusedViewModelChanged);

    auto panelview = static_cast<ProcessPanelView*>(view());
    connect(panelview, &ProcessPanelView::sizeChanged,
            this, &ProcessPanelPresenter::on_sizeChanged);

    on_focusedViewModelChanged(bem->focusManager().focusedViewModel());
}

void ProcessPanelPresenter::on_focusedViewModelChanged(const ProcessViewModel* thePVM)
{
    if(thePVM != m_processViewModel)
    {
        m_processViewModel = thePVM;
        delete m_processPresenter;
        m_processPresenter = nullptr;

        if(!m_processViewModel)
            return;

        auto& sharedmodel = m_processViewModel->sharedProcessModel();
        auto fact = ProcessList::getFactory(sharedmodel.processName());

        auto proxy = m_processViewModel->make_panelProxy(this);

        delete m_obj;
        m_obj = new ProcessPanelGraphicsProxy{*thePVM, *this};

        m_processView = fact->makeView(proxy->viewModel(),
                                       m_obj);

        m_processPresenter = fact->makePresenter(proxy->viewModel(),
                                                 m_processView,
                                                 this);

        connect(m_processViewModel, &QObject::destroyed,
                this, &ProcessPanelPresenter::cleanup);

        // Add the items to the scene.
        auto panelview = static_cast<ProcessPanelView*>(view());
        panelview->scene()->addItem(m_obj);

        // Have a zoom here too. For now the process should be the size of the window.
        on_sizeChanged(panelview->view()->size());
    }
}

void ProcessPanelPresenter::on_sizeChanged(const QSize& size)
{
    if(!m_processPresenter)
        return;

    // Compute milliseconds per pixel (half, to try).
    auto msec = m_processPresenter->viewModel().sharedProcessModel().duration().msec();
    m_zoomRatio = 2 * msec / size.width();
    m_processPresenter->on_zoomRatioChanged(m_zoomRatio);

    m_obj->setSize(size);
    auto panelview = static_cast<ProcessPanelView*>(view());
    m_processView->setPos(0, 0);
    m_processPresenter->setHeight(size.height());
    m_processPresenter->setWidth(size.width());
    m_processPresenter->parentGeometryChanged();

    panelview->view()->fitInView(m_processView);

}

void ProcessPanelPresenter::cleanup()
{
    m_processViewModel = nullptr;

    delete m_processPresenter; // Will delete the view, too
    m_processPresenter = nullptr;

    delete m_obj;
    m_obj = nullptr;
}
