#pragma once

#include <score/widgets/SearchLineEdit.hpp>
#include <QContextMenuEvent>

namespace Scenario {
class SearchWidget final : public score::SearchLineEdit
{
public:
  SearchWidget(const score::GUIApplicationContext& ctx);

  void toggle() { this->isHidden() ? this->show() : this->hide(); }

private:
  void search() override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void on_findAddresses(QStringList strlst);

  const score::GUIApplicationContext& m_ctx;
};
}
