#pragma once

#include <Process/LayerView.hpp>
#include <QString>
#include <QTextLayout>

class QQuickPaintedItem;
class QPainter;

namespace Mapping
{
class LayerView final : public Process::LayerView
{
public:
  explicit LayerView(QQuickPaintedItem* parent);

  void showName(bool b);

  void setDisplayedName(const QString& s);

private:
  void paint_impl(QPainter* painter) const override;

  QString m_displayedName;
  bool m_showName{true};

  QTextLayout m_textcache;
};
}
