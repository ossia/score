#pragma once
#include <Effect/EffectModel.hpp>
#include <Effect/EffectFactory.hpp>
#include <QJsonDocument>
#include <Media/Effect/VST/VSTLoader.hpp>
#include <Media/Effect/EffectExecutor.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Process/Dataflow/PortFactory.hpp>
namespace Media::VST
{
class VSTEffectModel;
class VSTControlInlet;
}
EFFECT_METADATA(, Media::VST::VSTEffectModel, "BE8E6BD3-75F2-4102-8895-8A4EB4EA545A", "VST", "VST", "", {})
UUID_METADATA(, Process::Port, Media::VST::VSTControlInlet, "e523bc44-8599-4a04-94c1-04ce0d1a692a")

namespace Media::VST
{
class VSTControlInlet : public Process::Inlet
{
    Q_OBJECT
    SCORE_SERIALIZE_FRIENDS
  public:
      MODEL_METADATA_IMPL(VSTControlInlet)
    using Process::Inlet::Inlet;

    VSTControlInlet(DataStream::Deserializer& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }
    VSTControlInlet(JSONObject::Deserializer& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }
    VSTControlInlet(DataStream::Deserializer&& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }
    VSTControlInlet(JSONObject::Deserializer&& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }

    int fxNum{};


    float value() const { return m_value; }
    void setValue(float v)
    {
      if(v != m_value)
      {
        m_value = v;
        emit valueChanged(v);
      }
    }
  signals:
    void valueChanged(float);

  private:
    float m_value{};
};


using VSTControlInletFactory = Process::PortFactory_T<VSTControlInlet>;
struct AEffectWrapper
{
    AEffect* fx{};
    VstTimeInfo info;

    AEffectWrapper(AEffect* f): fx{f}
    {

    }

    auto getParameter(VstInt32 index)
    {
      return fx->getParameter(fx, index);
    }
    auto setParameter(VstInt32 index, float p)
    {
      return fx->setParameter(fx, index, p);
    }

    auto dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      return fx->dispatcher(fx, opcode, index, value, ptr, opt);
    }

    ~AEffectWrapper()
    {
      if(fx)
      {
        fx->dispatcher(fx, effStopProcess, 0, 0, nullptr, 0.f);
        fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0.f);
        fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
      }
    }
};

class CreateVSTControl;
class VSTControlInlet;
class VSTEffectModel :
    public Process::EffectModel
{
    Q_OBJECT
    SCORE_SERIALIZE_FRIENDS
        friend class Media::VST::CreateVSTControl;
    public:
      MODEL_METADATA_IMPL(VSTEffectModel)
    VSTEffectModel(
    const QString& name,
      const Id<EffectModel>&,
            QObject* parent);


    ~VSTEffectModel() override;
    template<typename Impl>
    VSTEffectModel(
        Impl& vis,
        QObject* parent) :
      EffectModel{vis, parent}
    {
      init();
      vis.writeTo(*this);
    }

    const QString& effect() const
    { return m_effectPath; }

    void setEffect(const QString& s)
    { m_effectPath = s; }
    VSTControlInlet* getControl(const Id<Process::Port>& p);
    QString prettyName() const override;

    std::shared_ptr<AEffectWrapper> fx{};
    VstIntPtr ui{};

    std::unordered_map<int, VSTControlInlet*> controls;


  signals:
    void addControl(int idx, float v);

  private slots:
    void on_addControl(int idx, float v);
  private:
    QString getString(AEffectOpcodes op, int param);
    void init();
    void create();
    void load();
    void showUI() override;
    void hideUI() override;
    QString m_effectPath;

    auto dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      return fx->dispatch(opcode, index, value, ptr, opt);
    }

    void closePlugin();
    void initFx(VSTModule& plugin);
};
using VSTEffectFactory = Process::EffectFactory_T<VSTEffectModel>;

class VSTEffectItem:
    public Control::EffectItem
{
    Control::RectItem* rootItem{};
  public:
    VSTEffectItem(
        const VSTEffectModel& effect, const score::DocumentContext& doc, Control::RectItem* root);

    template<typename T>
    void setupInlet(
        T control,
        Process::ControlInlet& inlet,
        const score::DocumentContext& doc)
    {
      auto item = new Control::RectItem{this};

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

class VSTGraphicsSlider
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
  signals:
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


using VSTUIEffectFactory = Process::EffectUIFactory_T<VSTEffectModel, VSTEffectItem>;
}

namespace Engine::Execution
{
class VSTEffectComponent
    : public Engine::Execution::EffectComponent_T<Media::VST::VSTEffectModel>
{
    Q_OBJECT
    COMPONENT_METADATA("84bb8af9-bfb9-4819-8427-79787de716f3")

    public:
      static constexpr bool is_unique = true;

    VSTEffectComponent(
        Media::VST::VSTEffectModel& proc,
        const Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
};
using VSTEffectComponentFactory = Engine::Execution::EffectComponentFactory_T<VSTEffectComponent>;
}
