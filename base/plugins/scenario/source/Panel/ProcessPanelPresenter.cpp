#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include "ProcessPanelModel.hpp"

#include <QApplication>
#include <Document/BaseElement/BaseElementModel.hpp>

#include <Document/BaseElement/Widgets/GraphicsProxyObject.hpp>

#include <ProcessInterface/ProcessList.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <ProcessInterface/ProcessViewModelInterface.hpp>
#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>
#include <ProcessInterface/ProcessPresenterInterface.hpp>
#include <ProcessInterface/ProcessViewInterface.hpp>
#include <ProcessInterface/ProcessFactoryInterface.hpp>


#include <iscore/document/DocumentInterface.hpp>
#include <Document/BaseElement/Widgets/SizeNotifyingGraphicsView.hpp>

#include <QDebug>
#include <QGraphicsScene>

QString ProcessPanelPresenter::modelObjectName() const
{
    return "ProcessPanelModel";
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

        // TODO Leak
        auto gobj = new GraphicsProxyObject;

        auto fact = ProcessList::getFactory(m_processViewModel->sharedProcessModel()->processName());

        auto proxy = m_processViewModel->make_panelProxy();

        // TODO pvm::correspondingView ?
        m_processView = fact->makeView(proxy->viewModel(), gobj);
        m_processView->setHeight(panelview->view()->size().height());
        m_processView->setWidth(panelview->view()->size().width());

        // TODO pvm::correspondingPresenter ?
        m_processPresenter = fact->makePresenter(proxy->viewModel(), m_processView, this);

        panelview->scene()->addItem(gobj);
    }
}

void ProcessPanelPresenter::on_sizeChanged(const QSize& size)
{
    m_processView->setHeight(size.height());
    m_processView->setWidth(size.width());
    m_processView->setScale(2);
}
