#include <QVBoxLayout>
#include <QLabel>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include "LocalProtocolSettingsWidget.hpp"
#include "LocalSpecificSettings.hpp"

class QWidget;

LocalProtocolSettingsWidget::LocalProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
    auto lay = new QVBoxLayout;
    QLabel* deviceNameLabel = new QLabel(tr("Local device"), this);
    lay->addWidget(deviceNameLabel);
    setLayout(lay);
}

iscore::DeviceSettings LocalProtocolSettingsWidget::getSettings() const
{
    iscore::DeviceSettings s;
    // TODO *** protocol is never set here. Check everywhere.! ***
    s.name = "i-score";
    LocalSpecificSettings local;
    s.deviceSpecificSettings = QVariant::fromValue(local);
    return s;
}

void
LocalProtocolSettingsWidget::setSettings(const iscore::DeviceSettings &settings)
{
    if(settings.deviceSpecificSettings.canConvert<LocalSpecificSettings>())
    {
    }
}
