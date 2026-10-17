#include "PatternEditor.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QClipboard>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <Patternist/PatternController.hpp>
#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternTimeGrid.hpp>
#include <Patternist/PatternView.hpp>

namespace Patternist
{
static QToolButton* toolButton(const QString& text, const QString& tip, QWidget* parent)
{
  auto b = new QToolButton{parent};
  b->setText(text);
  b->setToolTip(tip);
  b->setAutoRaise(true);
  return b;
}

Editor::Editor(ProcessModel& model, const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_model{&model}
    , m_controller{new Controller{model, ctx, this}}
{
  setWindowTitle(model.metadata().getName());
  setAttribute(Qt::WA_DeleteOnClose);
  resize(720, 320);

  model.externalUI = this;
  model.externalUIVisible(true);

  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(2);

  // Pattern bank
  {
    auto row = new QHBoxLayout;
    row->setSpacing(2);
    row->addWidget(new QLabel{tr("Patterns"), this});

    auto area = new QScrollArea{this};
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto bank = new QWidget{area};
    m_bank = new QHBoxLayout{bank};
    m_bank->setContentsMargins(0, 0, 0, 0);
    m_bank->setSpacing(1);
    m_bank->addStretch(1);
    area->setWidget(bank);
    area->setFixedHeight(28);
    row->addWidget(area, 1);

    auto dup = toolButton(QStringLiteral("+"), tr("Duplicate the pattern"), this);
    connect(
        dup, &QToolButton::clicked, this, [this] { m_controller->duplicatePattern(); });
    row->addWidget(dup);
    m_removePattern = toolButton(QStringLiteral("−"), tr("Remove the pattern"), this);
    connect(m_removePattern, &QToolButton::clicked, this, [this] {
      m_controller->removePattern();
    });
    row->addWidget(m_removePattern);
    lay->addLayout(row);
  }

  // Pattern tools
  {
    auto row = new QHBoxLayout;
    row->setSpacing(2);
    const auto add = [&](const QString& text, const QString& tip, auto f) {
      auto b = toolButton(text, tip, this);
      connect(b, &QToolButton::clicked, this, f);
      row->addWidget(b);
      return b;
    };
    add(tr("Copy"), tr("Copy the pattern"), [this] { m_controller->copy(); });
    m_paste = add(tr("Paste"), tr("Replace the pattern with the copied one"), [this] {
      m_controller->paste();
    });
    const auto updatePaste = [this] {
      auto md = QApplication::clipboard()->mimeData();
      m_paste->setEnabled(md && patternFromMime(*md));
    };
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, updatePaste);
    updatePaste();
    add(tr("Clear"), tr("Remove every note"), [this] { m_controller->clear(); });
    add(QStringLiteral("◀"), tr("Rotate the pattern one step left"),
        [this] { m_controller->rotate(-1); });
    add(QStringLiteral("▶"), tr("Rotate the pattern one step right"),
        [this] { m_controller->rotate(1); });
    add(tr("Random"), tr("Fill the pattern at random"),
        [this] { m_controller->randomize(); });
    row->addSpacing(12);

    row->addWidget(new QLabel{tr("Steps"), this});
    m_steps = new QSpinBox{this};
    m_steps->setRange(1, Pattern::maxLength);
    m_steps->setKeyboardTracking(false);
    connect(m_steps, &QSpinBox::valueChanged, this, [this](int v) {
      m_controller->setLength(v);
    });
    row->addWidget(m_steps);

    row->addWidget(new QLabel{tr("Rate"), this});
    m_rate = new QSpinBox{this};
    m_rate->setRange(1, 64);
    m_rate->setPrefix(QStringLiteral("1/"));
    m_rate->setToolTip(tr("Steps per whole note"));
    m_rate->setKeyboardTracking(false);
    connect(m_rate, &QSpinBox::valueChanged, this, [this](int v) {
      m_controller->setDivision(v);
    });
    row->addWidget(m_rate);
    row->addSpacing(12);

    add(tr("Add lane"), tr("Add a lane, a copy of the last one"),
        [this] { m_controller->addLane(); });
    row->addStretch(1);

    m_status = new QLabel{this};
    m_status->setEnabled(false);
    row->addWidget(m_status);
    auto clearStatus = new QTimer{this};
    clearStatus->setSingleShot(true);
    connect(clearStatus, &QTimer::timeout, m_status, &QLabel::clear);
    m_controller->status = [this, clearStatus](const QString& text) {
      m_status->setText(text);
      clearStatus->start(2500);
    };
    lay->addLayout(row);
  }

  // Grid
  {
    auto& skin = score::Skin::instance();
    m_scene = new QGraphicsScene{this};
    m_graphics = new QGraphicsView{m_scene, this};
    m_graphics->setFrameShape(QFrame::NoFrame);
    m_graphics->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphics->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphics->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_graphics->setBackgroundBrush(skin.Background1.main.brush);
    m_graphics->setRenderHint(QPainter::Antialiasing, false);
    m_graphics->setMouseTracking(true);

    m_view = new View{model, nullptr};
    m_view->setMode(View::Mode::Fit);
    m_view->setAdvanced(true);
    auto grid = new TimeGridWatcher{model, this};
    connect(grid, &TimeGridWatcher::changed, m_view, &View::setTimeGrid);
    m_view->setTimeGrid(grid->grid());
    m_scene->addItem(m_view);
    m_controller->attach(*m_view);
    connect(m_view, &View::askContextMenu, this, [this](QPoint screen, QPointF scene) {
      QMenu menu;
      m_controller->fillMenu(menu, m_view->headerLaneAt(m_view->mapFromScene(scene)));
      menu.exec(screen);
    });
    lay->addWidget(m_graphics, 1);
  }

  con(model, &ProcessModel::patternsChanged, this, [this] {
    updateBank();
    updatePatternControls();
  });
  con(model, &ProcessModel::currentPatternChanged, this, [this] {
    updateBank();
    updatePatternControls();
  });
  con(model, &ProcessModel::execPattern, this, [this](int cur, int next) {
    if(cur != m_playing || next != m_pending)
    {
      m_playing = cur;
      m_pending = next;
      updateBank();
    }
  });
  con(model.metadata(), &score::ModelMetadata::NameChanged, this,
      [this](const QString& name) { setWindowTitle(name); });

  updateBank();
  updatePatternControls();
  QTimer::singleShot(0, this, [this] { fitGrid(); });
}

Editor::~Editor()
{
  if(m_model && m_model->externalUI == this)
  {
    m_model->externalUI = nullptr;
    m_model->externalUIVisible(false);
  }
}

void Editor::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  fitGrid();
}

void Editor::fitGrid()
{
  const QSize sz = m_graphics->viewport()->size();
  m_scene->setSceneRect(0, 0, sz.width(), sz.height());
  m_view->setWidth(sz.width());
  m_view->setHeight(sz.height());
}

void Editor::updateBank()
{
  if(!m_model)
    return;
  const int count = m_model->patterns().size();
  while(std::ssize(m_bankButtons) < count)
  {
    const int idx = m_bankButtons.size();
    auto b = new QToolButton{m_bank->parentWidget()};
    b->setCheckable(true);
    b->setText(QString::number(idx));
    // Room for three digits in bold, so that nothing moves when the playing
    // or shown pattern changes.
    QFont bold = b->font();
    bold.setBold(true);
    b->setFixedWidth(QFontMetrics{bold}.horizontalAdvance(QStringLiteral("000")) + 14);
    connect(b, &QToolButton::clicked, this, [this, idx] {
      m_controller->selectPattern(idx);
      updateBank();
    });
    m_bank->insertWidget(idx, b);
    m_bankButtons.push_back(b);
  }
  while(std::ssize(m_bankButtons) > count)
  {
    delete m_bankButtons.back();
    m_bankButtons.pop_back();
  }

  // The one shown is highlighted, the one playing is bold, the next one
  // underlined.
  auto& skin = score::Skin::instance();
  const QString base = QStringLiteral(
      "QToolButton { background: %1; color: %2; border: 1px solid %1; "
      "border-radius: 2px; padding: 1px 0px; }");
  const QString shown = base.arg(
      skin.Base4.main.brush.color().name(), skin.Dark.main.brush.color().name());
  const QString other = base.arg(
      skin.Background2.main.brush.color().name(), skin.Light.main.brush.color().name());
  for(int i = 0; i < count; i++)
  {
    auto b = m_bankButtons[i];
    const bool current = i == m_model->currentPattern();
    b->setChecked(current);
    b->setStyleSheet(current ? shown : other);
    QFont f = b->font();
    f.setBold(i == m_playing);
    f.setUnderline(i == m_pending);
    b->setFont(f);
  }
  m_removePattern->setEnabled(count > 1);
}

void Editor::updatePatternControls()
{
  if(!m_model || !ossia::valid_index(m_model->currentPattern(), m_model->patterns()))
    return;
  const auto& p = m_model->patterns()[m_model->currentPattern()];
  QSignalBlocker b1{m_steps}, b2{m_rate};
  m_steps->setValue(p.length);
  m_rate->setValue(p.division);
}
}
