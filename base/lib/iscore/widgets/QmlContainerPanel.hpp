#pragma once

#include <QWidget>
#include <QString>
#include <QUrl>
#include <iscore_lib_base_export.h>
#include <QSize>

class QVBoxLayout;
class QQuickWidget;

class ISCORE_LIB_BASE_EXPORT QMLContainerPanel : public QWidget {
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

public slots:
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
