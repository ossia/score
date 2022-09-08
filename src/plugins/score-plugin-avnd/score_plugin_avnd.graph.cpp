
#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/Skin.hpp>

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QListWidget>
#include <QSplitter>

#include <Avnd/Factories.hpp>
#include <halp/audio.hpp>
#include <halp/midifile_port.hpp>

#include <avnd/../../examples/Advanced/Graph/Graph.hpp>

namespace oscr
{
struct Port
{
  explicit Port(QString name)
      : name{name}
  {
  }

  QString name;
  QPointF pos;
  QGraphicsItem* widget{};
};

struct EdgeItem : public QGraphicsItem
{
  explicit EdgeItem(QGraphicsItem* parent)
      : QGraphicsItem{parent}
  {
  }

  QRectF boundingRect() const override
  {
    auto r = QRectF{p1, p2}.normalized();
    return {0, 0, r.width(), r.height()};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    auto& style = Process::Style::instance();

    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setPen(style.skin.Light.lighter.pen3_solid_round_round);
    painter->drawLine(mapFromScene(p1), mapFromScene(p2));
    painter->setRenderHint(QPainter::Antialiasing, false);

    // painter->drawRect(boundingRect());
    // painter->setPen(QPen(Qt::red, 10));
    // painter->drawPoint(boundingRect().topLeft());
  }

  void setLine(QPointF sceneA, QPointF sceneB)
  {
    prepareGeometryChange();
    this->p1 = sceneA;
    this->p2 = sceneB;

    auto r = QRectF{sceneA, sceneB}.normalized();
    setPos(r.topLeft());
    update();
  }
  QPointF p1, p2;
};

struct NodeItem : public QGraphicsItem
{
  explicit NodeItem(QGraphicsItem* parent)
      : QGraphicsItem{parent}
  {
    setCursor(QCursor());
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsFocusable, true);
    setFlag(ItemClipsChildrenToShape, true);
  }

  void setLabel(QString label) { m_label = label; }
  void addInlet(QString name)
  {
    m_inlets.emplace_back(name);

    updateRect();
  }
  void addOutlet(QString name)
  {
    int n = m_outlets.size();
    m_outlets.emplace_back(name);
    m_outlets.back().pos
        = firstOutletPos() + QPointF{radius / 2., radius / 2. + n * diameter};
  }

  void updateRect()
  {
    double max_w = 5 * m_label.length() + 40;

    prepareGeometryChange();

    auto cur_pos = firstInletPos() + QPointF{radius / 2., radius / 2.};
    for(int i = 0; i < m_inlets.size(); i++)
    {
      m_inlets[i].pos = cur_pos;
      if(auto w = m_inlets[i].widget)
        cur_pos.ry() += w->boundingRect().height() + 5.;
      else
        cur_pos.ry() += diameter;
    }

    double max_h = cur_pos.y();
    if(!m_inlets.empty())
    {
      if(m_inlets.back().widget)
      { max_h += m_inlets.back().widget->boundingRect().height();}
      else { max_h += m_inlets.back().pos.y() + radius; }
    }

    for(int i = 0; i < m_outlets.size(); i++)
      m_outlets[i].pos
          = firstOutletPos() + QPointF{radius / 2., radius / 2. + i * diameter};

    if(!m_outlets.empty())
        max_h = std::max(m_outlets.back().pos.y() + radius, max_h);

    m_rect = QRectF{0, 0, max_w, max_h};
    update();
  }
  QRectF rect() const noexcept { return m_rect; }
  QRectF boundingRect() const { return m_rect.adjusted(-10, -10, 10, 10); }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
  {
    auto& style = Process::Style::instance();
    const auto& skin = style.skin;
    const auto rect = this->rect();

    const auto& bset = skin.Emphasis5;
    const auto& fillbrush = skin.Emphasis5;
    const auto& brush = isSelected() ? skin.Base2.darker
                        : false      ? bset.lighter
                                     : bset.main;
    const auto& pen = brush.pen2_solid_round_round;

    painter->setRenderHint(QPainter::Antialiasing, true);

    // Body
    painter->setPen(pen);
    painter->setBrush(fillbrush);

    static const constexpr qreal Corner = 2.;
    painter->drawRoundedRect(rect, Corner, Corner);

    painter->setPen(skin.Light.main.pen1_5);
    painter->drawText(rect, m_label, QTextOption(Qt::AlignCenter));

    painter->save();
    painter->translate(firstInletPos());
    for(auto inl : this->m_inlets)
    {
      painter->drawEllipse(QRectF{0, 0, 10, 10});
      painter->translate(0, diameter);
    }
    painter->restore();
    painter->save();
    painter->translate(firstOutletPos());

    for(auto inl : this->m_outlets)
    {
      painter->drawEllipse(QRectF{0, 0, 10, 10});
      painter->translate(0, diameter);
    }
    painter->restore();
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event)
  {
    m_hover = true;
    update();
    event->accept();
  }

  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) { event->accept(); }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
  {
    m_hover = false;
    update();
    event->accept();
  }

  static constexpr double diameter = 20.;
  static constexpr double radius = diameter / 2.;

  QPointF firstInletPos() { return {-5, 10}; }
  QPointF firstOutletPos() { return {rect().width() - 5., 10}; }
  std::optional<int> inletAtPos(QPointF pos)
  {
    auto initialPos = firstInletPos();
    for(int i = 0; i < this->m_inlets.size(); i++)
    {
      // Center of the port:
      auto port_pos = initialPos + QPointF{radius, radius};
      if(QLineF{pos, port_pos}.length() < radius + 2)
        return i;

      initialPos += QPointF{0, diameter};
    }
    return std::nullopt;
  }
  std::optional<int> outletAtPos(QPointF pos)
  {
    auto initialPos = firstOutletPos();
    for(int i = 0; i < this->m_outlets.size(); i++)
    {
      // Center of the port:
      auto port_pos = initialPos + QPointF{radius, radius};
      if(QLineF{pos, port_pos}.length() < radius + 2)
        return i;

      initialPos += QPointF{0, diameter};
    }
    return std::nullopt;
  }

  QString m_label;
  std::vector<Port> m_inlets;
  std::vector<Port> m_outlets;
  QRectF m_rect{0, 0, 100, 100};
  bool m_hover{};
};

struct View : public QGraphicsView
{
protected:
  QPointF sceneOrig{};
  EdgeItem* drawnArc{};
  enum
  {
    FromInlet,
    FromOutlet
  } provenance{};
  void mousePressEvent(QMouseEvent* event) override
  {
    auto pos = event->pos();
    if(auto item = this->itemAt(pos))
    {
      auto scenePos = mapToScene(pos);
      auto node = static_cast<NodeItem*>(item);
      auto itempos = node->mapFromScene(scenePos);
      if(auto port = node->inletAtPos(itempos))
      {
        provenance = FromInlet;
        sceneOrig = node->mapToScene(node->m_inlets[*port].pos);
        drawnArc = new EdgeItem{nullptr};
        drawnArc->setPos(sceneOrig);
        scene()->addItem(drawnArc);
        update();
        event->accept();
        return;
      }
      else if(auto port = node->outletAtPos(itempos))
      {
        provenance = FromOutlet;
        sceneOrig = node->mapToScene(node->m_outlets[*port].pos);
        drawnArc = new EdgeItem{nullptr};
        drawnArc->setPos(sceneOrig);
        scene()->addItem(drawnArc);
        update();
        event->accept();
        return;
      }
    }
    return QGraphicsView::mousePressEvent(event);
  }
  void mouseMoveEvent(QMouseEvent* event) override
  {
    if(drawnArc)
    {
      auto pos = event->pos();
      auto scenePos = mapToScene(pos);

      drawnArc->setLine(sceneOrig, scenePos);

      update();
      event->accept();
      return;
    }
    return QGraphicsView::mouseMoveEvent(event);
  }
  void mouseReleaseEvent(QMouseEvent* event) override
  {
    if(drawnArc)
    {
      delete drawnArc;
      drawnArc = nullptr;

      auto pos = event->pos();
      if(auto item = this->itemAt(pos))
      {
        auto scenePos = mapToScene(pos);
        auto node = static_cast<NodeItem*>(item);
        auto itempos = node->mapFromScene(scenePos);
        if(provenance == FromOutlet)
        {
          if(auto port = node->inletAtPos(itempos))
          {
            qDebug() << "From outlet to " << *port;
          }
        }
        else if(provenance == FromInlet)
        {
          if(auto port = node->outletAtPos(itempos))
          {
            qDebug() << "From intlet to " << *port;
          }
        }
      }
    }
    return QGraphicsView::mouseReleaseEvent(event);
  }
};

template<typename F>
QGraphicsItem* makeidget(const F& field, QGraphicsItem* parent)
{
  if constexpr(avnd::control<F>)
  {
    constexpr auto widg = avnd::get_widget<F>();
    if constexpr(widg.widget == avnd::widget_type::slider)
    {
     auto c = new score::QGraphicsSlider{parent};
     c->setRange(0., 1.);
     return c;
    }
    else if constexpr(widg.widget == avnd::widget_type::spinbox)
    {
     auto c = new score::QGraphicsSpinbox{parent};
     c->setRange(0., 1.);
     return c;
    }
    else if constexpr(widg.widget == avnd::widget_type::lineedit)
    {
     return new Process::LineEditItem{parent};
    }
  }
  return nullptr;

}
struct GraphWidget : public QSplitter
{
  QListWidget items;
  QGraphicsScene scene;
  View view;

  std::vector<std::function<void()>> factories;

public:
  GraphWidget()
  {
    this->setGeometry(0, 0, 1000, 1000);
    this->addWidget(&items);
    this->addWidget(&view);
    this->setStretchFactor(0, 20);
    this->setStretchFactor(1, 80);

    scene.setBackgroundBrush(Qt::black);
    view.setScene(&scene);
    view.setSceneRect(QRectF{-5000, -5000, 10000, 10000});
    view.setDragMode(QGraphicsView::ScrollHandDrag);

    connect(
        &items, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
          auto idx = items.indexFromItem(item);
          factories[idx.row()]();
        });


    using T = grph::expression_graph;
    using node = T::node;

    auto f = [this]<typename N>(N&& n) {
      this->items.addItem(QString::fromUtf8(avnd::get_name<N>()));
      factories.push_back([this, n]() mutable {
        auto it = new NodeItem{nullptr};

        if constexpr(avnd::has_label<N>)
          it->setLabel(QString::fromUtf8(avnd::get_label<N>()));
        else
          it->setLabel(QString::fromUtf8(avnd::get_name<N>()));

        using ins = avnd::input_introspection<N>;
        if constexpr(ins::size > 0)
        {
          ins::for_all(avnd::get_inputs(n), [it]<typename F>(const F& field) {
            it->addInlet(QString::fromUtf8(avnd::get_name<F>()));
            if(auto w = makeidget<F>(field, it)) {
              it->m_inlets.back().widget = w;
              w->setZValue(10);
            }
          });
        }
        using outs = avnd::output_introspection<N>;
        if constexpr(outs::size > 0)
        {
          outs::for_all(avnd::get_outputs(n), [it]<typename F>(const F& field) {
            it->addOutlet(QString::fromUtf8(avnd::get_name<F>()));
          });
        }
        it->updateRect();
        scene.addItem(it);
      });
    };
    boost::mp11::mp_for_each<node>(f);
  }
};

struct MidiFileOctaver
{
  halp_meta(name, "MidiFile scaled playback")
  halp_meta(c_name, "avnd_helpers_midifile_scaled")
  halp_meta(uuid, "c43300b0-6c98-4a38-81f2-b549f0297e38")

  struct
  {
    // This port will read a midi file
    halp::midifile_port<"MIDI file", halp::simple_midi_track_event> midi;
    halp::hslider_i32<"Track", halp::range{0, 16, 0}> track;
    halp::hslider_i32<"CC", halp::range{0, 127, 0}> cc;
  } inputs;

  struct
  {
    halp::val_port<"Out", float> out;
  } outputs;

  using tick = halp::tick;
  void operator()(halp::tick t)
  {
    qDebug() << inputs.midi.midifile.length << inputs.midi.midifile.ticks_per_beat;
    if(inputs.track < inputs.midi.midifile.tracks.size())
      return;

    for(int i = 0; i < inputs.midi.midifile.tracks.size() % 10; i++)
    {
    }
  }
};
template <>
void custom_factories<grph::Graph>(
    std::vector<std::unique_ptr<score::InterfaceBase>>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  static auto w = new GraphWidget;
  w->show();
  /*
  using namespace oscr;
  auto res = oscr::instantiate_fx<grph::Graph>(ctx, key);
  fx.insert(
      fx.end(), std::make_move_iterator(res.begin()),
      std::make_move_iterator(res.end()));
      */
}
}
