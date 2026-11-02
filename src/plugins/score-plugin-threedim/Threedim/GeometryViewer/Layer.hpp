#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Monitoring/ModeSwitch.hpp>
#include <Process/Monitoring/MonitorLink.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QPainter>
#include <QPointer>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Effect/EffectLayer.hpp>

#include <Gfx/Widgets/OutletPreviewLayer.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <Threedim/GeometryViewer/Process.hpp>
#include <Threedim/GeometryViewer/SceneMonitor.hpp>

#include <score/model/Skin.hpp>

namespace Gfx::GeometryViewer
{
/**
 * The world axes as the camera sees them, X red, Y green, Z blue, and the
 * handle of the camera: dragging it orbits all the way round, the wheel
 * zooms, a double click goes back to the default view.
 */
class OrbitHandle final : public QGraphicsItem
{
public:
  static constexpr double size = 46.;

  OrbitHandle(const Process::ProcessModel& process, const Process::Context& ctx, QGraphicsItem* parent)
      : QGraphicsItem{parent}
      , m_context{ctx}
      , m_dispatcher{ctx.commandStack}
  {
    auto control = [&](int i) -> Process::ControlInlet* {
      return i < int(process.inlets().size())
                 ? qobject_cast<Process::ControlInlet*>(process.inlets()[i])
                 : nullptr;
    };
    m_yaw = control(3);
    m_pitch = control(4);
    m_zoom = control(5);
    m_centre = control(6);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::OpenHandCursor);
    setZValue(50.);
    setToolTip(QObject::tr("Drag to orbit, wheel to zoom, double click to reset"));
  }

  QRectF boundingRect() const override { return {0., 0., size, size}; }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = boundingRect();
    p->setPen(QPen(QColor(255, 255, 255, 40), 1));
    p->setBrush(QColor(0, 0, 0, 120));
    p->drawEllipse(r.adjusted(1, 1, -1, -1));

    // The camera looks at the centre from yaw and pitch
    constexpr double deg = std::numbers::pi / 180.;
    const double yaw = value(m_yaw) * deg, pitch = value(m_pitch) * deg;
    const QVector3D eye(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
    const QVector3D fwd = -eye;
    // As the viewer: up turns over with the camera past the poles
    const QVector3D up(
        -std::sin(pitch) * std::sin(yaw), std::cos(pitch), -std::sin(pitch) * std::cos(yaw));
    const QVector3D right = QVector3D::crossProduct(fwd, up).normalized();

    struct Axis
    {
      QVector3D dir;
      QColor color;
      const char* name;
    };
    Axis axes[]{
        {{1, 0, 0}, QColor(235, 70, 60), "X"},
        {{0, 1, 0}, QColor(90, 200, 90), "Y"},
        {{0, 0, 1}, QColor(80, 140, 240), "Z"}};
    // The farthest first, so that the nearest stays on top
    std::sort(std::begin(axes), std::end(axes), [&](const Axis& a, const Axis& b) {
      return QVector3D::dotProduct(a.dir, fwd) > QVector3D::dotProduct(b.dir, fwd);
    });
    const QPointF c = r.center();
    const double len = size * 0.34;
    QFont f = Process::ModeSwitch::font();
    f.setBold(true);
    p->setFont(f);
    for(const auto& a : axes)
    {
      const QPointF tip = c + QPointF{QVector3D::dotProduct(a.dir, right) * len, -QVector3D::dotProduct(a.dir, up) * len};
      // Darker the further it points away, so that it fades as it turns
      const double away = std::clamp(QVector3D::dotProduct(a.dir, fwd), 0.f, 1.f);
      p->setPen(QPen(mix(a.color, a.color.darker(200), away), 1.5));
      p->drawLine(c, tip);
      p->drawText(QRectF(tip.x() - 6, tip.y() - 6, 12, 12), Qt::AlignCenter, a.name);
    }
  }

  //! Repaints when the camera controls change.
  void watch(QObject* owner)
  {
    for(Process::ControlInlet* c : {m_yaw.data(), m_pitch.data()})
      if(c)
        QObject::connect(c, &Process::ControlInlet::valueChanged, owner, [this] { update(); });
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
  {
    ev->accept();
    startDrag();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override
  {
    ev->accept();
    const QPointF d = ev->scenePos() - ev->buttonDownScenePos(Qt::LeftButton);
    // Half a degree per pixel, both angles wrapping around
    const double yaw = wrap(m_startYaw - d.x() * 0.5);
    const double pitch = wrap(m_startPitch + d.y() * 0.5);
    // One command per control: start over from the values before the drag
    m_dispatcher.rollback();
    if(m_yaw)
      m_dispatcher.submit<Process::SetControlValue>(*m_yaw, float(yaw));
    if(m_pitch)
      m_dispatcher.submit<Process::SetControlValue>(*m_pitch, float(pitch));
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
  {
    ev->accept();
    m_dispatcher.commit<Process::SetControlValues>();
    setCursor(Qt::OpenHandCursor);
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev) override
  {
    ev->accept();
    // The second press drags from the view it resets to
    m_startYaw = 35.;
    m_startPitch = 25.;
    if(m_yaw)
      m_dispatcher.submit<Process::SetControlValue>(*m_yaw, 35.f);
    if(m_pitch)
      m_dispatcher.submit<Process::SetControlValue>(*m_pitch, 25.f);
    if(m_zoom)
      m_dispatcher.submit<Process::SetControlValue>(*m_zoom, 1.f);
    if(m_centre)
      m_dispatcher.submit<Process::SetControlValue>(*m_centre, ossia::vec3f{});
    m_dispatcher.commit<Process::SetControlValues>();
  }

  void wheelEvent(QGraphicsSceneWheelEvent* ev) override
  {
    ev->accept();
    if(!m_zoom)
      return;
    // Ten percent per notch
    const double z = std::clamp(value(m_zoom) * std::pow(1.1, ev->delta() / 120.), 0.01, 100.);
    CommandDispatcher<>{m_context.commandStack}.submit<Process::SetControlValue>(*m_zoom, float(z));
  }

private:
  void startDrag()
  {
    m_startYaw = value(m_yaw);
    m_startPitch = value(m_pitch);
    setCursor(Qt::ClosedHandCursor);
  }

  static QColor mix(const QColor& a, const QColor& b, double t)
  {
    return QColor::fromRgbF(
        float(a.redF() + (b.redF() - a.redF()) * t),
        float(a.greenF() + (b.greenF() - a.greenF()) * t),
        float(a.blueF() + (b.blueF() - a.blueF()) * t));
  }

  //! Into [-180, 180)
  static double wrap(double degrees)
  {
    return degrees - 360. * std::floor((degrees + 180.) / 360.);
  }

  static double value(const Process::ControlInlet* c)
  {
    return c ? ossia::convert<float>(c->value()) : 0.;
  }

  const Process::Context& m_context;
  MultiOngoingCommandDispatcher m_dispatcher;
  QPointer<Process::ControlInlet> m_yaw, m_pitch, m_zoom, m_centre;
  double m_startYaw{}, m_startPitch{};
};

//! The node shows what the viewer renders, and on hover what its scene holds.
class Layer final : public Gfx::OutletPreviewLayer
{
public:
  Layer(const Process::ProcessModel& process, const Process::Context& doc, QGraphicsItem* parent)
      : Gfx::OutletPreviewLayer{process, doc, parent}
      , m_link{process, *this, [this](Process::Monitor& m) { refresh(m); },
               [this](Process::Monitor*) {
                 m_counts.reset();
                 update();
               }}
  {
    setAcceptedMouseButtons({});
    setAcceptHoverEvents(true);
    placeholder = QStringLiteral("Geometry viewer\nno image");
    // The geometry, then the texture shown on it
    const auto& factories = doc.app.interfaces<Process::PortFactoryList>();
    const double y[2]{20., 5.};
    for(int i = 0; i < 2 && i < int(process.inlets().size()); i++)
    {
      auto* inlet = process.inlets()[i];
      if(auto fact = factories.get(inlet->concreteKey()))
        fact->makePortItem(*inlet, doc, this, this)->setPos(0, y[i]);
    }
    Process::ModeSwitch::make(process, doc, this, this);
    m_orbit = new OrbitHandle{process, doc, this};
    m_orbit->watch(this);
    placeOrbit();
  }

protected:
  void heightChanged(qreal) override { placeOrbit(); }
  void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { update(); }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { update(); }

  //! Top right, on hover: what the scene holds
  void paintOverlay(QPainter* p, const QRectF& image) const override
  {
    if(!m_counts || !isUnderMouse())
      return;
    const auto& c = *m_counts;
    auto n = [](uint64_t v, const char* one, const char* many) {
      return QStringLiteral("%1 %2").arg(v).arg(QLatin1String(v == 1 ? one : many));
    };
    QStringList lines{
        n(c.nodes, "node", "nodes") + QStringLiteral(", ")
            + n(c.meshes, "mesh", "meshes") + QStringLiteral(", ")
            + n(c.primitives, "primitive", "primitives"),
        n(c.vertices, "vertex", "vertices") + QStringLiteral(", ")
            + n(c.materials, "material", "materials"),
        n(c.lights, "light", "lights") + QStringLiteral(", ")
            + n(c.cameras, "camera", "cameras")};
    if(c.instances > 0)
      lines.back() += QStringLiteral(", ") + n(c.instances, "instance", "instances");

    static const QFont font = [] {
      QFont f = score::Skin::instance().SansFontSmall;
      f.setPixelSize(9);
      return f;
    }();
    p->setFont(font);
    const QFontMetricsF fm{font};
    double w = 0;
    for(const auto& l : lines)
      w = std::max(w, fm.horizontalAdvance(l));
    const double lh = fm.height();
    const QRectF bounds = boundingRect();
    QRectF box{0., 0., w + 6., lh * lines.size() + 4.};
    box.moveTopRight(QPointF{bounds.right() - 2., std::max(bounds.top() + 2., image.top() + 2.)});
    p->fillRect(box, QColor(0, 0, 0, 150));
    p->setPen(QColor(170, 170, 175));
    for(int i = 0; i < lines.size(); i++)
      p->drawText(QPointF{box.left() + 3., box.top() + 2. + fm.ascent() + i * lh}, lines[i]);
  }

private:
  void refresh(Process::Monitor& m)
  {
    const auto& s = static_cast<SceneMonitor&>(m).latest();
    if(!s)
      return;
    if(!m_counts || *m_counts != s->counts)
    {
      m_counts = s->counts;
      if(isUnderMouse())
        update();
    }
  }

  //! Bottom left
  void placeOrbit()
  {
    m_orbit->setPos(4., std::max(30., height() - OrbitHandle::size - 4.));
  }

  OrbitHandle* m_orbit{};
  std::optional<SceneCounts> m_counts;
  Process::MonitorLink m_link;
};

class LayerFactory final : public Process::LayerFactory
{
public:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model>::get();
  }
  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == concreteKey();
  }
  Process::LayerView* makeLayerView(
      const Process::ProcessModel& proc, const Process::Context& ctx,
      QGraphicsItem* parent) const override
  {
    return new Layer{proc, ctx, parent};
  }
  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& model, Process::LayerView* v,
      const Process::Context& ctx, QObject* parent) const override
  {
    return new Process::EffectLayerPresenter{
        model, static_cast<Process::EffectLayerView*>(v), ctx, parent};
  }
};
}
