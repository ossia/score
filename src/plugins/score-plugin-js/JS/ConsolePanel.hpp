#pragma once

#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QJSEngine>
#include <QJSValueIterator>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QtMath>

#include <score_plugin_js_export.h>
#include <wobjectimpl.h>
namespace JS
{

struct ActionContext : public QObject
{
  W_OBJECT(ActionContext)
public:
  QString Menu = "Menu";

  W_PROPERTY(QString, Menu READ Menu)
};
class SCORE_PLUGIN_JS_EXPORT PanelDelegate final
    : public QObject
    , public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

  QJSEngine& engine() noexcept;

  void evaluate(const QString& txt);
  void compute(const QString& txt, std::function<void(QVariant)> r);
  QMenu* addMenu(QMenu* cur, QStringList names);
  void importModule(const QString& path);

private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;

  QWidget* m_widget{};
  QPlainTextEdit* m_edit{};
  QLineEdit* m_lineEdit{};
  QJSEngine m_engine;
};

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("6060a63c-26b1-4ec6-a468-27e72530ac69")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {
    return std::make_unique<PanelDelegate>(ctx);
  }
};
}
