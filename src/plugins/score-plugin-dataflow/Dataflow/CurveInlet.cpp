#include "CurveInlet.hpp"

#include <Process/Commands/SetControlValue.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/CurveEditor.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveStyle.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/SelectionStack.hpp>

#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QClipboard>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QResizeEvent>
#include <QScopedValueRollback>

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
  const score::DocumentContext& m_context;
  score::SelectionStack* m_selectionStack{};
  Curve::Model* m_model{};
  Curve::View* m_view{};
  Curve::Presenter* m_presenter{};
  Curve::ToolPalette* m_sm{};
  BasicToolPaletteInputDispatcher<Curve::Tool, Curve::ToolPalette, Curve::View>* m_disp{};
  QRectF m_rect{0., 0., 100., 60.};

  //! selectionStack: where point and segment selections go; the document's
  //! when null.
  CurveItem(
      Curve::Model& model, const score::DocumentContext& ctx, QGraphicsItem* parent,
      score::SelectionStack* selectionStack = nullptr)
      : QGraphicsItem{parent}
      , m_colors{score::Skin::instance()}
      , m_context{ctx}
      , m_selectionStack{selectionStack}
  {
    setModel(model);
  }

  ~CurveItem() override { clearEditor(); }

  void setModel(Curve::Model& model)
  {
    if(m_model == &model)
      return;
    clearEditor();
    m_model = &model;
    m_view = new Curve::View{this};
    m_presenter = new Curve::Presenter{m_context, m_colors.style(), model, m_view, this};
    m_presenter->setRect(m_rect);
    m_presenter->enable();
    m_presenter->enableActions(true);
    m_presenter->setBoundedMove(true);

    // The edition settings are the application's, shared with the automation
    // editors, and start disabled: the palette ignores the mouse until a
    // tool that edits is chosen.
    auto& settings = m_presenter->editionSettings();
    switch(settings.tool())
    {
      case Curve::Tool::Select:
      case Curve::Tool::Create:
      case Curve::Tool::SetSegment:
      case Curve::Tool::CreatePen:
        break;
      default:
        settings.setTool(Curve::Tool::Select);
        break;
    }

    m_view->setRect(m_rect);
    m_view->setDefaultWidth(m_rect.width());
    connect(m_view, &Curve::View::doubleClick, this, [this](QPointF pt) {
      initializeTools();
      if(m_sm)
        m_sm->createPoint(pt);
    });
    connect(
        m_presenter, &Curve::Presenter::contextMenuRequested, this,
        [this](const QPoint& screenPos, const QPointF& scenePos) {
      QMenu menu;
      m_presenter->fillContextMenu(menu, screenPos, scenePos);
      menu.exec(screenPos);
    });
    initializeTools();
    update();
  }

  void setRect(const QRectF& rect)
  {
    prepareGeometryChange();
    m_rect = rect;
    m_presenter->setRect(rect);
    m_view->setRect(rect);
    m_view->setDefaultWidth(rect.width());
    update();
  }

  QRectF boundingRect() const override { return m_rect; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    // Same box as the XY chooser control.
    auto& skin = score::Skin::instance();
    painter->fillRect(m_rect, skin.Dark.main.brush);

    const auto& pen = skin.DarkGray.main.pen0;
    const QPen previous = painter->pen();
    painter->setPen(pen);
    for(int i = 1; i < 4; i++)
    {
      const double x = m_rect.left() + m_rect.width() * i / 4.;
      const double y = m_rect.top() + m_rect.height() * i / 4.;
      painter->drawLine(QPointF{x, m_rect.top()}, QPointF{x, m_rect.bottom()});
      painter->drawLine(QPointF{m_rect.left(), y}, QPointF{m_rect.right(), y});
    }
    painter->setPen(previous);
  }

  void rollbackGesture()
  {
    if(m_sm)
      m_sm->cancelGesture();
  }

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
  {
    // The palette is bound to the scene it was made in.
    if(change == ItemSceneChange)
      clearTools();
    else if(change == ItemSceneHasChanged)
      initializeTools();
    return QGraphicsItem::itemChange(change, value);
  }

private:
  // The palette works on the scene of the view: it can only exist once the
  // item is in one.
  void initializeTools()
  {
    if(m_sm || !m_view || !m_view->scene())
      return;
    m_sm = new Curve::ToolPalette{m_context, *m_presenter, m_selectionStack};
    m_disp = new BasicToolPaletteInputDispatcher<
        Curve::Tool, Curve::ToolPalette, Curve::View>{*m_view, *m_sm};
  }

  void clearTools()
  {
    rollbackGesture();
    delete m_disp;
    m_disp = nullptr;
    delete m_sm;
    m_sm = nullptr;
  }

  void clearEditor()
  {
    clearTools();
    // The presenter owns its view and removes it from the scene.
    delete m_presenter;
    m_presenter = nullptr;
    m_view = nullptr;
  }
};

void setCurveItemModel(QGraphicsItem& item, Curve::Model& model)
{
  auto* curve = dynamic_cast<CurveItem*>(&item);
  SCORE_ASSERT(curve);
  curve->setModel(model);
}

void setCurveItemRect(QGraphicsItem& item, const QRectF& rect)
{
  auto* curve = dynamic_cast<CurveItem*>(&item);
  SCORE_ASSERT(curve);
  curve->setRect(rect);
}

//! The inspector hosts the same editor as the process, on the same model and
//! commands. Its point selection stays local: in the document's, it would
//! make the inspector show the point instead of this widget, mid-gesture.
class CurveWidget final : public QGraphicsView
{
public:
  CurveWidget(Curve::Model& model, const score::DocumentContext& ctx, QWidget* parent)
      : QGraphicsView{parent}
      , m_item{new CurveItem{model, ctx, nullptr, &m_selection}}
  {
    connect(
        &m_selection, &score::SelectionStack::currentSelectionChanged, &model,
        [&model](const Selection&, const Selection& selected) {
      model.setSelection(selected);
    });

    auto* scene = new QGraphicsScene{this};
    setScene(scene);
    scene->addItem(m_item);
    m_item->setPos(margin, margin);
    // CurveEditor::paste looks for the presenter under the focused widget.
    m_item->QObject::setParent(scene);

    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(score::Skin::instance().Dark.main.brush);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setRenderHint(QPainter::Antialiasing);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  ~CurveWidget() override
  {
    m_item->rollbackGesture();
    scene()->clear();
    m_selection.clear();
  }

  QSize sizeHint() const override { return {200, 120}; }

protected:
  void resizeEvent(QResizeEvent* event) override
  {
    QGraphicsView::resizeEvent(event);
    const QSizeF size = viewport()->size();
    setSceneRect({QPointF{}, size});
    // Room for the handles of the points at the bounds.
    m_item->setRect(
        {0., 0., std::max(1., size.width() - 2. * margin),
         std::max(1., size.height() - 2. * margin)});
  }

  bool event(QEvent* event) override
  {
    // Keep these from the application's Edit actions, which act on the
    // document selection, not on this editor's.
    if(event->type() == QEvent::ShortcutOverride
       && editKey(*static_cast<QKeyEvent*>(event)))
    {
      event->accept();
      return true;
    }
    return QGraphicsView::event(event);
  }

  void keyPressEvent(QKeyEvent* event) override
  {
    if(!editKey(*event))
      return QGraphicsView::keyPressEvent(event);
    event->accept();

    auto& presenter = *m_item->m_presenter;
    if(event->matches(QKeySequence::SelectAll))
    {
      Selection all;
      for(auto& segment : m_item->m_model->segments())
        all.append(&segment);
      for(auto* point : m_item->m_model->points())
        all.append(point);
      m_selection.pushNewSelection(all);
      return;
    }
    if(event->matches(QKeySequence::Delete) || event->key() == Qt::Key_Backspace)
    {
      presenter.removeSelection();
      return;
    }

    auto& ctx = m_item->m_context;
    auto* editor = ctx.app.interfaces<score::ObjectEditorList>().get(
        Curve::CurveEditor::static_concreteKey());
    if(!editor)
      return;
    if(event->matches(QKeySequence::Paste))
    {
      if(const auto* mime = QApplication::clipboard()->mimeData())
      {
        // Pasted where the mouse is when over the curve, else at its start.
        QPoint pos = QCursor::pos();
        if(!viewport()->rect().contains(viewport()->mapFromGlobal(pos)))
          pos = viewport()->mapToGlobal(mapFromScene(m_item->mapToScene(QPointF{})));
        editor->paste(pos, this, *mime, ctx);
      }
      return;
    }

    JSONReader reader;
    if(editor->copy(reader, m_selection.currentSelection(), ctx))
    {
      auto* mime = new QMimeData;
      mime->setData("text/plain", reader.toByteArray());
      QApplication::clipboard()->setMimeData(mime);
      if(event->matches(QKeySequence::Cut))
        presenter.removeSelection();
    }
  }

private:
  static bool editKey(const QKeyEvent& event)
  {
    return event.matches(QKeySequence::Copy) || event.matches(QKeySequence::Cut)
           || event.matches(QKeySequence::Paste) || event.matches(QKeySequence::Delete)
           || event.matches(QKeySequence::SelectAll) || event.key() == Qt::Key_Backspace;
  }

  static constexpr double margin = 4.;
  score::SelectionStack m_selection;
  CurveItem* m_item{};
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
    : ControlInlet{"Curve", id, parent}
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
  connect(
      this, &Process::ControlInlet::valueChanged, this, &CurveInlet::on_valueChange);

  // The execution only sees the value: fill it from the curve when empty
  // (a new port, or a document saved with an empty value).
  const auto* segments = value().target<std::vector<ossia::value>>();
  if((!segments || segments->empty()) && !m_curve->segments().empty())
    on_curveChange();
}

static std::optional<Curve::Point> curveInletPoint(const ossia::value& v)
{
  if(auto vec = v.target<ossia::vec2f>())
    return Curve::Point{(*vec)[0], (*vec)[1]};

  if(auto arr = v.target<std::vector<ossia::value>>())
    if(arr->size() == 2)
      return Curve::Point{
          ossia::convert<float>((*arr)[0]), ossia::convert<float>((*arr)[1])};

  return std::nullopt;
}

void CurveInlet::on_valueChange(const ossia::value& v)
{
  // Our own serialization coming back around
  if(m_syncing)
    return;

  // An empty list is no curve: the model keeps the one it has.
  const auto* segments = v.target<std::vector<ossia::value>>();
  if(!segments || segments->empty())
    return;

  auto& csl = score::GUIAppContext().interfaces<Curve::SegmentList>();

  std::vector<Curve::SegmentData> data;
  data.reserve(segments->size());

  for(const auto& seg : *segments)
  {
    // Same shapes as oscr::convert_to_curve accepts: [start, end] or
    // [start, end, map], with map a gamma or the name of a segment type.
    const auto* arr = seg.target<std::vector<ossia::value>>();
    if(!arr || arr->size() < 2)
      return;

    const auto start = curveInletPoint((*arr)[0]);
    const auto end = curveInletPoint((*arr)[1]);
    if(!start || !end)
      return;
    if(!ossia::safe_isfinite(start->x()) || !ossia::safe_isfinite(start->y())
       || !ossia::safe_isfinite(end->x()) || !ossia::safe_isfinite(end->y()))
      return;

    auto type = Curve::PowerSegment::static_concreteKey();
    QVariant specific = QVariant::fromValue(
        Curve::PowerSegmentData{Curve::PowerSegmentData::linearGamma});

    if(arr->size() >= 3)
    {
      const auto& map = (*arr)[2];
      if(auto gamma = map.target<float>())
      {
        specific = QVariant::fromValue(Curve::PowerSegmentData{(double)*gamma});
      }
      else if(auto name = map.target<std::string>())
      {
        // on_curveChange writes the factory's pretty name for anything that is
        // not a power segment; look it back up so those round-trip too.
        const auto wanted = QString::fromStdString(*name);
        for(auto& factory : csl)
        {
          if(factory.prettyName() == wanted)
          {
            type = factory.concreteKey();
            specific = {};
            break;
          }
        }
      }
    }

    data.push_back(Curve::SegmentData{
        Id<Curve::SegmentModel>{(int32_t)data.size()}, *start, *end, {}, {}, type,
        std::move(specific)});
  }

  // Drop anything degenerate: the curve editor cannot handle a segment that
  // does not advance in x.
  std::erase_if(data, [](const Curve::SegmentData& s) {
    return s.end.x() - s.start.x() < 0.0001;
  });
  if(data.empty())
    return;

  std::sort(
      data.begin(), data.end(),
      [](const Curve::SegmentData& a, const Curve::SegmentData& b) {
    return a.x() < b.x();
  });

  // fromCurveData asserts that the chain is closed at both ends
  const int N = std::ssize(data);
  for(int i = 0; i < N; i++)
  {
    data[i].previous
        = (i > 0) ? OptionalId<Curve::SegmentModel>{data[i - 1].id} : std::nullopt;
    data[i].following
        = (i < N - 1) ? OptionalId<Curve::SegmentModel>{data[i + 1].id} : std::nullopt;
  }

  // Also when the port is not in a document yet, e.g. a preset applied to a
  // process being created.
  QScopedValueRollback guard{m_syncing, true};
  m_curve->fromCurveData(data);
}

void CurveInlet::on_curveChange()
{
  // Rebuilding the model from the value must not write it straight back
  if(m_syncing)
    return;
  QScopedValueRollback guard{m_syncing, true};

  const auto& sorted = m_curve->sortedSegments();

  std::vector<ossia::value> segments;
  segments.reserve(sorted.size());
  auto& fact = score::GUIAppContext().interfaces<Curve::SegmentList>();
  for(const Curve::SegmentModel* v : sorted)
  {
    if(auto samples = qobject_cast<const Curve::PointArraySegment*>(v))
    {
      // The pen tool uses a sampled segment while dragging. Preserve its
      // shape in the same linear/power value format used after release.
      for(const auto& data : samples->toPowerSegments())
        segments.emplace_back(std::vector<ossia::value>{
            ossia::vec2f{float(data.start.x()), float(data.start.y())},
            ossia::vec2f{float(data.end.x()), float(data.end.y())}, 1.f});
      continue;
    }
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
    const Dataflow::CurveInlet& inlet, const score::DocumentContext& ctx,
    QWidget* parent, QObject* context)
{
  SCORE_ASSERT(inlet.m_curve);
  return new Dataflow::CurveWidget{*inlet.m_curve, ctx, parent};
}

QGraphicsItem* WidgetFactory::CurveInletItems::make_item(
    const Dataflow::CurveInlet& slider, const Dataflow::CurveInlet& inlet,
    const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  SCORE_ASSERT(inlet.m_curve);
  return new Dataflow::CurveItem{*inlet.m_curve, ctx, parent};
}

template <>
void DataStreamReader::read(const Dataflow::CurveInlet& p)
{
  read((const Process::ControlInlet&)p);
  readFrom(*p.m_curve);
}
template <>
void DataStreamWriter::write(Dataflow::CurveInlet& p)
{
  delete p.m_curve;
  p.m_curve = new Curve::Model{*this, &p};
}

template <>
void JSONReader::read(const Dataflow::CurveInlet& p)
{
  read((const Process::ControlInlet&)p);
  obj[strings.Domain] = p.m_domain;
  obj["Curve"] = *p.m_curve;
}

template <>
void JSONWriter::write(Dataflow::CurveInlet& p)
{
  delete p.m_curve;
  JSONObject::Deserializer curve_deser{obj["Curve"]};
  p.m_curve = new Curve::Model{curve_deser, &p};
}
