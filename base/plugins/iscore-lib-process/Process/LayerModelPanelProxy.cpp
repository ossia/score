#include "LayerModelPanelProxy.hpp"
#include <iscore/widgets/DoubleSlider.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Tools/ProcessPanelGraphicsProxy.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
Process::LayerModelPanelProxy::~LayerModelPanelProxy()
{

}

#include <QLayout>

Process::GraphicsViewLayerModelPanelProxy::GraphicsViewLayerModelPanelProxy(
        const Process::LayerModel& model,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_layer{model}
{
    // Setup the view
    m_widget = new QWidget;
    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    m_widget->setLayout(new QVBoxLayout);
    m_view = new ProcessGraphicsView{m_scene};

    m_widget->layout()->addWidget(m_view);

    //m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_view, &ProcessGraphicsView::sizeChanged,
            this,   &GraphicsViewLayerModelPanelProxy::on_sizeChanged);


    m_zoomSlider = new DoubleSlider{m_widget};
    m_zoomSlider->setValue(0.03); // 30 seconds by default on an average screen

    connect(m_zoomSlider, &DoubleSlider::valueChanged,
            this,         &GraphicsViewLayerModelPanelProxy::on_zoomChanged);
    m_widget->layout()->addWidget(m_zoomSlider);
    m_view->show();


    // Setup the model
    auto& sharedmodel = m_layer.processModel();
    auto fact = iscore::AppContext().components.factory<ProcessList>().get(sharedmodel.concreteFactoryKey());


    m_obj = new ProcessPanelGraphicsProxy{};
    // Add the items to the scene early because
    // the presenters might call scene() in their ctor.
    m_scene->addItem(m_obj);

    m_layerView = fact->makeLayerView(m_layer, m_obj);

    m_processPresenter = fact->makeLayerPresenter(m_layer,
                                                  m_layerView,
                                             this);



    // Have a zoom here too. For now the process should be the size of the window.
    on_sizeChanged(m_view->size());

    on_zoomChanged(0.03);

}

Process::GraphicsViewLayerModelPanelProxy::~GraphicsViewLayerModelPanelProxy()
{
    if(m_processPresenter)
    {
        deleteGraphicsObject(m_layerView);
        delete m_processPresenter;
        m_processPresenter = nullptr;
    }

    delete m_obj;
    m_obj = nullptr;

}

const Process::LayerModel&Process::GraphicsViewLayerModelPanelProxy::layer()
{
    return m_layer;
}

QWidget*Process::GraphicsViewLayerModelPanelProxy::widget() const
{
    return m_widget;
}

void Process::GraphicsViewLayerModelPanelProxy::on_sizeChanged(const QSize& size)
{
    m_processPresenter->setHeight(size.height());
    m_processPresenter->parentGeometryChanged();

    double fullWidth = m_processPresenter->layerModel().processModel().duration().toPixels(m_zoomRatio);

    m_obj->setSize(QSizeF{(double)fullWidth, (double)size.height()});
}

void Process::GraphicsViewLayerModelPanelProxy::on_zoomChanged(ZoomRatio newzoom)
{
    // TODO refactor this with what's in base element model
    // mapZoom maps a value between 0 and 1 to the correct zoom.
    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    // computedMax : the number of pixels in a millisecond when the whole constraint
    // is displayed on screen;

    // We want the value to be at least twice the duration of the constraint
    const auto& viewsize = m_view->size();
    const auto& duration =  m_layer.processModel().duration();

    m_zoomRatio = mapZoom(1.0 - newzoom,
                          2.,
                          std::max(4., 2 * duration.msec() / viewsize.width()));

    m_view->setSceneRect(
                0, 0, duration.toPixels(m_zoomRatio) * 1.2,  viewsize.height());

    m_processPresenter->on_zoomRatioChanged(m_zoomRatio);

    auto fullWidth = duration.toPixels(m_zoomRatio);
    m_processPresenter->setWidth(fullWidth);

    m_processPresenter->parentGeometryChanged();
    m_obj->setSize(QSizeF{(double)fullWidth, (double)viewsize.height()});
}
