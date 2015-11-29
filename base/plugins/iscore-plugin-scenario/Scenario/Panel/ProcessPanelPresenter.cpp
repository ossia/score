#include <Process/LayerModelPanelProxy.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/Widgets/ScenarioBaseGraphicsView.hpp>
#include <QGraphicsScene>
#include <QObject>
#include <QSize>
#include <algorithm>

#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include "Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp"
#include "Scenario/Panel/ProcessPanelGraphicsProxy.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/tools/Todo.hpp>

#include "ProcessPanelId.hpp"
namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

ProcessPanelPresenter::ProcessPanelPresenter(
        const DynamicProcessList& plist,
        iscore::Presenter* parent_presenter,
        iscore::PanelView* view):
    iscore::PanelPresenter{parent_presenter, view},
    m_processList{plist}
{
    auto v = static_cast<ProcessPanelView*>(view);
    connect(v, &ProcessPanelView::horizontalZoomChanged,
            this, &ProcessPanelPresenter::on_zoomChanged);
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

    auto bem = iscore::IDocument::try_get<ScenarioDocumentModel>(*iscore::IDocument::documentFromObject(model()));

    if(!bem)
        return;

    con(bem->focusManager(),  &ProcessFocusManager::sig_focusedViewModel,
        this, &ProcessPanelPresenter::on_focusedViewModelChanged);

    auto panelview = static_cast<ProcessPanelView*>(view());
    connect(panelview, &ProcessPanelView::sizeChanged,
            this, &ProcessPanelPresenter::on_sizeChanged);

    on_focusedViewModelChanged(bem->focusManager().focusedViewModel());
}

void ProcessPanelPresenter::on_focusedViewModelChanged(const LayerModel* theLM)
{
    if(theLM != m_layerModel)
    {
        m_layerModel = theLM;
        delete m_processPresenter;
        m_processPresenter = nullptr;

        if(!m_layerModel)
            return;

        auto& sharedmodel = m_layerModel->processModel();
        auto fact = m_processList.list().get(sharedmodel.key());

        auto proxy = m_layerModel->make_panelProxy(this);

        delete m_obj;
        m_obj = new ProcessPanelGraphicsProxy{*theLM, *this};
        // Add the items to the scene early because
        // the presenters might call scene() in their ctor.
        auto panelview = static_cast<ProcessPanelView*>(view());
        panelview->scene()->addItem(m_obj);

        m_layer = fact->makeLayerView(proxy->layer(),
                                       m_obj);

        m_processPresenter = fact->makeLayerPresenter(proxy->layer(),
                                                 m_layer,
                                                 this);

        connect(m_layerModel, &QObject::destroyed,
                this, &ProcessPanelPresenter::cleanup);


        // Have a zoom here too. For now the process should be the size of the window.
        on_sizeChanged(panelview->view()->size());

        on_zoomChanged(0.03);
    }
}

void ProcessPanelPresenter::on_sizeChanged(const QSize& size)
{
    if(!m_processPresenter)
        return;

    m_processPresenter->setHeight(size.height());
    m_processPresenter->parentGeometryChanged();

    auto fullWidth = m_processPresenter->layerModel().processModel().duration().toPixels(m_zoomRatio);

    m_obj->setSize(QSizeF{(double)fullWidth, (double)size.height()});
}

void ProcessPanelPresenter::on_zoomChanged(ZoomRatio newzoom)
{
    // TODO refactor this with what's in base element model
    // mapZoom maps a value between 0 and 1 to the correct zoom.
    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    // computedMax : the number of pixels in a millisecond when the whole constraint
    // is displayed on screen;

    // We want the value to be at least twice the duration of the constraint
    const auto& viewsize = static_cast<ProcessPanelView*>(view())->view()->size();
    const auto& duration =  m_layerModel->processModel().duration();

    m_zoomRatio = mapZoom(1.0 - newzoom,
                          2.,
                          std::max(4., 2 * duration.msec() / viewsize.width()));

    auto panelview = static_cast<ProcessPanelView*>(view());
    panelview->view()->setSceneRect(
                0, 0, duration.toPixels(m_zoomRatio) * 1.2,  viewsize.height());

    m_processPresenter->on_zoomRatioChanged(m_zoomRatio);

    auto fullWidth = duration.toPixels(m_zoomRatio);
    m_processPresenter->setWidth(fullWidth);

    m_processPresenter->parentGeometryChanged();
    m_obj->setSize(QSizeF{(double)fullWidth, (double)viewsize.height()});
}

void ProcessPanelPresenter::cleanup()
{
    m_layerModel = nullptr;

    delete m_processPresenter; // Will delete the view, too
    m_processPresenter = nullptr;

    delete m_obj;
    m_obj = nullptr;
}
