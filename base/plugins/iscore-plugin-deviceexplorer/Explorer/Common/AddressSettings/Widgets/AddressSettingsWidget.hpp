#pragma once

#include <Device/Address/AddressSettings.hpp>
#include <iscore_lib_device_export.h>
#include <QWidget>

class QComboBox;
class QFormLayout;


namespace Explorer
{
class ISCORE_LIB_DEVICE_EXPORT AddressSettingsWidget : public QWidget
{
    public:
        struct no_widgets_t {};
        explicit AddressSettingsWidget(QWidget* parent = nullptr);
        explicit AddressSettingsWidget(no_widgets_t, QWidget* parent = nullptr);

        virtual Device::AddressSettings getSettings() const = 0;
        virtual void setSettings(const Device::AddressSettings& settings) = 0;

    protected:
        Device::AddressSettings getCommonSettings() const;
        void setCommonSettings(const Device::AddressSettings&);
        QFormLayout* m_layout;

    private:
        bool m_none_type{false};
        QComboBox* m_ioTypeCBox;
        QComboBox* m_clipModeCBox;
        QComboBox* m_tagsEdit;
};
}

