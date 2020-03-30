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

  auto btnLayout = new QHBoxLayout;
  btnLayout->setContentsMargins(0,0,0,0);

  // Full View
  {
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
    btnLayout->addWidget(fullview);
  }

  // Audio
  {
    ScenarioDocumentModel& doc = get<ScenarioDocumentModel>(*documentFromObject(m_model));
    auto busWidg = new QToolButton{this};
    busWidg->setIcon(makeIcons(QStringLiteral(":/icons/audio_bus_on.png")
                                , QStringLiteral(":/icons/audio_bus_off.png")
                                , QStringLiteral(":/icons/audio_bus_off.png")));
    busWidg->setToolTip(tr("Audio bus"));
    busWidg->setCheckable(true);
    busWidg->setChecked(ossia::contains(doc.busIntervals, &m_model));
    busWidg->setAutoRaise(true);
    busWidg->setIconSize(QSize{32,32});
    connect(busWidg, &QToolButton::toggled,
            this, [=, &ctx, &doc] (bool b) {
      bool is_bus = ossia::contains(doc.busIntervals, &m_model);
      if((b && !is_bus) || (!b && is_bus))
      {
          CommandDispatcher<> disp{ctx.commandStack};
          disp.submit<Command::SetBus>(doc, m_model, b);
      }
    });
    btnLayout->addWidget(busWidg);
  }
  // Time signature
  {
    auto sigWidg = new QToolButton{this};
    sigWidg->setIcon(makeIcons(QStringLiteral(":/icons/time_signature_on.png")
                                , QStringLiteral(":/icons/time_signature_off.png")
                                , QStringLiteral(":/icons/time_signature_off.png")));
    sigWidg->setToolTip(tr("Time signature"));
    sigWidg->setCheckable(true);
    sigWidg->setAutoRaise(true);
    sigWidg->setChecked(this->m_model.hasTimeSignature());

    sigWidg->setIconSize(QSize{32,32});
    connect(sigWidg, &QToolButton::toggled,
            this, [=] (bool b) {
      if(b != this->m_model.hasTimeSignature())
      {
        this->commandDispatcher()->submit<Command::SetHasTimeSignature>(m_model, b);
      }
    });
    btnLayout->addWidget(sigWidg);
  }
  {
    QWidget *spacerWidget = new QWidget(this);
    spacerWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    spacerWidget->setVisible(true);
    btnLayout->addWidget(spacerWidget);
  }

   lay->addRow(btnLayout);

  // Speed
  auto speedWidg = new SpeedWidget{true, true, this};
  speedWidg->setInterval(m_model);
  lay->addRow(tr("Speed"), speedWidg);

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
