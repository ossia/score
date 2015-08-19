#pragma once
#include <QSpinBox>
#include <QDoubleSpinBox>

template<typename T> struct TemplatedSpinBox;
template<> struct TemplatedSpinBox<int> { using spinbox_type = QSpinBox; using value_type = int; };
template<> struct TemplatedSpinBox<char> { using spinbox_type = QSpinBox;  using value_type = char; };
template<> struct TemplatedSpinBox<float> { using spinbox_type = QDoubleSpinBox;  using value_type = float; };
template<> struct TemplatedSpinBox<double> { using spinbox_type = QDoubleSpinBox;  using value_type = double; };


template<typename SpinBox>
class MaxRangeSpinBox : public SpinBox::spinbox_type
{
    public:
        template<typename... Args>
        MaxRangeSpinBox(Args&&... args):
            SpinBox::spinbox_type{std::forward<Args>(args)...}
        {
            this->setMinimum(std::numeric_limits<typename SpinBox::value_type>::lowest());
            this->setMaximum(std::numeric_limits<typename SpinBox::value_type>::max());
        }
};
