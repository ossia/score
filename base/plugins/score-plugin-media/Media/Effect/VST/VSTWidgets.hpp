#pragma once
#include <Media/Effect/VST/VSTEffectModel.hpp>

#include <QTimer>

namespace Media::VST
{

class VSTEffectItem final:
    public score::EmptyRectItem
{
    score::RectItem* rootItem{};
    std::vector<std::pair<VSTControlInlet*, score::EmptyRectItem*>> controlItems;
  public:
    VSTEffectItem(
        const VSTEffectModel& effect, const score::DocumentContext& doc, score::RectItem* root);

    template<typename T>
    void setupInlet(
        T control,
        Process::ControlInlet& inlet,
        const score::DocumentContext& doc)
    {
      auto item = new score::EmptyRectItem{this};

      double pos_y = this->childrenBoundingRect().height();

      auto port = Dataflow::setupInlet(inlet, doc, item, this);

      auto lab = new Scenario::SimpleTextItem{item};
      lab->setColor(ScenarioStyle::instance().EventDefault);
      lab->setText(inlet.customData());
      lab->setPos(15, 2);


      QGraphicsItem* widg = T::make_item(control, inlet, doc, nullptr, this);
      widg->setParentItem(item);
      widg->setPos(15, lab->boundingRect().height());

      auto h = std::max(20., (qreal)(widg->boundingRect().height() + lab->boundingRect().height() + 2.));

      port->setPos(7., h / 2.);

      item->setPos(0, pos_y);
      item->setRect(QRectF{0., 0, 170., h});
    }
    void setupInlet(
        const VSTEffectModel& fx,
        VSTControlInlet& inlet,
        const score::DocumentContext& doc);
};

class VSTGraphicsSlider final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT

    double m_value{};
    QRectF m_rect;
    AEffect* fx{};
    int num{};
  private:
    bool m_grab;
  public:
    VSTGraphicsSlider(AEffect* fx, int num, QGraphicsItem* parent);

    void setRect(QRectF r);
    void setValue(double v);
    double value() const;

    bool moving = false;
  Q_SIGNALS:
    void valueChanged(double);
    void sliderMoved();
    void sliderReleased();

  private:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    bool isInHandle(QPointF p);
    double getHandleX() const;
    QRectF sliderRect() const;
    QRectF handleRect() const;
};

class VSTWindow final: public QDialog
{
    Q_OBJECT
  public:
    static ERect getRect(AEffect& e);

    VSTWindow(const VSTEffectModel& e, const score::DocumentContext& ctx, QWidget* parent):
      VSTWindow{e.fx, getRect(*e.fx->fx)}
    {
      connect(&ctx.coarseUpdateTimer, &QTimer::timeout,
              this, [=] {
        if(auto eff = effect.lock())
          eff->fx->dispatcher(eff->fx, effEditIdle, 0, 0, nullptr, 0);
      }, Qt::UniqueConnection);
    }

    ~VSTWindow();
  Q_SIGNALS:
    void uiClosing();
  private:
    VSTWindow(std::shared_ptr<AEffectWrapper> effect, ERect rect);
    void closeEvent(QCloseEvent* event) override;

    std::weak_ptr<AEffectWrapper> effect;

};

using LayerFactory = Process::EffectLayerFactory_T<VSTEffectModel, VSTEffectItem, VSTWindow>;

}
