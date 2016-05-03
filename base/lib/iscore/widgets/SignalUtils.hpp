#pragma once
#include <QSpinBox>
#include <QComboBox>

struct SignalUtils
{
        static const constexpr auto QSpinBox_valueChanged_int = static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged);
        static const constexpr auto QDoubleSpinBox_valueChanged_double = static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged);
        static const constexpr auto QComboBox_currentIndexChanged_int = static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged);
        static const constexpr auto QComboBox_activated_int = static_cast<void (QComboBox::*)(int)>(&QComboBox::activated);
};
