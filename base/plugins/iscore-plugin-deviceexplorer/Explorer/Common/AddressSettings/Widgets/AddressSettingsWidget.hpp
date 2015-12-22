#pragma once

#include <Device/Address/AddressSettings.hpp>
#include <iscore_lib_device_export.h>
#include <QWidget>

class QComboBox;
class QFormLayout;

class ISCORE_LIB_DEVICE_EXPORT AddressSettingsWidget : public QWidget
{
    public:
        struct no_widgets_t {};
        explicit AddressSettingsWidget(QWidget* parent = nullptr);
        explicit AddressSettingsWidget(no_widgets_t, QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const = 0;
        virtual void setSettings(const iscore::AddressSettings& settings) = 0;

    protected:
        iscore::AddressSettings getCommonSettings() const;
        void setCommonSettings(const iscore::AddressSettings&);
        QFormLayout* m_layout;

    private:
        bool m_none_type{false};
        QComboBox* m_ioTypeCBox;
        QComboBox* m_clipModeCBox;
        QComboBox* m_tagsEdit;
};


