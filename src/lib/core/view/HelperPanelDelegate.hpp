#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace score
{

class HelperPanelDelegate : public PanelDelegate
{
public:
  struct FasterLabel : QLabel
  {
    QSize sizeHint() const override { return {180, 200}; }
    QSize minimumSizeHint() const override { return {180, 100}; }
    int heightForWidth(int) const override { return 200; }
  };

  HelperPanelDelegate(const score::GUIApplicationContext& ctx)
      : PanelDelegate{ctx}
  {
    widg = status = new FasterLabel;
    widg->setContentsMargins(3, 5, 3, 2);
    widg->setMinimumHeight(100);
    widg->setMaximumHeight(200);
    widg->setMinimumWidth(180);

    status->setAlignment(Qt::AlignTop);
    status->setTextFormat(Qt::RichText);
    status->setText("<i>Do the words need changing?</i>");
    status->setWordWrap(true);
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
