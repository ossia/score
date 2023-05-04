#include "CurveInlet.hpp"

#include <Process/Commands/SetControlValue.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveStyle.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QLabel>
#include <QTimer>

#include <wobjectimpl.h>

#include <verdigris>
namespace Dataflow
{

template <typename Tool_T, typename ToolPalette_T, typename Input_T>
class BasicToolPaletteInputDispatcher : public QObject
{
public:
  BasicToolPaletteInputDispatcher(const Input_T& input, ToolPalette_T& palette)
      : m_palette{palette}
      , m_currentTool{palette.editionSettings().tool()}
  {
    auto screens = QGuiApplication::screens();
    if(!screens.empty())
    {
      m_frameTime = 1000000. / screens.front()->refreshRate();
    }
    using EditionSettings_T
        = std::remove_reference_t<decltype(palette.editionSettings())>;
    con(palette.editionSettings(), &EditionSettings_T::toolChanged, this,
        &BasicToolPaletteInputDispatcher::on_toolChanged);
    con(input, &Input_T::pressed, this, &BasicToolPaletteInputDispatcher::on_pressed);
    con(input, &Input_T::moved, this, &BasicToolPaletteInputDispatcher::on_moved);
    con(input, &Input_T::released, this, &BasicToolPaletteInputDispatcher::on_released);
    con(input, &Input_T::escPressed, this, &BasicToolPaletteInputDispatcher::on_cancel);
  }

  void on_toolChanged(Tool_T t)
  {
    m_palette.desactivate(m_currentTool);
    m_palette.activate(t);
    m_currentTool = t;
    if(m_running)
    {
      m_palette.on_cancel();
      m_prev = std::chrono::steady_clock::now();
      m_palette.on_pressed(m_currentPoint);
    }
  }

  void on_pressed(QPointF p)
  {
    m_currentPoint = p;
    m_prev = std::chrono::steady_clock::now();
    m_palette.on_pressed(p);
    m_running = true;
  }

  void on_moved(QPointF p)
  {
    using namespace std::literals::chrono_literals;
    const auto t = std::chrono::steady_clock::now();
    if(t - m_prev < std::chrono::microseconds((int64_t)m_frameTime))
    {
      m_elapsedPoint = p;
    }
    m_currentPoint = p;
    m_palette.on_moved(p);
    m_prev = t;
  }

  void on_released(QPointF p)
  {
    m_running = false;

    m_currentPoint = p;
    m_palette.on_released(p);
  }

  void on_cancel()
  {
    m_running = false;

    m_palette.on_cancel();
  }

private:
  ToolPalette_T& m_palette;
  QPointF m_currentPoint;
  Tool_T m_currentTool;

  std::chrono::steady_clock::time_point m_prev;
  QPointF m_elapsedPoint;

  qreal m_frameTime{16666}; // In microseconds
  bool m_running = false;
};
struct CurveItem
    : public QObject
    , public QGraphicsItem
{
  class Colors
  {
  public:
    Colors(const score::Skin& s)
        : m_style{s.Emphasis3, s.Tender2, s.Emphasis3, s.Tender2, s.Gray}
    {
      m_style.init(s);
    }

    const auto& style() const { return m_style; }

  private:
    Curve::Style m_style;
  } m_colors;
  Curve::Model& m_model;
  Curve::View* m_view{};
  Curve::Presenter* m_presenter{};
  CommandDispatcher<> m_commandDispatcher;

  Curve::ToolPalette* m_sm{};
  BasicToolPaletteInputDispatcher<Curve::Tool, Curve::ToolPalette, Curve::View>*
      m_disp{};

  CurveItem(
      Curve::Model& model, const score::DocumentContext& ctx, QGraphicsItem* parent)
      : QGraphicsItem{parent}
      , m_colors{score::Skin::instance()}
      , m_model{model}
      , m_commandDispatcher{ctx.commandStack}
  {
    m_view = new Curve::View{this};

    m_presenter = new Curve::Presenter{ctx, m_colors.style(), m_model, m_view, this};
    m_presenter->setRect(QRectF{0, 0, 100, 60});
    m_presenter->enable();
    m_presenter->enableActions(true);
    m_presenter->editionSettings().setTool(Curve::Tool::Select);
    m_presenter->setBoundedMove(true);

    m_view->setRect(QRectF{0, 0, 100, 60});
    m_view->setDefaultWidth(100);

    // Needed because scene() is null at this point in time
    QTimer::singleShot(100, this, [this, &ctx] {
      m_sm = new Curve::ToolPalette{ctx, *m_presenter};
      m_disp = new BasicToolPaletteInputDispatcher<
          Curve::Tool, Curve::ToolPalette, Curve::View>{*m_view, *m_sm};
    });

    connect(m_view, &Curve::View::doubleClick, this, [this](QPointF pt) {
      m_sm->createPoint(pt);
    });
  }

  ~CurveItem()
  {
    delete m_disp;
    delete m_sm;
    delete m_presenter;
  }

  QRectF boundingRect() const override { return m_view->boundingRect(); }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
  }
};

CurveInlet::CurveInlet(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
    , m_curve{new Curve::Model{Id<Curve::Model>{0}, this}}
{
  vis.writeTo(*this);
  this->init();
}

CurveInlet::CurveInlet(JSONObject::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
    , m_curve{new Curve::Model{Id<Curve::Model>{0}, this}}
{
  vis.writeTo(*this);
  this->init();
}

CurveInlet::CurveInlet(DataStream::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
    , m_curve{new Curve::Model{Id<Curve::Model>{0}, this}}
{
  vis.writeTo(*this);
  this->init();
}

CurveInlet::CurveInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
    , m_curve{new Curve::Model{Id<Curve::Model>{0}, this}}
{
  vis.writeTo(*this);
  this->init();
}

CurveInlet::~CurveInlet() { }
CurveInlet::CurveInlet(Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
    , m_curve{new Curve::Model{Id<Curve::Model>{0}, this}}
{
  auto s1 = new Curve::DefaultCurveSegmentModel(Id<Curve::SegmentModel>(1), m_curve);

  s1->setStart({0., 0.});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  this->init();
}

void CurveInlet::init()
{
  connect(m_curve, &Curve::Model::changed, this, &CurveInlet::on_curveChange);
}

void CurveInlet::on_curveChange()
{
  const auto& sorted = m_curve->sortedSegments();

  std::vector<ossia::value> segments;
  segments.reserve(sorted.size());
  auto& fact = score::GUIAppContext().interfaces<Curve::SegmentList>();
  for(const Curve::SegmentModel* v : sorted)
  {
    std::vector<ossia::value> segment;
    segment.reserve(3);
    segment.push_back(ossia::vec2f{(float)v->start().x(), (float)v->start().y()});
    segment.push_back(ossia::vec2f{(float)v->end().x(), (float)v->end().y()});
    if(auto power = qobject_cast<const Curve::PowerSegment*>(v))
    {
      segment.push_back(power->gamma);
    }
    else
    {
      auto sf = fact.get(v->concreteKey());

      segment.push_back(sf->prettyName().toStdString());
    }
    segments.push_back(std::move(segment));
  }

  setValue(std::move(segments));
}
}

QWidget* WidgetFactory::CurveInletItems::make_widget(
    const Dataflow::CurveInlet& slider, const Dataflow::CurveInlet& inlet,
    const score::DocumentContext& ctx, QWidget* parent, QObject* context)
{
  return new QLabel{"Unimplemented"};
}

QGraphicsItem* WidgetFactory::CurveInletItems::make_item(
    const Dataflow::CurveInlet& slider, const Dataflow::CurveInlet& inlet,
    const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  SCORE_ASSERT(inlet.m_curve);
  return new Dataflow::CurveItem{*inlet.m_curve, ctx, parent};
}

template <>
void DataStreamReader::read<Dataflow::CurveInlet>(const Dataflow::CurveInlet& p)
{
  read((const Process::ControlInlet&)p);
  readFrom(*p.m_curve);
}
template <>
void DataStreamWriter::write<Dataflow::CurveInlet>(Dataflow::CurveInlet& p)
{
  delete p.m_curve;
  p.m_curve = new Curve::Model{*this, &p};
}

template <>
void JSONReader::read<Dataflow::CurveInlet>(const Dataflow::CurveInlet& p)
{
  read((const Process::ControlInlet&)p);
  obj[strings.Domain] = p.m_domain;
  obj["Curve"] = *p.m_curve;
}

template <>
void JSONWriter::write<Dataflow::CurveInlet>(Dataflow::CurveInlet& p)
{
  delete p.m_curve;
  JSONObject::Deserializer curve_deser{obj["Curve"]};
  p.m_curve = new Curve::Model{curve_deser, &p};
}
