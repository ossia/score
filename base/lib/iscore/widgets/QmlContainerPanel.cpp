#include "QmlContainerPanel.hpp"
#include <QObject>
#include <QWidget>
#include <QString>
#include <QDockWidget>
#include <QtQuickWidgets/QQuickWidget>
#include <QUrl>
#include <QToolButton>
#include <QAction>
#include <QIcon>
#include <QVBoxLayout>
#include <QQmlEngine>
#include <QQmlContext>

#include <iscore/widgets/SetIcons.hpp>

QMLContainerPanel::QMLContainerPanel(QWidget *parent) :
    QWidget{parent},
    m_widget{new QWidget(this)},
    m_layout{new QVBoxLayout},
    m_qcontainer{new QQuickWidget},
    m_collapse{new QAction(tr("Collapse"), this)}
{
    QIcon icon = genIconFromPixmaps(QString(":/icons/condition_remove_on.png"),
                                         QString(":/icons/condition_remove_off.png"));
    m_collapse->setIcon(icon);
    m_collapse->setCheckable(true);
    m_collapse->setToolTip(tr("Collapse tracks"));

    m_qcontainer->setMinimumWidth(60);

    auto button = new QToolButton(this);
    button->setText(tr("Collapse Tracks"));
    button->setDefaultAction(m_collapse);
    m_layout->addWidget(button);
    m_layout->addWidget(m_qcontainer);
    m_widget->setLayout(m_layout);

    connect(m_collapse, &QAction::triggered, this, &QMLContainerPanel::collapse);
    qDebug() << &QMLContainerPanel::collapse;
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

QString QMLContainerPanel::source() const {
    return QString(m_source);
}

bool QMLContainerPanel::isCollapsed() const {
    return (m_qcontainer != nullptr && m_qcontainer->isVisible());
}

void QMLContainerPanel::collapse() {
    bool checked = m_collapse->isChecked();
    m_qcontainer->setVisible(!checked);

    if(checked)
        m_widget->resize(50, 50);
    else
        m_widget->resize(m_qcontainer->size());

    qDebug() << "collapsing:" << checked << "is visible" << m_qcontainer->isVisible();
}

QWidget* QMLContainerPanel::rootWidget() {
    return m_widget;
}

QQuickWidget* QMLContainerPanel::container() {
    return m_qcontainer;
}

const QSize QMLContainerPanel::internalSize() const {
    return m_qcontainer->size();
}

void QMLContainerPanel::setInternalSize(const QSize &s) {
    m_qcontainer->setMinimumSize(s);
}

void QMLContainerPanel::setInternalSize(const int &w, const int &h) {
    setInternalSize(QSize(w, h));
}

QMLContainerPanel::~QMLContainerPanel() {

}
