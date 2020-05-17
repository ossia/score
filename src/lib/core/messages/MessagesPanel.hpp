#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

#include <score_lib_base_export.h>
class QListWidget;
class QListView;
class QDockWidget;
namespace score
{
class LogMessagesItemModel;

// REFACTORME
namespace log
{
static const QColor dark1 = QColor(Qt::darkGray).darker();
static const QColor dark2 = dark1.darker(); // almost darker than black
static const QColor dark3 = QColor(Qt::darkRed).darker();
static const QColor dark4 = QColor(Qt::darkBlue).darker();
}
class SCORE_LIB_BASE_EXPORT MessagesPanelDelegate final : public QObject,
                                                          public score::PanelDelegate
{
  friend class err_sink;
  W_OBJECT(MessagesPanelDelegate)

public:
  MessagesPanelDelegate(const score::GUIApplicationContext& ctx);

  void push(const QString& str, const QColor& col);
  void qtLog(const std::string& str);

  QDockWidget* dock();

private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;

  LogMessagesItemModel* m_itemModel{};
  QListView* m_widget{};
};

class MessagesPanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("84a66cbe-aee3-496a-b7f4-0ea0d699deac")

  std::unique_ptr<score::PanelDelegate> make(const score::GUIApplicationContext& ctx) override;
};
}
