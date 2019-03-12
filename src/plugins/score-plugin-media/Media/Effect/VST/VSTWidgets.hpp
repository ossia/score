#pragma once
#if defined(HAS_VST2)
#include <Dataflow/UI/PortItem.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/TextItem.hpp>

#include <QDialog>

#include <Control/Widgets.hpp>
#include <Process/Inspector/GenericProcessInspector.hpp>
#include <verdigris>

namespace Media::VST
{

class VSTEffectItem final : public score::EmptyRectItem
{
  QGraphicsItem* rootItem{};
  std::vector<std::pair<VSTControlInlet*, score::EmptyRectItem*>> controlItems;
public:
  VSTEffectItem(
      const VSTEffectModel& effect,
      const score::DocumentContext& doc,
      QGraphicsItem* root);

  template <typename T>
  void setupInlet(
      T control,
      Process::ControlInlet& inlet,
      const score::DocumentContext& doc)
  {
    auto item = new score::EmptyItem{this};

    double pos_y = this->childrenBoundingRect().height();

    auto& portFactory
        = score::AppContext().interfaces<Process::PortFactoryList>();
    Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
    auto port = fact->makeItem(inlet, doc, item, this);


    auto lab = new score::SimpleTextItem{score::Skin::instance().Port2.main, item};
    lab->setText(inlet.customData());
    lab->setPos(20., 2.);
    const qreal labelHeight = 10;

    QGraphicsItem* widg = T::make_item(control, inlet, doc, nullptr, this);
    widg->setParentItem(item);
    widg->setPos(18., labelHeight + 5.);

    port->setPos(8., 4.);

    item->setPos(0, pos_y);
  }
  void setupInlet(
      const VSTEffectModel& fx,
      VSTControlInlet& inlet,
      const score::DocumentContext& doc);
};

class VSTGraphicsSlider final
    : public QObject
    , public score::QGraphicsSliderBase<VSTGraphicsSlider>
{
  W_OBJECT(VSTGraphicsSlider)
  Q_INTERFACES(QGraphicsItem)
  friend struct QGraphicsSliderBase<VSTGraphicsSlider>;

  double m_value{};
  AEffect* fx{};
  int num{};

private:
  bool m_grab{};

public:
  static const constexpr double min = 0.;
  static const constexpr double max = 1.;
  friend struct score::DefaultGraphicsSliderImpl;
  VSTGraphicsSlider(AEffect* fx, int num, QGraphicsItem* parent);

  static double map(double v) { return v; }
  static double unmap(double v) { return v; }

  void setValue(double v);
  double value() const;

  bool moving = false;

public:
  void valueChanged(double arg_1) W_SIGNAL(valueChanged, arg_1);
  void sliderMoved() W_SIGNAL(sliderMoved);
  void sliderReleased() W_SIGNAL(sliderReleased);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
};

struct VSTFloatSlider : ossia::safe_nodes::control_in
{
  static QWidget* make_widget(
      AEffect* fx,
      VSTControlInlet& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context);
  static QGraphicsItem* make_item(
      AEffect* fx,
      VSTControlInlet& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context);
};

class VSTWindow final : public QDialog
{
  W_OBJECT(VSTWindow)
public:
  static ERect getRect(AEffect& e);
  static bool hasUI(AEffect& e);

  VSTWindow(
      const VSTEffectModel& e,
      const score::DocumentContext& ctx,
      QWidget* parent);

  ~VSTWindow() override;
  void resize(int w, int h);

public:
  void uiClosing() W_SIGNAL(uiClosing);

private:
  static void setup_rect(QWidget* container, int width, int height);

  VSTWindow(const VSTEffectModel& e, const score::DocumentContext& ctx);

  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  std::weak_ptr<AEffectWrapper> effect;
  QWidget* m_defaultWidg{};
  const VSTEffectModel& m_model;
};

using LayerFactory
    = Process::EffectLayerFactory_T<VSTEffectModel, VSTEffectItem, VSTWindow>;

class VSTInspector final
    : public Process::GenericInspectorWidget<VSTEffectModel>
{
public:
  using GenericInspectorWidget::GenericInspectorWidget;
};

class VSTInspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<VSTEffectModel, VSTInspector>
{
  SCORE_CONCRETE("26a8d3ef-1a77-4d58-961b-a43d7539bc22")
};
}
#endif
