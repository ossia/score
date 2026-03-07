#include "LaneInspectorWidget.hpp"

#include <Inspector/InspectorLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
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

  // Blend mode
  auto blendCombo = new QComboBox;
  blendCombo->addItems(
      {tr("Add"), tr("Average"), tr("Color Burn"), tr("Color Dodge"), tr("Darken"),
       tr("Difference"), tr("Exclusion"), tr("Glow"), tr("Hard Light"), tr("Hard Mix"),
       tr("Lighten"), tr("Linear Burn"), tr("Linear Dodge"), tr("Linear Light"),
       tr("Multiply"), tr("Negation"), tr("Normal"), tr("Overlay"), tr("Phoenix"),
       tr("Pin Light"), tr("Reflect"), tr("Screen"), tr("Soft Light"), tr("Subtract"),
       tr("Vivid Light")});
  // Enum values start at 1, combo index starts at 0
  blendCombo->setCurrentIndex(static_cast<int>(lane.blendMode()) - 1);
  connect(
      blendCombo, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      [this](int idx) {
        auto newMode = static_cast<VideoBlendMode>(idx + 1);
        if(newMode != m_lane.blendMode())
        {
          CommandDispatcher<> disp{m_ctx.commandStack};
          disp.submit<SetLaneBlendMode>(parentProcess(), m_lane, newMode);
        }
      });
  con(lane, &LaneModel::blendModeChanged, this, [blendCombo](VideoBlendMode m) {
    blendCombo->setCurrentIndex(static_cast<int>(m) - 1);
  });
  lay->addRow(tr("Blend Mode"), blendCombo);

  // Video opacity
  auto opacitySpin = new QDoubleSpinBox;
  opacitySpin->setRange(0.0, 1.0);
  opacitySpin->setSingleStep(0.05);
  opacitySpin->setValue(lane.videoOpacity());
  connect(opacitySpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    if(v != m_lane.videoOpacity())
    {
      CommandDispatcher<> disp{m_ctx.commandStack};
      disp.submit<SetLaneVideoOpacity>(parentProcess(), m_lane, v);
    }
  });
  con(lane, &LaneModel::videoOpacityChanged, opacitySpin, &QDoubleSpinBox::setValue);
  lay->addRow(tr("Video Opacity"), opacitySpin);

  // Volume
  auto volumeSpin = new QDoubleSpinBox;
  volumeSpin->setRange(0.0, 2.0);
  volumeSpin->setSingleStep(0.05);
  volumeSpin->setValue(lane.volume());
  connect(volumeSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    if(v != m_lane.volume())
    {
      CommandDispatcher<> disp{m_ctx.commandStack};
      disp.submit<SetLaneVolume>(parentProcess(), m_lane, v);
    }
  });
  con(lane, &LaneModel::volumeChanged, volumeSpin, &QDoubleSpinBox::setValue);
  lay->addRow(tr("Volume"), volumeSpin);

  updateAreaLayout({w});
}

LaneInspectorWidget::~LaneInspectorWidget() = default;

} // namespace ClipLauncher
