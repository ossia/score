#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/Process.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Effect/EffectLayer.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/YPos.hpp>
#include <score/graphics/widgets/QGraphicsPixmapMultichoice.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QPainter>
#include <QTextLayout>
#include <QTextLine>
namespace std
{
template <>
struct hash<std::pair<QString, const QPen*>>
{
  std::size_t operator()(const std::pair<QString, const QPen*>& p) const noexcept
  {
    return qHash(p);
  }
};
}

namespace Process
{

static auto& glyphCache() noexcept
{
  // FIXME LRU
  static ossia::hash_map<std::pair<QString, const QPen*>, QPixmap> cache;
  return cache;
}

QPixmap makeGlyphs(const QString& glyph, const QPen& pen)
{
  auto& cache = glyphCache();

  auto k = std::make_pair(glyph, &pen);
  if(auto it = cache.find(k); it != cache.end())
    return it->second;

  QImage path;

  QTextLayout lay;
  lay.setFont(score::Skin::instance().Medium8Pt);
  lay.setText(glyph);
  lay.beginLayout();
  QTextLine line = lay.createLine();
  lay.endLayout();
  line.setPosition(QPointF{0., 0.});

  auto r = line.glyphRuns();
  if(r.size() >= 1)
  {
    auto rect = line.naturalTextRect();
    path = newImage(rect.width(), rect.height());

    QPainter p{&path};

    p.setPen(pen);
    p.drawGlyphRun(QPointF{0, 0}, r[0]);
  }

  const auto& res = cache.emplace(
      std::move(k), QPixmap::fromImage(std::move(path), Qt::NoFormatConversion));
  return res.first->second;
}

static double portY()
{
  return SCORE_YPOS(1., 1.);
}

static double minPortWidth()
{
  return 20.;
}

// FIXME: need to access Scenario::networkFlags here due to the hierarchy
// FIXME: same thing in Node header
static void setupProcessNetworkToggleState(
    const Process::ProcessModel& process, score::QGraphicsPixmapMultichoice* rec_btn,
    int size)
{
  auto& pixmaps = Process::Pixmaps::instance();
  auto flags = process.networkFlags();
  if(flags & Process::NetworkFlags::Active)
  {
    if(size == 16)
    {
      rec_btn->setPixmaps(
          {pixmaps.net_sync_slot_header_pfa, pixmaps.net_sync_slot_header_psaa,
           pixmaps.net_sync_slot_header_pssa});
    }
    else
    {
      rec_btn->setPixmaps(
          {pixmaps.net_sync_node_header_pfa, pixmaps.net_sync_node_header_psaa,
           pixmaps.net_sync_node_header_pssa});
    }
  }
  else
  {
    if(size == 16)
    {
      rec_btn->setPixmaps(
          {pixmaps.net_sync_slot_header_pfi, pixmaps.net_sync_slot_header_psai,
           pixmaps.net_sync_slot_header_pssi});
    }
    else
    {
      rec_btn->setPixmaps(
          {pixmaps.net_sync_node_header_pfi, pixmaps.net_sync_node_header_psai,
           pixmaps.net_sync_node_header_pssi});
    }
  }

  if(flags & Process::NetworkFlags::Shared)
  {
    if(flags & Process::NetworkFlags::Compensated)
      rec_btn->setState(2);
    else
      rec_btn->setState(1);
  }
  else if(flags & Process::NetworkFlags::Free)
  {
    rec_btn->setState(0);
  }
}

score::QGraphicsPixmapMultichoice* setupProcessNetworkToggle(
    const Process::ProcessModel& process, int size, QGraphicsItem* parent)
{
  auto rec_btn = new score::QGraphicsPixmapMultichoice{parent};
  setupProcessNetworkToggleState(process, rec_btn, size);

  QObject::connect(
      &process, &Process::ProcessModel::networkFlagsChanged, rec_btn,
      [&process, rec_btn, size] {
    setupProcessNetworkToggleState(process, rec_btn, size);
      });
  return rec_btn;
}

DefaultHeaderDelegate::DefaultHeaderDelegate(
    const Process::ProcessModel& m, const Process::Context& doc)
    : HeaderDelegate{m, doc}
{
  m_portStartX = 0.;
  const auto flags = m.flags();
  auto& pixmaps = Process::Pixmaps::instance();
  m_ui = Process::makeExternalUIButton(m_model, m_context, this, this);
  if(m_ui)
  {
    m_ui->setPos({m_portStartX, 2});
    m_portStartX += 12;
  }

  // Net sync mode
  {
    auto rec_btn = setupProcessNetworkToggle(m, 16, this);
    rec_btn->setPos(m_portStartX, 0);
    m_portStartX += 14;
  }

  if(flags & Process::ProcessFlags::Recordable)
  {
    auto rec_btn
        = new score::QGraphicsPixmapToggle{pixmaps.record_on, pixmaps.record_off, this};
    rec_btn->setPos(m_portStartX, 0);
    m_portStartX += 12;
  }
  if(flags & Process::ProcessFlags::Snapshottable)
  {
    auto rec_btn = new score::QGraphicsPixmapButton{
        pixmaps.snapshot_on, pixmaps.snapshot_off, this};
    rec_btn->setPos(m_portStartX, 0);
    m_portStartX += 18;
  }

  con(m_model, &Process::ProcessModel::prettyNameChanged, this, [=] {
    updateText();
    update();
  });

  con(m_model, &Process::ProcessModel::inletsChanged, this, [=] { updatePorts(); });
  updatePorts();

  con(m_model, &Process::ProcessModel::benchmark, this,
      [=](double d) { updateBench(d); });
  con(
      m_model.selection, &Selectable::changed, this,
      [=](bool b) {
    m_sel = b;
    updateText();
    update();
      },
      Qt::QueuedConnection);
}

DefaultHeaderDelegate::~DefaultHeaderDelegate() { }

void DefaultHeaderDelegate::updateBench(double d)
{
  if(d >= 0.)
  {
    const auto& style = Process::Style::instance();
    m_bench = makeGlyphs(
        QString::number(d, 'g', 3),
        m_sel ? style.IntervalHeaderTextPen() : style.SlotHeaderTextPen());
  }
  else
  {
    m_bench = QPixmap{};
  }
  update();
}

void DefaultHeaderDelegate::updateText()
{
  auto& style = Process::Style::instance();
  const QPen& pen = m_sel ? style.IntervalHeaderTextPen() : textPen(style, m_model);
  if(&pen != m_lastPen || m_model.prettyName() != m_lastText)
  {
    m_line = makeGlyphs(m_model.prettyName(), pen);
    m_lastPen = &pen;
    m_lastText = m_model.prettyName();
    update();
  }
}

const QPen& DefaultHeaderDelegate::textPen(
    Style& style, const Process::ProcessModel& model) const noexcept
{
  score::ModelMetadata* parent_col
      = model.parent()->template findChild<score::ModelMetadata*>(
          {}, Qt::FindDirectChildrenOnly);
  auto& b = parent_col->getColor().getBrush();
  if(&b == &style.IntervalDefaultBackground())
    return style.skin.HalfLight.main.pen_cosmetic;
  else
    return b.lighter180.pen_cosmetic;
}

void DefaultHeaderDelegate::setSize(QSizeF sz)
{
  GraphicsShapeItem::setSize(sz);

  for(auto p : m_inPorts)
  {
    if(p->x() > sz.width())
    {
      if(p->isVisible())
        p->setPortVisible(false);
    }
    else
    {
      if(!p->isVisible())
        p->setPortVisible(true);
    }
  }
}

void DefaultHeaderDelegate::updatePorts()
{
  qDeleteAll(m_inPorts);
  m_inPorts.clear();

  m_portEndX = m_portStartX;
  const auto& inlets = m_model.inlets();

  auto& portFactory = score::AppContext().interfaces<Process::PortFactoryList>();
  for(Process::Inlet* port : inlets)
  {
    if(port->hidden)
      continue;
    if(auto fact = portFactory.get(port->concreteKey()))
    {
      if(auto item = fact->makePortItem(*port, m_context, this, this))
      {
        item->setPos(m_portEndX, portY());
        m_inPorts.push_back(item);
        m_portEndX += ((QGraphicsItem*)item)->boundingRect().width() - 2.;
      }
    }
    else
    {
      qWarning() << "Port factory for " << port << " not found !";
    }
  }
}

void DefaultHeaderDelegate::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto start = 3. + m_portStartX;
  const auto w = boundingRect().width();
  if(w > minPortWidth())
  {
    if(m_inPorts.empty())
    {
      painter->drawPixmap(QPointF{start, SCORE_YPOS(2., -1.)}, m_line);
    }
    else
    {
      double startText = start + m_portEndX;
      painter->drawPixmap(QPointF{startText, SCORE_YPOS(2., -1.)}, m_line);
    }

    if(!m_bench.isNull())
      painter->drawPixmap(QPointF{w - 32., SCORE_YPOS(2., -1.)}, m_bench);
  }
}

DefaultFooterDelegate::DefaultFooterDelegate(
    const Process::ProcessModel& model, const Process::Context& context)
    : FooterDelegate{model, context}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorScaleV);
  setFlag(ItemHasNoContents, true);
  con(model, &Process::ProcessModel::outletsChanged, this, [=] { updatePorts(); });
  updatePorts();
}

DefaultFooterDelegate::~DefaultFooterDelegate() { }

void DefaultFooterDelegate::setSize(QSizeF sz)
{
  if(sz != m_size)
  {
    prepareGeometryChange();
    m_size = sz;

    for(auto p : m_outPorts)
    {
      if(p->x() > sz.width())
      {
        if(p->isVisible())
          p->setPortVisible(false);
      }
      else
      {
        if(!p->isVisible())
          p->setPortVisible(true);
      }
    }
    update();
  }
}

void DefaultFooterDelegate::updatePorts()
{
  qDeleteAll(m_outPorts);
  m_outPorts.clear();

  auto& portFactory = score::AppContext().interfaces<Process::PortFactoryList>();

  m_portEndX = 0.;
  for(Process::Outlet* port : m_model.outlets())
  {
    if(port->hidden)
      continue;
    if(auto fact = portFactory.get(port->concreteKey()))
    {
      auto item = fact->makePortItem(*port, m_context, this, this);
      item->setPos(m_portEndX, SCORE_YPOS(0., 0.));
      m_outPorts.push_back(item);
      m_portEndX += ((QGraphicsItem*)item)->boundingRect().width() - 2.;
    }
    else
    {
      qWarning() << "Port factory for " << port << " not found !";
    }
  }
}

void DefaultFooterDelegate::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  // painter->fillRect(boundingRect(), Qt::white);
}

FooterDelegate::FooterDelegate(
    const Process::ProcessModel& model, const Process::Context& context)
    : m_model{model}
    , m_context{context}
{
  setAcceptedMouseButtons(Qt::NoButton);
}

FooterDelegate::~FooterDelegate() { }

QRectF FooterDelegate::boundingRect() const
{
  return {0., 0., m_size.width(), m_size.height()};
}

void FooterDelegate::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->ignore();
}

void FooterDelegate::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->ignore();
}

void FooterDelegate::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->ignore();
}

int FooterDelegate::type() const
{
  return UserType + 10000; // See ScenarioDocumentViewConstants
}
}
