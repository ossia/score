#pragma once

#include <QWidget>
#include <QList>
#include <QString>

struct AddressSettings;

class AddressSettingsWidget : public QWidget
{
    public:
        AddressSettingsWidget(QWidget* parent = nullptr) : QWidget(parent) {}

        // TODO
        virtual AddressSettings getSettings() const = 0;
        virtual void setSettings(const AddressSettings& settings) = 0;

};


