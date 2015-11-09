#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <QVariant>
#include <Device/Protocol/DeviceSettings.hpp>

class ProtocolSettingsWidget : public QWidget
{
    public:
        explicit ProtocolSettingsWidget(QWidget* parent = nullptr) : QWidget(parent) {}

        virtual ~ProtocolSettingsWidget();
        virtual iscore::DeviceSettings getSettings() const = 0;
        virtual QString getPath() const
        {
            return QString("");
        }
        virtual void setSettings(const iscore::DeviceSettings& settings) = 0;
};

