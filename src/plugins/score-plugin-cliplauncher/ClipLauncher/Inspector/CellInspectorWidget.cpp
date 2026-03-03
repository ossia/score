#include "CellInspectorWidget.hpp"

#include <Inspector/InspectorLayout.hpp>
#include <Inspector/InspectorSectionWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/Commands/AddTransitionRule.hpp>
#include <ClipLauncher/Commands/RemoveTransitionRule.hpp>
#include <ClipLauncher/Commands/SetCellProperties.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

const ProcessModel& CellInspectorWidget::parentProcess() const
{
  return *safe_cast<const ProcessModel*>(m_cell.parent());
}

CellInspectorWidget::CellInspectorWidget(
    const CellModel& cell, const score::DocumentContext& ctx, QWidget* parent)
    : InspectorWidgetBase{cell, ctx, parent,
          QString("Cell [%1, %2]").arg(cell.lane() + 1).arg(cell.scene() + 1)}
    , m_cell{cell}
    , m_ctx{ctx}
{
  std::vector<QWidget*> parts;

  // --- Cell Properties Section ---
  {
    auto section = new Inspector::InspectorSectionWidget{"Cell Properties", false, this};
    auto w = new QWidget;
    auto lay = new Inspector::Layout{w};

    // Launch Mode
    m_launchModeCombo = new QComboBox;
    m_launchModeCombo->addItems(
        {tr("Immediate"), tr("Quantized Beat"), tr("Quantized Bar"),
         tr("Quantized End Clip"), tr("Queued"), tr("Fader Start")});
    m_launchModeCombo->setCurrentIndex(static_cast<int>(cell.launchMode()));
    connect(
        m_launchModeCombo, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this](int idx) {
          auto newMode = static_cast<LaunchMode>(idx);
          if(newMode != m_cell.launchMode())
          {
            CommandDispatcher<> disp{m_ctx.commandStack};
            disp.submit<SetCellLaunchMode>(parentProcess(), m_cell, newMode);
          }
        });
    con(cell, &CellModel::launchModeChanged, this, [this](LaunchMode m) {
      m_launchModeCombo->setCurrentIndex(static_cast<int>(m));
    });
    lay->addRow(tr("Launch"), m_launchModeCombo);

    // Trigger Style
    m_triggerStyleCombo = new QComboBox;
    m_triggerStyleCombo->addItems(
        {tr("Trigger"), tr("Toggle"), tr("Gate"), tr("Retrigger"), tr("Legato")});
    m_triggerStyleCombo->setCurrentIndex(static_cast<int>(cell.triggerStyle()));
    connect(
        m_triggerStyleCombo, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this](int idx) {
          auto newStyle = static_cast<TriggerStyle>(idx);
          if(newStyle != m_cell.triggerStyle())
          {
            CommandDispatcher<> disp{m_ctx.commandStack};
            disp.submit<SetCellTriggerStyle>(parentProcess(), m_cell, newStyle);
          }
        });
    con(cell, &CellModel::triggerStyleChanged, this, [this](TriggerStyle s) {
      m_triggerStyleCombo->setCurrentIndex(static_cast<int>(s));
    });
    lay->addRow(tr("Trigger"), m_triggerStyleCombo);

    // Velocity
    m_velocitySpin = new QDoubleSpinBox;
    m_velocitySpin->setRange(0.0, 1.0);
    m_velocitySpin->setSingleStep(0.05);
    m_velocitySpin->setValue(cell.velocity());
    connect(m_velocitySpin, &QDoubleSpinBox::editingFinished, this, [this] {
      double newVal = m_velocitySpin->value();
      if(newVal != m_cell.velocity())
      {
        CommandDispatcher<> disp{m_ctx.commandStack};
        disp.submit<SetCellVelocity>(parentProcess(), m_cell, newVal);
      }
    });
    con(cell, &CellModel::velocityChanged, m_velocitySpin, &QDoubleSpinBox::setValue);
    lay->addRow(tr("Velocity"), m_velocitySpin);

    section->addContent(w);
    parts.push_back(section);
  }

  // --- Follow Actions (Transition Rules) Section ---
  {
    auto section = new Inspector::InspectorSectionWidget{"Follow Actions", false, this};
    m_transitionRulesContainer = new QWidget;
    m_transitionRulesContainer->setLayout(new QVBoxLayout);
    m_transitionRulesContainer->layout()->setContentsMargins(0, 0, 0, 0);
    m_transitionRulesContainer->layout()->setSpacing(2);
    rebuildTransitionRulesList();

    auto addBtn = new QPushButton{tr("Add Follow Action")};
    connect(addBtn, &QPushButton::clicked, this, [this] {
      TransitionRule rule;
      int32_t maxId = 0;
      for(const auto& r : m_cell.transitionRules())
        maxId = std::max(maxId, r.id);
      rule.id = maxId + 1;
      rule.condition = TransitionRule::Condition::OnEnd;
      rule.target.scene = -1; // next scene

      CommandDispatcher<> disp{m_ctx.commandStack};
      disp.submit<AddTransitionRule>(parentProcess(), m_cell, rule);
    });

    con(cell, &CellModel::transitionRulesChanged, this,
        &CellInspectorWidget::rebuildTransitionRulesList);

    section->addContent(m_transitionRulesContainer);
    section->addContent(addBtn);
    parts.push_back(section);
  }

  // --- Interval Info Section ---
  {
    auto section = new Inspector::InspectorSectionWidget{"Interval", false, this};
    auto w = new QWidget;
    auto lay = new Inspector::Layout{w};

    QString name = cell.interval().metadata().getName();
    if(name.isEmpty())
      name = tr("(unnamed)");
    lay->addRow(tr("Name"), new QLabel{name});
    lay->addRow(
        tr("Duration"),
        new QLabel{cell.interval().duration.defaultDuration().toString()});
    lay->addRow(
        tr("Processes"),
        new QLabel{QString::number(cell.interval().processes.size())});

    section->addContent(w);
    parts.push_back(section);
  }

  updateAreaLayout(parts);
}

CellInspectorWidget::~CellInspectorWidget() = default;

void CellInspectorWidget::rebuildTransitionRulesList()
{
  auto layout = m_transitionRulesContainer->layout();
  QLayoutItem* item;
  while((item = layout->takeAt(0)) != nullptr)
  {
    delete item->widget();
    delete item;
  }

  for(const auto& rule : m_cell.transitionRules())
  {
    auto rowWidget = new QWidget;
    auto rowLay = new QHBoxLayout{rowWidget};
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(2);

    // Condition combo
    auto condCombo = new QComboBox;
    condCombo->addItems(
        {tr("On End"), tr("After Loop Count"), tr("On Trigger"), tr("Probability")});
    condCombo->setCurrentIndex(static_cast<int>(rule.condition));

    // Target combo
    auto targetCombo = new QComboBox;
    targetCombo->addItems({tr("Next Scene"), tr("Random"), tr("Specific...")});
    if(rule.target.scene == -1)
      targetCombo->setCurrentIndex(0);
    else if(rule.target.scene == -2)
      targetCombo->setCurrentIndex(1);
    else
      targetCombo->setCurrentIndex(2);

    // Remove button
    auto removeBtn = new QPushButton{tr("X")};
    removeBtn->setMaximumWidth(30);
    int32_t ruleId = rule.id;
    connect(removeBtn, &QPushButton::clicked, this, [this, ruleId] {
      for(const auto& r : m_cell.transitionRules())
      {
        if(r.id == ruleId)
        {
          CommandDispatcher<> disp{m_ctx.commandStack};
          disp.submit<RemoveTransitionRule>(parentProcess(), m_cell, r);
          return;
        }
      }
    });

    rowLay->addWidget(condCombo);
    rowLay->addWidget(targetCombo);
    rowLay->addWidget(removeBtn);
    layout->addWidget(rowWidget);
  }
}

} // namespace ClipLauncher
