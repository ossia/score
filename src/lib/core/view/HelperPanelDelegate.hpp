#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace score
{

class HelperPanelDelegate : public PanelDelegate
{
public:
  HelperPanelDelegate(const score::GUIApplicationContext& ctx)
      : PanelDelegate{ctx}
  {
    widg = new QWidget;
    widg->setContentsMargins(3, 2, 3, 2);
    widg->setMinimumHeight(100);
    widg->setMaximumHeight(100);
    widg->setMinimumWidth(180);

    auto l = new QVBoxLayout{widg};

    status = new QLabel;
    status->setTextFormat(Qt::RichText);
    status->setText("<i>Remember those quiet evenings</i>");
    status->setWordWrap(true);

#ifndef QT_NO_STYLE_STYLESHEET
    status->setStyleSheet("color: #787876;");
#endif
    l->addWidget(status);
    l->addStretch(12);
  }

  QWidget* widget() override { return widg; }

  const PanelStatus& defaultPanelStatus() const override
  {
    static const PanelStatus stat{true,
                                  true,
                                  Qt::RightDockWidgetArea,
                                  -100000,
                                  "Info",
                                  "info",
                                  QKeySequence::HelpContents};
    return stat;
  }
  QWidget* widg{};
  QLabel* status{};
};
}
