#include "PatternEditor.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
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
    row->addSpacing(12);

    row->addWidget(new QLabel{tr("Bars"), this});
    m_field = new QComboBox{this};
    m_field->setToolTip(
        tr("The step parameter drawn under the grid, for the selected lane"));
    for(auto f :
        {StepField::Velocity, StepField::Chance, StepField::Offset, StepField::Gate,
         StepField::Ratchet, StepField::Pitch})
      m_field->addItem(fieldName(f), int(f));
    row->addWidget(m_field);
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
    connect(m_field, &QComboBox::currentIndexChanged, this, [this] {
      m_view->setField(StepField(m_field->currentData().toInt()));
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
  });
  con(model, &ProcessModel::currentPatternChanged, this, [this] {
    updateBank();
    updatePatternControls();
    updateLanePanel();
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

  m_euclid = new QCheckBox{tr("Euclidean"), panel};
  m_euclid->setToolTip(
      tr("Spread a number of hits as evenly as possible over the lane"));
  connect(m_euclid, &QCheckBox::toggled, this, [this, lane](bool b) {
    m_controller->editLane(lane(), [b](Lane& l) { l.euclid.enabled = b; });
  });
  form->addRow(m_euclid);

  m_pulses = new QSpinBox{panel};
  m_pulses->setRange(0, Pattern::maxLength);
  m_pulses->setKeyboardTracking(false);
  connect(m_pulses, &QSpinBox::valueChanged, this, [this, lane](int v) {
    m_controller->editLane(lane(), [v](Lane& l) { l.euclid.pulses = v; });
  });
  form->addRow(tr("Hits"), m_pulses);

  m_rotation = new QSpinBox{panel};
  m_rotation->setRange(-Pattern::maxLength, Pattern::maxLength);
  m_rotation->setKeyboardTracking(false);
  connect(m_rotation, &QSpinBox::valueChanged, this, [this, lane](int v) {
    m_controller->editLane(lane(), [v](Lane& l) { l.euclid.rotation = v; });
  });
  form->addRow(tr("Rotation"), m_rotation);

  m_euclidMode = new QComboBox{panel};
  m_euclidMode->addItem(tr("Replace the steps"), int(EuclidMode::Replace));
  m_euclidMode->addItem(tr("Add to the steps"), int(EuclidMode::Add));
  m_euclidMode->addItem(tr("Mask the steps"), int(EuclidMode::Mask));
  connect(m_euclidMode, &QComboBox::currentIndexChanged, this, [this, lane] {
    const auto m = EuclidMode(m_euclidMode->currentData().toInt());
    m_controller->editLane(lane(), [m](Lane& l) { l.euclid.mode = m; });
  });
  form->addRow(tr("Mode"), m_euclidMode);

  m_print = new QPushButton{tr("Write into steps"), panel};
  m_print->setToolTip(tr("Turn the rhythm into ordinary steps, to edit them by hand"));
  connect(m_print, &QPushButton::clicked, this, [this, lane] {
    m_controller->printEuclid(lane());
  });
  form->addRow(m_print);

  auto random = new QPushButton{tr("Randomize lane"), panel};
  connect(random, &QPushButton::clicked, this, [this, lane] {
    m_controller->randomize(lane());
  });
  form->addRow(random);
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

  QSignalBlocker b0{m_laneLength}, b1{m_laneDivision}, b2{m_laneVelocity}, b3{m_euclid},
      b4{m_pulses}, b5{m_rotation}, b6{m_euclidMode};
  m_laneLength->setValue(l.length);
  m_laneDivision->setValue(l.division);
  m_laneVelocity->setValue(int(std::lround(l.velocity * 127.f)));
  m_euclid->setChecked(l.euclid.enabled);
  m_pulses->setValue(l.euclid.pulses);
  m_rotation->setValue(l.euclid.rotation);
  m_euclidMode->setCurrentIndex(m_euclidMode->findData(int(l.euclid.mode)));
  for(QWidget* w :
      {(QWidget*)m_pulses, (QWidget*)m_rotation, (QWidget*)m_euclidMode,
       (QWidget*)m_print})
    w->setEnabled(l.euclid.enabled);
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
