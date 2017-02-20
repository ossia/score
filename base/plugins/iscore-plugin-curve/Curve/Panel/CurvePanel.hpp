#pragma once
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveStyle.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <QActionGroup>
#include <QGraphicsLineItem>
#include <QScrollBar>
#include <QVBoxLayout>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/DoubleSlider.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{

template <typename PanelProxy_T>
struct PanelFocusDispatcher
{
  void focus(Curve::Presenter* p)
  {
  }
};

template <typename PanelProxy_T>
struct PanelBaseContext : public iscore::DocumentContext
{
  PanelBaseContext(const iscore::DocumentContext& doc)
      : iscore::DocumentContext{doc}, focusDispatcher{}
  {
  }

  PanelFocusDispatcher<PanelProxy_T> focusDispatcher;
};

template <typename PanelProxy_T>
struct PanelContext
{
  PanelBaseContext<PanelProxy_T> context;
  Curve::Presenter& presenter;
};

template <typename Layer_T>
class CurvePanelProxy : public Process::LayerModelPanelProxy
{
  using context_t = PanelContext<CurvePanelProxy<Layer_T>>;

public:
  CurvePanelProxy(
                const Layer_T& layermodel,
                QObject* parent):
            LayerModelPanelProxy{parent},
            m_layer{layermodel},
            m_widget{new QWidget},
            //m_scene{new QGraphicsScene{this}},
            m_view{new ProcessGraphicsView{nullptr, m_widget}},
            m_processEnd{new QGraphicsLineItem{}},
            m_curveView{new Curve::View{nullptr}},
            m_curvePresenter{
                [this] () {

            // Add the items to the scene early because
            // the presenters might call scene() in their ctor.
            //m_scene->addItem(m_curveView);

            // Setup the model
            auto fact = iscore::AppContext()
                    .components
                    .interfaces<Process::LayerFactoryList>()
                    .get(m_layer.concreteKey());

            if(!fact)
                throw;

            auto istyle = dynamic_cast<Curve::StyleInterface*>(fact);
            ISCORE_ASSERT(istyle);

            // TODO find a way to not call documentcontext twice in this ctor
            return new Curve::Presenter{
                    iscore::IDocument::documentContext(m_layer),
                    istyle->style(),
                    m_layer.processModel().curve(),
                    m_curveView,
                    this};
                } () // This lambda function gets called and returns what we need
            },

            m_context{{iscore::IDocument::documentContext(layermodel)}, *m_curvePresenter},
            m_sm{m_context, *m_curvePresenter}
  {
    // Setup the view
    //m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    QPen p{Qt::DashLine};
    p.setWidth(2);
    p.setColor(QColor::fromRgba(qRgba(100, 100, 100, 100)));
    m_processEnd->setPen(p);
    //m_scene->addItem(m_processEnd);

    m_widget->setLayout(new QVBoxLayout);

    m_widget->layout()->addWidget(m_view);

    //m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    //m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(
        m_view, &ProcessGraphicsView::sizeChanged, this,
        &CurvePanelProxy::on_sizeChanged);

    m_zoomSlider = new iscore::DoubleSlider{m_widget};
    m_zoomSlider->setValue(0.03); // 30 seconds by default on an average screen

    connect(
        m_zoomSlider, &iscore::DoubleSlider::valueChanged, this,
        &CurvePanelProxy::on_zoomChanged);
    m_widget->layout()->addWidget(m_zoomSlider);
    m_view->show();

    m_curveView->setFlag(QQuickPaintedItem::ItemClipsChildrenToShape, false);
    m_view->addActions(m_curvePresenter->actions().actions());
    m_curvePresenter->editionSettings().setTool(Curve::Tool::Select);
    m_curvePresenter->setBoundedMove(false);

    con(m_layer.processModel().curve(), &Curve::Model::changed, this,
        &CurvePanelProxy::on_curveChanged);

    // connect(&m_context.context.updateTimer, &QTimer::timeout, this,
    // [&vp=*m_view->viewport()] { vp.update(); } );

    on_curveChanged();
    // Have a zoom here too. For now the process should be the size of the
    // window.
    on_sizeChanged(m_view->size());

    on_zoomChanged(0.03);
  }

  virtual ~CurvePanelProxy()
  {
    delete m_curvePresenter;
    delete m_widget;
  }

  void on_curveChanged()
  {
    // Find the last point

    auto& points = m_layer.processModel().curve().points();
    m_currentCurveMax = 0;
    for (Curve::PointModel* point : points)
    {
      auto x = point->pos().x();
      if (x > m_currentCurveMax)
        m_currentCurveMax = x;
    }
  }

  // Can return the same view model, or a new one.
  const Layer_T& layer() final override
  {
    return m_layer;
  }

  QWidget* widget() const final override
  {
    return m_widget;
  }

  void on_sizeChanged(const QSize& size)
  {
    //m_height = size.height() - m_view->horizontalScrollBar()->height() - 2;
    m_width = size.width();

    recompute();
  }

  void on_zoomChanged(ZoomRatio newzoom)
  {
    // TODO refactor this with what's in base element model
    // mapZoom maps a value between 0 and 1 to the correct zoom.
    // TODO use the fma ease
    auto mapZoom = [](double val, double min, double max) {
      return (max - min) * val + min;
    };

    auto duration = m_layer.processModel().duration().msec();
    if (m_currentCurveMax > 1)
      duration *= m_currentCurveMax;

    m_zoomRatio
        = mapZoom(1.0 - newzoom, 1.5, std::max(5., 2 * duration) / m_width);

    recompute();
  }

  auto& toolPalette()
  {
    return m_sm;
  }

private:
  void recompute()
  {
    // computedMax : the number of pixels in a millisecond when the whole
    // constraint
    // is displayed on screen;
    // We want the value to be at least twice the duration of the constraint
    const auto& duration = m_layer.processModel().duration();
    auto fullWidth = duration.toPixels(m_zoomRatio);
    if (m_currentCurveMax > 1)
      fullWidth *= m_currentCurveMax;

    m_processEnd->setLine(fullWidth, 0, fullWidth, m_height);

    auto maxWidth = std::max(fullWidth * 1.2, m_width) * 1.5;
    //m_view->setSceneRect({0, 0, maxWidth, m_height});

    m_curveView->setRect({0, 0, maxWidth, m_height});
    m_curvePresenter->setRect(QRectF{0, 0, fullWidth, m_height});
  }

  double m_currentCurveMax{};

  double m_height{}, m_width{};

  const Layer_T& m_layer;
  QWidget* m_widget{};

  //QGraphicsScene* m_scene{};
  ProcessGraphicsView* m_view{};
  iscore::DoubleSlider* m_zoomSlider{};

  QGraphicsLineItem* m_processEnd{};
  Curve::View* m_curveView{};
  Curve::Presenter* m_curvePresenter{};

  context_t m_context;

  Curve::ToolPalette_T<context_t> m_sm;
  ZoomRatio m_zoomRatio{};
};
}
