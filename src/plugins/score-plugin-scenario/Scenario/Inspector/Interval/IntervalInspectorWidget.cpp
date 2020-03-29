// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "IntervalInspectorWidget.hpp"

#include <Inspector/InspectorLayout.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>
#include <Scenario/Inspector/Interval/Widgets/DurationSectionWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Commands/Signature/SignatureCommands.hpp>
#include <Scenario/Commands/Interval/MakeBus.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/StyleSheets.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCheckBox>
#include <QToolBar>

namespace Scenario
{
IntervalInspectorWidget::IntervalInspectorWidget(
    const Inspector::InspectorWidgetList& widg,
    const IntervalModel& object,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object,
                          ctx,
                          parent,
                          tr("Interval (%1)").arg(object.metadata().getName())}
    , m_model{object}
{
  using namespace score;
  using namespace score::IDocument;
  setObjectName("Interval");

  std::vector<QWidget*> parts;
  ////// HEADER
  // metadata
  auto meta = new MetadataWidget{
      m_model.metadata(), ctx.commandStack, &m_model, this};

  meta->setupConnections(m_model);
  addHeader(meta);

  ////// BODY
  // Separator
  auto w = new QWidget;
  auto lay = new Inspector::Layout{w};
  parts.push_back(w);

  // Full View
  auto fullview = new QToolButton;
  fullview->setIcon(makeIcons(QStringLiteral(":/icons/fullview_on.png")
                              , QStringLiteral(":/icons/fullview_off.png")
                              , QStringLiteral(":/icons/fullview_off.png")));
  fullview->setToolTip(tr("FullView"));
  fullview->setAutoRaise(true);
  fullview->setIconSize(QSize{32,32});
  connect(fullview, &QToolButton::clicked, this, [this] {
    auto base = get<ScenarioDocumentPresenter>(*documentFromObject(m_model));
    if (base)
      base->setDisplayedInterval(model());
  });
  lay->addRow(fullview);
  lay->setAlignment(fullview, Qt::AlignHCenter);

  // Audio
  ScenarioDocumentModel& doc = get<ScenarioDocumentModel>(*documentFromObject(m_model));
  auto busWidg = new QCheckBox{this};
  busWidg->setChecked(ossia::contains(doc.busIntervals, &m_model));
  connect(busWidg, &QCheckBox::toggled,
          this, [=, &ctx, &doc] (bool b) {
    bool is_bus = ossia::contains(doc.busIntervals, &m_model);
    if((b && !is_bus) || (!b && is_bus))
    {
        CommandDispatcher<> disp{ctx.commandStack};
        disp.submit<Command::SetBus>(doc, m_model, b);
    }
  });
  lay->addRow(tr("Audio bus"), busWidg);

  // Speed
  auto speedWidg = new SpeedWidget{true, true, this};
  speedWidg->setInterval(m_model);
  lay->addRow(tr("Speed"), speedWidg);

  // signature
  auto sigWidg = new QCheckBox{this};
  sigWidg->setChecked(this->m_model.hasTimeSignature());
  lay->addRow(tr("Time signature"), sigWidg);
  connect(sigWidg, &QCheckBox::toggled,
          this, [=] (bool b) {
    if(b != this->m_model.hasTimeSignature())
    {
      this->commandDispatcher()->submit<Command::SetHasTimeSignature>(m_model, b);
    }
  });

  // Durations
  auto& ctrl = ctx.app.guiApplicationPlugin<ScenarioApplicationPlugin>();
  auto dur = new DurationWidget{ctrl.editionSettings(), *lay, this};
  lay->addWidget(dur);

  // Display data
  updateAreaLayout(parts);
}

IntervalInspectorWidget::~IntervalInspectorWidget() = default;

IntervalModel& IntervalInspectorWidget::model() const
{
  return const_cast<IntervalModel&>(m_model);
}
}
