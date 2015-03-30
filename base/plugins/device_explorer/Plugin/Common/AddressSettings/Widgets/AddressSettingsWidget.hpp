#pragma once

#include <QWidget>
#include <QList>
#include <QString>


class AddressSettingsWidget : public QWidget
{
    public:
        AddressSettingsWidget(QWidget* parent = nullptr) : QWidget(parent) {}

        // TODO
        virtual QList<QString> getSettings() const = 0;
        virtual void setSettings(const QList<QString>& settings) = 0;

};


