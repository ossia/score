#pragma once

#include <QWidget>
#include <QList>
#include <QString>


class ProtocolSettingsWidget : public QWidget
{
    public:
        ProtocolSettingsWidget (QWidget* parent = nullptr) : QWidget (parent) {}

        //TODO: use QVariant instead ???
        /*

          The first element in returned list must be the name of the device.
         */
        virtual QList<QString> getSettings() const = 0;

        virtual void setSettings (const QList<QString>& settings) = 0;

};

