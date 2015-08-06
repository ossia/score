#include "SpaceLayerPresenter.hpp"
#include "SpaceLayerModel.hpp"
#include "SpaceLayerView.hpp"
#include "SpaceProcess.hpp"

#include <QGraphicsScene>
#include "Widgets/SpaceGuiWindow.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QMainWindow>
SpaceLayerPresenter::SpaceLayerPresenter(const LayerModel& model, LayerView* view, QObject* parent):
    LayerPresenter{"LayerPresenter", parent},
    m_model{static_cast<const SpaceLayerModel&>(model)},
    m_view{static_cast<SpaceLayerView*>(view)}
{
    const SpaceProcess& procmodel = static_cast<SpaceProcess&>(m_model.processModel());
    m_spaceWindowView = new QMainWindow;
    m_spaceWindowView->setCentralWidget(new SpaceGuiWindow{iscore::IDocument::documentFromObject(model)->commandStack(), procmodel, m_spaceWindowView});

    connect(m_view, SIGNAL(guiRequested()), m_spaceWindowView, SLOT(show()));
    for(const auto& area : ::model(m_model).areas())
    {
        on_areaAdded(area);
    }

    connect(&procmodel, &SpaceProcess::areaAdded, this, &SpaceLayerPresenter::on_areaAdded);
}

SpaceLayerPresenter::~SpaceLayerPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

void SpaceLayerPresenter::setWidth(int width)
{
    m_view->setWidth(width);
    update();
}

void SpaceLayerPresenter::setHeight(int height)
{
    m_view->setHeight(height);
    update();
}


void SpaceLayerPresenter::putToFront()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
    update();
}

void SpaceLayerPresenter::putBehind()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    update();
}

void SpaceLayerPresenter::on_zoomRatioChanged(ZoomRatio)
{
    ISCORE_TODO;
    update();
}

void SpaceLayerPresenter::parentGeometryChanged()
{
    ISCORE_TODO;
    update();
}

const LayerModel &SpaceLayerPresenter::layerModel() const
{
    return m_model;
}

const id_type<Process> &SpaceLayerPresenter::modelId() const
{
    return m_model.processModel().id();
}

void SpaceLayerPresenter::update()
{
    m_view->update();
    for(auto& pres : m_areas)
    {
        pres.update();
    }
}

void SpaceLayerPresenter::on_areaAdded(const AreaModel & a)
{
    auto area_pres = new AreaPresenter{a, m_view, this};

    m_areas.insert(area_pres);
    update();
}
