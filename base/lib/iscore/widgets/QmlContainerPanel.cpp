#include "QmlContainerPanel.hpp"
#include <QObject>
#include <QWidget>
#include <QString>
#include <QDockWidget>
#include <QtQuickWidgets/QQuickWidget>
#include <QUrl>
#include <QPushButton>
#include <QAction>
#include <QIcon>
#include <QVBoxLayout>

#include <iscore/widgets/SetIcons.hpp>

QMLContainerPanel::QMLContainerPanel(QWidget *parent) :
    QWidget{parent},
    m_widget{new QWidget(this)},
    m_qcontainer{new QQuickWidget{m_widget}}
{
    auto lay = new QVBoxLayout{m_widget};
    QIcon icon = genIconFromPixmaps(QString(":/icons/condition_remove_on.png"),
                                         QString(":/icons/condition_remove_off.png"));
    auto action = new QAction(icon, "Collapse", this);
    action->setCheckable(true);
    auto button = new QPushButton(tr("Collapse"), this);
    button->addAction(action);
    lay->addWidget(button);
    lay->addWidget(m_qcontainer);
    m_widget->setLayout(lay);

    connect(action, &QAction::triggered, this, &QMLContainerPanel::collapse);
}

QMLContainerPanel::QMLContainerPanel(QMLContainerPanel *container, QWidget *parent) :
    QMLContainerPanel(parent)
{
    setSource(container->source());
}

void QMLContainerPanel::setSource(const QString &src) {
    m_source = src;
    if (m_qcontainer)
        m_qcontainer->setSource(m_source);
}

QString QMLContainerPanel::source() {
    return QString(m_source);
}

bool QMLContainerPanel::isCollapsed() {
    return (m_qcontainer != nullptr && m_qcontainer->isVisible());
}

void QMLContainerPanel::collapse(bool checked) {
    m_qcontainer->setVisible(checked);
    qDebug() << "collapsing:" << checked << m_qcontainer->isVisible();
}

QMLContainerPanel::~QMLContainerPanel() {

}
