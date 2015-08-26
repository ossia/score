#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <DeviceExplorer/Address/AddressSettings.hpp>

namespace iscore{
struct AddressSettings;
}
class QComboBox;
class QLineEdit;
class QSpinBox;
class QFormLayout;

class AddressSettingsWidget : public QWidget
{
    public:
        explicit AddressSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const = 0;
        virtual void setSettings(const iscore::AddressSettings& settings) = 0;

    protected:
        iscore::AddressSettings getCommonSettings() const;
        void setCommonSettings(const iscore::AddressSettings&);
        QFormLayout* m_layout;

    private:
        QComboBox* m_ioTypeCBox;
        QComboBox* m_clipModeCBox;
        // TODO priority QSpinBox* m_prioritySBox;
        QComboBox* m_tagsEdit;
};


