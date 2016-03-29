#pragma once
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimeEdit>
#include <QWheelEvent>
#include <type_traits>

namespace iscore
{
template<typename T>
/**
 * @brief The TemplatedSpinBox class
 *
 * Maps a fundamental type to a spinbox type.
 */
struct TemplatedSpinBox;
template<> struct TemplatedSpinBox<int> { using spinbox_type = QSpinBox; using value_type = int; };
template<> struct TemplatedSpinBox<float> { using spinbox_type = QDoubleSpinBox;  using value_type = float; };
template<> struct TemplatedSpinBox<double> { using spinbox_type = QDoubleSpinBox;  using value_type = double; };

/**
 * @brief The MaxRangeSpinBox class
 *
 * A spinbox mixin that will set its min and max to
 * the biggest positive and negative numbers for the given spinbox type.
 */
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

        void wheelEvent(QWheelEvent* event) override
        {
            event->ignore();
        }
};

/**
 * @brief The SpinBox class
 *
 * An abstraction for the most common spinbox type in iscore.
 */
template<typename T>
class SpinBox final : public MaxRangeSpinBox<TemplatedSpinBox<T>>
{
    public:
        using MaxRangeSpinBox<TemplatedSpinBox<T>>::MaxRangeSpinBox;
};

template<>
class SpinBox<double> final : public MaxRangeSpinBox<TemplatedSpinBox<double>>
{
    public:
        template<typename... Args>
        SpinBox(Args&&... args):
            MaxRangeSpinBox{std::forward<Args>(args)...}
        {
            setDecimals(5);
        }
};
template<>
class SpinBox<float> final : public MaxRangeSpinBox<TemplatedSpinBox<float>>
{
    public:
        template<typename... Args>
        SpinBox(Args&&... args):
            MaxRangeSpinBox{std::forward<Args>(args)...}
        {
            setDecimals(5);
        }
};

/**
 * @brief The TimeSpinBox class
 *
 * Adapted for the i-score usage in various duration widgets.
 */
class TimeSpinBox final : public QTimeEdit
{
    public:
        TimeSpinBox(QWidget* parent = 0):
            QTimeEdit(parent)
        {
            setDisplayFormat(QString("h.mm.ss.zzz"));
        }

        void wheelEvent(QWheelEvent* event) override
        {
            event->ignore();
        }
};

}
