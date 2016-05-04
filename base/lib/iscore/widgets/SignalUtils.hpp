#pragma once
#include <QSpinBox>
#include <QComboBox>

struct SignalUtils
{
        static constexpr auto QSpinBox_valueChanged_int() { return static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged); }
        static constexpr auto QDoubleSpinBox_valueChanged_double() { return static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged); }
        static constexpr auto QComboBox_currentIndexChanged_int() { return static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged); }
        static constexpr auto QComboBox_activated_int() { return static_cast<void (QComboBox::*)(int)>(&QComboBox::activated); }
};
