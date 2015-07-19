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

        virtual iscore::DeviceSettings getSettings() const = 0;
        virtual QString getPath() const
        {
            return QString("");
        }
        virtual void setSettings(const iscore::DeviceSettings& settings) = 0;
};

