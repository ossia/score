#include "LaneInspectorWidget.hpp"

#include <Inspector/InspectorLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QLineEdit>

#include <ClipLauncher/Commands/SetLaneProperties.hpp>
#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

namespace ClipLauncher
{

const ProcessModel& LaneInspectorWidget::parentProcess() const
{
  return *safe_cast<const ProcessModel*>(m_lane.parent());
}

LaneInspectorWidget::LaneInspectorWidget(
    const LaneModel& lane, const score::DocumentContext& ctx, QWidget* parent)
    : InspectorWidgetBase{lane, ctx, parent, tr("Lane")}
    , m_lane{lane}
    , m_ctx{ctx}
{
  auto w = new QWidget;
  auto lay = new Inspector::Layout{w};

  // Name
  auto nameEdit = new QLineEdit{lane.name()};
  connect(nameEdit, &QLineEdit::editingFinished, this, [this, nameEdit] {
    if(nameEdit->text() != m_lane.name())
    {
      CommandDispatcher<> disp{m_ctx.commandStack};
      disp.submit<SetLaneName>(parentProcess(), m_lane, nameEdit->text());
    }
  });
  con(lane, &LaneModel::nameChanged, nameEdit, &QLineEdit::setText);
  lay->addRow(tr("Name"), nameEdit);

  // Exclusivity mode
  auto exclCombo = new QComboBox;
  exclCombo->addItems({tr("Exclusive"), tr("Polyphonic"), tr("Crossfade")});
  exclCombo->setCurrentIndex(static_cast<int>(lane.exclusivityMode()));
  connect(
      exclCombo, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      [this](int idx) {
        auto newMode = static_cast<ExclusivityMode>(idx);
        if(newMode != m_lane.exclusivityMode())
        {
          CommandDispatcher<> disp{m_ctx.commandStack};
          disp.submit<SetLaneExclusivityMode>(parentProcess(), m_lane, newMode);
        }
      });
  con(lane, &LaneModel::exclusivityModeChanged, this,
      [exclCombo](ExclusivityMode m) { exclCombo->setCurrentIndex(static_cast<int>(m)); });
  lay->addRow(tr("Exclusivity"), exclCombo);

  updateAreaLayout({w});
}

LaneInspectorWidget::~LaneInspectorWidget() = default;

} // namespace ClipLauncher
