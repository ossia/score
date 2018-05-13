#pragma once

#include <QSize>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <score_lib_base_export.h>

class QVBoxLayout;
class QQuickWidget;

class SCORE_LIB_BASE_EXPORT QMLContainerPanel : public QWidget
{
public:
  QMLContainerPanel(QWidget* parent = nullptr);
  QMLContainerPanel(QMLContainerPanel* container, QWidget* parent = nullptr);

  virtual void setSource(const QString&);
  QString source() const;
  virtual bool isCollapsed() const;
  const QSize containerSize() const;
  void setContainerSize(const QSize& s);
  void setContainerSize(const int& w, const int& h);

  void show();
  ~QMLContainerPanel();

public:
  virtual void collapse();
  virtual QQuickWidget* container();
  virtual QWidget* rootWidget();

private:
  QString m_source;
  QWidget* m_widget{};
  QVBoxLayout* m_layout{};
  QQuickWidget* m_qcontainer{};
  QAction* m_collapse{};
};
