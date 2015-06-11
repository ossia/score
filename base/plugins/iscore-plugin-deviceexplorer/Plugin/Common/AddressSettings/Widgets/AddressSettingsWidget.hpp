#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include "Common/AddressSettings/AddressSettings.hpp"

struct AddressSettings;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QFormLayout;

class AddressSettingsWidget : public QWidget
{
    public:
        AddressSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const = 0;
        virtual void setSettings(const AddressSettings& settings) = 0;

    protected:
        AddressSettings getCommonSettings() const;
        void setCommonSettings(const AddressSettings&);
        QFormLayout* m_layout;

    private:
        QComboBox* m_ioTypeCBox;
        QComboBox* m_clipModeCBox;
        QSpinBox* m_prioritySBox;
        QLineEdit* m_tagsEdit;
};


