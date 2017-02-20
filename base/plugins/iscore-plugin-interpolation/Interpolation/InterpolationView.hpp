#pragma once
#include <Process/LayerView.hpp>
#include <QTextLayout>

namespace Interpolation
{
class View final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit View(QQuickPaintedItem* parent);
  virtual ~View();

  void setDisplayedName(const QString& s);
  void showName(bool b)
  {
    m_showName = b;
    update();
  }

signals:
  void dropReceived(const QMimeData* mime);

protected:
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QDropEvent* event) override;

private:
  bool m_showName{true};

  QTextLayout m_textcache;
};
}
