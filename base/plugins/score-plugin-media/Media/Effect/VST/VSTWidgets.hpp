#pragma once
#include <Dataflow/UI/PortItem.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/CommentBlock/TextItem.hpp>

#include <score/widgets/GraphicWidgets.hpp>

#include <QDialog>
#include <QTimer>

#include <Engine/Node/Widgets.hpp>
#include <wobjectdefs.h>

namespace Media::VST
{

class VSTEffectItem final : public score::EmptyRectItem
{
  score::RectItem* rootItem{};
  std::vector<std::pair<VSTControlInlet*, score::EmptyRectItem*>> controlItems;

public:
  VSTEffectItem(
      const VSTEffectModel& effect, const score::DocumentContext& doc,
      score::RectItem* root);

  template <typename T>
  void setupInlet(
      T control, Process::ControlInlet& inlet,
      const score::DocumentContext& doc)
  {
    auto item = new score::EmptyRectItem{this};

    double pos_y = this->childrenBoundingRect().height();

    auto& portFactory
        = score::AppContext().interfaces<Process::PortFactoryList>();
    Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
    auto port = fact->makeItem(inlet, doc, item, this);

    auto lab = new Scenario::SimpleTextItem{item};
    lab->setColor(ScenarioStyle::instance().EventDefault);
    lab->setText(inlet.customData());
    lab->setPos(15, 2);

    QGraphicsItem* widg = T::make_item(control, inlet, doc, nullptr, this);
    widg->setParentItem(item);
    widg->setPos(15, lab->boundingRect().height());

    auto h = std::max(
        20., (qreal)(
                 widg->boundingRect().height() + lab->boundingRect().height()
                 + 2.));

    port->setPos(7., h / 2.);

    item->setPos(0, pos_y);
    item->setRect(QRectF{0., 0, 170., h});
  }
  void setupInlet(
      const VSTEffectModel& fx, VSTControlInlet& inlet,
      const score::DocumentContext& doc);
};

class VSTGraphicsSlider final : public QObject, public QGraphicsItem
{
  W_OBJECT(VSTGraphicsSlider)
  Q_INTERFACES(QGraphicsItem)

  double m_value{};
  QRectF m_rect;
  AEffect* fx{};
  int num{};

private:
  bool m_grab{};

public:
  static const constexpr double min = 0.;
  static const constexpr double max = 1.;
  friend struct score::DefaultGraphicsSliderImpl;
  VSTGraphicsSlider(AEffect* fx, int num, QGraphicsItem* parent);

  static double map(double v)
  {
    return v;
  }
  static double unmap(double v)
  {
    return v;
  }

  void setRect(QRectF r);
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
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;
};

struct VSTFloatSlider : ossia::safe_nodes::control_in
{
  static QGraphicsItem* make_item(
      AEffect* fx, VSTControlInlet& inlet, const score::DocumentContext& ctx,
      QWidget* parent, QObject* context);
};

class VSTWindow final : public QDialog
{
  W_OBJECT(VSTWindow)
public:
  static ERect getRect(AEffect& e);
  static bool hasUI(AEffect& e);

  VSTWindow(
      const VSTEffectModel& e, const score::DocumentContext& ctx,
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
}
