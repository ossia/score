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

    struct FasterLabel : QLabel
    {
      QSize sizeHint() const override { return {180, 100}; }
      QSize minimumSizeHint() const override { return {180, 100}; }
      int heightForWidth(int) const override { return 100; }
    };

    status = new FasterLabel;
    status->setTextFormat(Qt::RichText);
    status->setText("<i>Remember those quiet evenings</i>");
    status->setWordWrap(true);

    l->addWidget(status);
    l->addStretch(12);
  }

  QWidget* widget() override { return widg; }

  const PanelStatus& defaultPanelStatus() const override
  {
    static const PanelStatus stat{true,   true,   Qt::RightDockWidgetArea,   -100000,
                                  "Info", "info", QKeySequence::HelpContents};
    return stat;
  }
  QWidget* widg{};
  QLabel* status{};
};
}
