#include "MessagesPanel.hpp"
#include <QDockWidget>
#include <QListWidget>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QMenu>
namespace Engine
{
PanelDelegate::PanelDelegate(const iscore::ApplicationContext& ctx):
    iscore::PanelDelegate{ctx},
    m_widget{new QListWidget}
{
    m_widget->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(m_widget, &QListWidget::customContextMenuRequested,
            this, [=] (const QPoint &pos) {
        QMenu m{};
        auto act = m.addAction(QObject::tr("Clear"));
        auto res = m.exec(QCursor::pos());
        if(res == act)
        {
            m_widget->clear();
        }
    });
}

QWidget*PanelDelegate::widget()
{
    return m_widget;
}

const iscore::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
    static const iscore::PanelStatus status{
        false,
        Qt::BottomDockWidgetArea,
                0,
                QObject::tr("Messages"),
                QObject::tr("Ctrl+Shift+M")};

    return status;
}

void PanelDelegate::on_modelChanged(
        iscore::MaybeDocument oldm,
        iscore::MaybeDocument newm)
{
    QObject::disconnect(m_inbound);
    QObject::disconnect(m_outbound);
    QObject::disconnect(m_visible);

    if(!newm)
        return;

    if(auto qw = qobject_cast<QDockWidget*>(m_widget->parent()))
    {
        m_visible = QObject::connect(qw, &QDockWidget::visibilityChanged,
                         [=] (bool visible) {
            if(auto devices = getDeviceList(newm))
            {
                if(visible)
                {
                    setupConnections(*devices);
                }

                devices->setLogging(visible);
            }
        });

        if(auto devices = getDeviceList(newm))
        {
            if(qw->isVisible())
            {
                setupConnections(*devices);
            }
            devices->setLogging(qw->isVisible());
        }
    }
}

void PanelDelegate::setupConnections(Device::DeviceList& devices)
{
    QObject::disconnect(m_inbound);
    QObject::disconnect(m_outbound);

    const auto dark1 = QColor(Qt::darkGray).darker();
    const auto dark2 = dark1.darker(); // almost darker than black
    m_inbound = QObject::connect(&devices, &Device::DeviceList::logInbound,
                                 m_widget, [=] (const QString& str) {
        auto lw = new QListWidgetItem{str};
        lw->setBackgroundColor(dark1);
        m_widget->addItem(lw);
        if(m_widget->count() > 500)
            delete m_widget->takeItem(0);
        m_widget->scrollToBottom();
    }, Qt::QueuedConnection);

    m_outbound = QObject::connect(&devices, &Device::DeviceList::logOutbound,
                                 m_widget, [=] (const QString& str) {
        auto lw = new QListWidgetItem{str};
        lw->setBackgroundColor(dark2);
        m_widget->addItem(lw);
        if(m_widget->count() > 500)
            delete m_widget->takeItem(0);
        m_widget->scrollToBottom();
    }, Qt::QueuedConnection);
}

Device::DeviceList* PanelDelegate::getDeviceList(iscore::MaybeDocument newm)
{
    if(!newm)
        return nullptr;

    auto plug = newm->findPlugin<Explorer::DeviceDocumentPlugin>();
    if(!plug)
        return nullptr;
    return &plug->list();
}

std::unique_ptr<iscore::PanelDelegate> PanelDelegateFactory::make(
        const iscore::ApplicationContext& ctx)
{
    return std::make_unique<PanelDelegate>(ctx);
}

}
