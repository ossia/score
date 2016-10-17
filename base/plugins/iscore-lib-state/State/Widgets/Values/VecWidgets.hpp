#pragma once
#include <QWidget>
#include <State/Value.hpp>
class QDoubleSpinBox;
namespace State
{

class ISCORE_LIB_STATE_EXPORT Vec3DEdit : public QWidget
{
        Q_OBJECT
    public:
        Vec3DEdit(QWidget* parent);

        void setValue(State::vec3f v);

        State::vec3f value() const;

    signals:
        void changed();

    private:
        std::array<QDoubleSpinBox*, 3> m_boxes;
};
}
