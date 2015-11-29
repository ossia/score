#pragma once

#include <Device/Address/AddressSettings.hpp>
#include <qwidget.h>

class QComboBox;
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
        QComboBox* m_tagsEdit;
};


