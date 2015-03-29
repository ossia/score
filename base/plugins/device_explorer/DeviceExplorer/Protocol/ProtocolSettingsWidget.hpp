#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <QVariant>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

class ProtocolSettingsWidget : public QWidget
{
    public:
        ProtocolSettingsWidget(QWidget* parent = nullptr) : QWidget(parent) {}

        virtual DeviceSettings getSettings() const = 0;
        virtual void setSettings(const QList<QString>& settings) = 0;
};

