#include "PatternEditor.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
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

//! Tool buttons that light up under the mouse and while pressed or checked.
static QString toolStyle()
{
  auto& skin = score::Skin::instance();
  return QStringLiteral(
             "QToolButton { border: 1px solid transparent; border-radius: 2px; "
             "padding: 2px 6px; }"
             "QToolButton:hover { background: %1; border-color: %2; }"
             "QToolButton:pressed, QToolButton:checked { background: %3; color: %4; }"
             "QToolButton:disabled { color: %2; }")
      .arg(
          skin.Emphasis2.main.brush.color().name(), skin.Gray.main.brush.color().name(),
          skin.Base4.main.brush.color().name(), skin.Dark.main.brush.color().name());
}

Editor::Editor(ProcessModel& model, const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_model{&model}
    , m_controller{new Controller{model, ctx, this}}
{
  setWindowTitle(model.metadata().getName());
  setAttribute(Qt::WA_DeleteOnClose);
  resize(960, 600);

  model.externalUI = this;
  model.externalUIVisible(true);

  setStyleSheet(toolStyle());

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

    // How the lanes are laid out, by the pattern controls.
    auto layout = new QButtonGroup{this};
    for(bool rings : {false, true})
    {
      auto b = toolButton(
          rings ? tr("Rings") : tr("Grid"),
          rings ? tr("The lanes as concentric rings, each its own loop")
                : tr("The lanes as rows, lined up in time"),
          this);
      b->setCheckable(true);
      b->setChecked(!rings);
      layout->addButton(b);
      connect(b, &QToolButton::clicked, this, [this, rings] { setRings(rings); });
      row->addWidget(b);
    }
    row->addSpacing(12);

    auto dup = toolButton(QStringLiteral("+"), tr("Duplicate the pattern"), this);
    connect(
        dup, &QToolButton::clicked, this, [this] { m_controller->duplicatePattern(); });
    row->addWidget(dup);
    m_removePattern = toolButton(QStringLiteral("-"), tr("Remove the pattern"), this);
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
    m_select = add(
        tr("Select"),
        tr("Drag over steps to select them, or hold Ctrl\n"
           "Ctrl+C, Ctrl+X: copy, cut\n"
           "Ctrl+V: paste on the step under the mouse\n"
           "Delete: clear\n"
           "Escape: deselect"),
        [this](bool on) { m_view->setSelectMode(on); });
    m_select->setCheckable(true);
    row->addSpacing(12);
    add(tr("Copy"), tr("Copy the selected steps, or the pattern"), [this] { copy(); });
    m_paste = add(
        tr("Paste"),
        tr("Paste copied steps on the step under the mouse, or a copied pattern "
           "over this one"),
        [this] { paste(); });
    const auto updatePaste = [this] {
      auto md = QApplication::clipboard()->mimeData();
      m_paste->setEnabled(md && (patternFromMime(*md) || stepsFromMime(*md)));
    };
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, updatePaste);
    updatePaste();
    add(tr("Clear"), tr("Clear the selected steps, or every note"), [this] { clear(); });
    add(QStringLiteral("◀"), tr("Rotate the pattern one step left"),
        [this] { m_controller->rotate(-1); });
    add(QStringLiteral("▶"), tr("Rotate the pattern one step right"),
        [this] { m_controller->rotate(1); });
    add(tr("Random"), tr("Fill the pattern at random"),
        [this] { m_controller->randomize(); });
    row->addSpacing(12);

    row->addWidget(new QLabel{tr("Transpose"), this});
    const std::pair<int, QString> moves[]
        = {{-12, tr("Every lane's note an octave down")},
           {-1, tr("Every lane's note a semitone down")},
           {1, tr("Every lane's note a semitone up")},
           {12, tr("Every lane's note an octave up")}};
    for(const auto& [semitones, tip] : moves)
      add(semitones > 0 ? QStringLiteral("+%1").arg(semitones) : QString::number(semitones),
          tip, [this, semitones = semitones] { m_controller->transpose(semitones); });
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
    // Many lanes in a small window scroll, rather than get too low to use.
    m_graphics->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_graphics->viewport()->installEventFilter(this);
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
    connect(m_view, &View::fieldRandomized, this, [this](int lane, int f) {
      m_controller->randomizeField(StepField(f), lane);
    });
    connect(m_view, &View::fieldReset, this, [this](int lane, int f) {
      m_controller->resetField(StepField(f), lane);
    });
    connect(m_view, &View::laneSelected, this, [this] { updateLanePanel(); });
    connect(
        m_view, &View::stepMenuRequested, this,
        [this](int lane, int step, QPoint screen) { showStepMenu(lane, step, screen); });

    auto body = new QHBoxLayout;
    body->setSpacing(4);
    body->addWidget(m_graphics, 1);
    body->addWidget(makeLanePanel());
    lay->addLayout(body, 1);
  }

  con(model, &ProcessModel::patternsChanged, this, [this] {
    updateBank();
    updatePatternControls();
    updateLanePanel();
    fitGrid();
  });
  con(model, &ProcessModel::currentPatternChanged, this, [this] {
    updateBank();
    updatePatternControls();
    updateLanePanel();
    fitGrid();
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

  // Another skin: the styles built from its colours are built again.
  connect(&score::Skin::instance(), &score::Skin::changed, this, [this] {
    setStyleSheet(toolStyle());
    m_graphics->setBackgroundBrush(score::Skin::instance().Background1.main.brush);
    updateBank();
  });

  addShortcuts();
  updateBank();
  updatePatternControls();
  updateLanePanel();
  QTimer::singleShot(0, this, [this] { fitGrid(); });
}

QWidget* Editor::makeLanePanel()
{
  auto panel = new QGroupBox{tr("Lane"), this};
  panel->setFixedWidth(190);
  auto form = new QFormLayout{panel};
  form->setContentsMargins(4, 4, 4, 4);

  const auto lane = [this] { return m_view->selectedLane(); };

  m_laneLength = new QSpinBox{panel};
  m_laneLength->setRange(0, Pattern::maxLength);
  m_laneLength->setSpecialValueText(tr("Pattern"));
  m_laneLength->setToolTip(tr("Steps before the lane loops"));
  m_laneLength->setKeyboardTracking(false);
  connect(m_laneLength, &QSpinBox::valueChanged, this, [this, lane](int v) {
    m_controller->editLane(lane(), [v](Lane& l) { l.length = v; });
  });
  form->addRow(tr("Steps"), m_laneLength);

  m_laneDivision = new QSpinBox{panel};
  m_laneDivision->setRange(0, 64);
  m_laneDivision->setSpecialValueText(tr("Pattern"));
  m_laneDivision->setPrefix(QStringLiteral("1/"));
  m_laneDivision->setToolTip(tr("Steps per whole note"));
  m_laneDivision->setKeyboardTracking(false);
  connect(m_laneDivision, &QSpinBox::valueChanged, this, [this, lane](int v) {
    m_controller->editLane(lane(), [v](Lane& l) { l.division = v; });
  });
  form->addRow(tr("Rate"), m_laneDivision);

  m_laneVelocity = new QSpinBox{panel};
  m_laneVelocity->setRange(0, 127);
  m_laneVelocity->setKeyboardTracking(false);
  connect(m_laneVelocity, &QSpinBox::valueChanged, this, [this, lane](int v) {
    m_controller->editLane(lane(), [v](Lane& l) { l.velocity = v / 127.f; });
  });
  form->addRow(tr("Velocity"), m_laneVelocity);

  // Which spreads each hit of a ratchet draws for itself.
  auto perHitLabel = new QLabel{tr("Random on each hit"), panel};
  perHitLabel->setToolTip(
      tr("Checked: each hit of a ratchet gets its own random value\n"
         "Unchecked: the whole note shares one"));
  form->addRow(perHitLabel);
  auto perHit = new QWidget{panel};
  auto perHitRow = new QVBoxLayout{perHit};
  perHitRow->setContentsMargins(0, 0, 0, 0);
  const std::pair<uint8_t, QString> spreads[]
      = {{Lane::VelocityPerHit, tr("Velocity")},
         {Lane::PitchPerHit, tr("Pitch")},
         {Lane::TimingPerHit, tr("Timing")}};
  for(int i = 0; i < 3; i++)
  {
    const auto [bit, name] = spreads[i];
    auto box = new QCheckBox{name, perHit};
    connect(box, &QCheckBox::toggled, this, [this, lane, bit = bit](bool on) {
      m_controller->editLane(lane(), [bit, on](Lane& l) {
        l.randomPerHit = on ? (l.randomPerHit | bit) : (l.randomPerHit & ~bit);
      });
    });
    perHitRow->addWidget(box);
    m_perHit[i] = box;
  }
  form->addRow(perHit);

  // Generators write into the lane's steps, as one undo step.
  auto gen = new QLabel{tr("Generate"), panel};
  gen->setEnabled(false);
  form->addRow(gen);

  auto random = new QPushButton{tr("Random"), panel};
  random->setToolTip(tr("Notes at random, more of them on the beats"));
  connect(random, &QPushButton::clicked, this, [this, lane] {
    m_controller->randomize(lane());
  });
  form->addRow(random);

  m_pulses = new QSpinBox{panel};
  m_pulses->setRange(0, Pattern::maxLength);
  m_pulses->setValue(4);
  m_pulses->setToolTip(tr("Notes to spread over the lane"));
  form->addRow(tr("Hits"), m_pulses);

  m_rotation = new QSpinBox{panel};
  m_rotation->setRange(-Pattern::maxLength, Pattern::maxLength);
  m_rotation->setToolTip(tr("Steps the rhythm is turned later"));
  form->addRow(tr("Rotation"), m_rotation);

  m_combine = new QComboBox{panel};
  m_combine->addItem(tr("Replace the steps"), int(Combine::Replace));
  m_combine->addItem(tr("Add to the steps"), int(Combine::Add));
  m_combine->addItem(tr("Keep steps on hits"), int(Combine::Mask));
  form->addRow(m_combine);

  auto euclid = new QPushButton{tr("Euclidean"), panel};
  euclid->setToolTip(tr("Spread the hits as evenly as possible over the lane"));
  const auto generate = [this, lane] {
    m_controller->euclidean(
        lane(), m_pulses->value(), m_rotation->value(),
        Combine(m_combine->currentData().toInt()));
  };
  connect(euclid, &QPushButton::clicked, this, generate);
  form->addRow(euclid);
  return panel;
}

void Editor::updateLanePanel()
{
  if(!m_model || !m_laneLength
     || !ossia::valid_index(m_model->currentPattern(), m_model->patterns()))
    return;
  const auto& p = m_model->patterns()[m_model->currentPattern()];
  const int lane = m_view->selectedLane();
  if(!ossia::valid_index(lane, p.lanes))
    return;
  const Lane& l = p.lanes[lane];

  QSignalBlocker b0{m_laneLength}, b1{m_laneDivision}, b2{m_laneVelocity},
      b3{m_perHit[0]}, b4{m_perHit[1]}, b5{m_perHit[2]};
  m_perHit[0]->setChecked(l.randomPerHit & Lane::VelocityPerHit);
  m_perHit[1]->setChecked(l.randomPerHit & Lane::PitchPerHit);
  m_perHit[2]->setChecked(l.randomPerHit & Lane::TimingPerHit);
  m_laneLength->setValue(l.length);
  m_laneDivision->setValue(l.division);
  m_laneVelocity->setValue(int(std::lround(l.velocity * 127.f)));
}

void Editor::showStepMenu(int lane, int step, QPoint screen)
{
  if(!m_model || !ossia::valid_index(m_model->currentPattern(), m_model->patterns()))
    return;
  const auto& p = m_model->patterns()[m_model->currentPattern()];
  if(!ossia::valid_index(lane, p.lanes))
    return;
  const StepParams& sp = stepParams(p.lanes[lane], step);

  QMenu menu;
  auto title = menu.addAction(tr("Step %1 plays").arg(step + 1));
  title->setEnabled(false);
  const auto add = [&](QMenu& m, const QString& text, Condition c, int a, int b) {
    auto act = m.addAction(text);
    act->setCheckable(true);
    act->setChecked(
        sp.condition == c
        && (c != Condition::Cycle || (sp.cycleA == a && sp.cycleB == b)));
    connect(act, &QAction::triggered, this, [this, lane, step, c, a, b] {
      m_controller->setCondition(lane, step, c, a, b);
    });
  };
  add(menu, tr("Always"), Condition::Always, 1, 2);
  add(menu, tr("Only during a fill"), Condition::Fill, 1, 2);
  add(menu, tr("Except during a fill"), Condition::NotFill, 1, 2);
  add(menu, tr("Only the first time"), Condition::First, 1, 2);
  add(menu, tr("Except the first time"), Condition::NotFirst, 1, 2);
  auto cycles = menu.addMenu(tr("Every few passes"));
  for(int b = 2; b <= 8; b++)
  {
    auto sub = cycles->addMenu(tr("Out of %1").arg(b));
    for(int a = 1; a <= b; a++)
      add(*sub, tr("Pass %1 of %2").arg(a).arg(b), Condition::Cycle, a, b);
  }
  menu.addSeparator();
  connect(
      menu.addAction(tr("Reset the step's parameters")), &QAction::triggered, this,
      [this, lane, step] {
    m_controller->editLane(lane, [step](Lane& l) {
      if(ossia::valid_index(step, l.params))
        l.params[step] = StepParams{};
    });
  });
  menu.exec(screen);
}

void Editor::setRings(bool b)
{
  // No step selection on rings.
  m_select->setEnabled(!b);
  if(b)
  {
    m_select->setChecked(false);
    m_view->setSelectMode(false);
    m_view->clearSelection();
  }
  m_view->setRings(b);
}

void Editor::copy()
{
  if(m_view->selection().valid())
    m_controller->copySteps(m_view->selection());
  else
    m_controller->copy();
}

void Editor::paste()
{
  auto md = QApplication::clipboard()->mimeData();
  if(auto steps = md ? stepsFromMime(*md) : std::nullopt)
  {
    const auto t = m_view->pasteTarget(steps->lanes.size());
    m_controller->pasteSteps(t.lane, t.starts);
  }
  else
  {
    m_controller->paste();
  }
}

void Editor::clear()
{
  if(m_view->selection().valid())
    m_controller->clearSteps(m_view->selection());
  else
    m_controller->clear();
}

void Editor::addShortcuts()
{
  const auto add = [this](QKeySequence k, auto f) {
    auto s = new QShortcut{k, this};
    s->setContext(Qt::WidgetWithChildrenShortcut);
    connect(s, &QShortcut::activated, this, f);
  };
  add(QKeySequence::Copy, [this] { copy(); });
  add(QKeySequence::Paste, [this] { paste(); });
  add(QKeySequence::Cut, [this] {
    if(m_view->selection().valid())
    {
      copy();
      clear();
    }
  });
  add(QKeySequence::Delete, [this] {
    if(m_view->selection().valid())
      clear();
  });
  add(QKeySequence::SelectAll, [this] { m_view->selectAll(); });
  add(Qt::Key_Escape, [this] { m_view->clearSelection(); });
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
  m_view->setWidth(sz.width());
  m_view->setHeight(sz.height());
  const double h = std::max<double>(sz.height(), m_view->minimumHeight());
  m_view->setHeight(h);
  m_scene->setSceneRect(0, 0, sz.width(), h);
}

//! The viewport narrows when the scroll bar shows up: the grid follows.
bool Editor::eventFilter(QObject* watched, QEvent* event)
{
  if(watched == m_graphics->viewport() && event->type() == QEvent::Resize)
    fitGrid();
  return QWidget::eventFilter(watched, event);
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
      "border-radius: 2px; padding: 1px 0px; }"
      "QToolButton:hover { border-color: %3; }");
  const QString hover = skin.Light.main.brush.color().name();
  const QString shown = base.arg(
      skin.Base4.main.brush.color().name(), skin.Dark.main.brush.color().name(), hover);
  const QString other = base.arg(
      skin.Background2.main.brush.color().name(), skin.Light.main.brush.color().name(),
      hover);
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
