#pragma once

#include <QWidget>
#include <QString>
#include <QUrl>
#include <iscore_lib_base_export.h>

class QQuickWidget;

class ISCORE_LIB_BASE_EXPORT QMLContainerPanel : public QWidget {
public:
    QMLContainerPanel(QWidget* parent = nullptr);
    QMLContainerPanel(QMLContainerPanel* container, QWidget* parent = nullptr);

    virtual void setSource(const QString&);
    QString source();
    virtual bool isCollapsed();

    ~QMLContainerPanel();

public slots:
    virtual void collapse(bool);

private:
    QString m_source;
    QWidget* m_widget;
    QQuickWidget* m_qcontainer;
};
