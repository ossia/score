#include "VecWidgets.hpp"
#include <iscore/widgets/SignalUtils.hpp>
#include <QDoubleSpinBox>
#include <QHBoxLayout>

namespace State
{

Vec3DEdit::Vec3DEdit(QWidget* parent):
    QWidget{parent}
{
    auto lay = new QHBoxLayout;
    this->setLayout(lay);

    m_boxes[0] = new QDoubleSpinBox{this};
    m_boxes[1] = new QDoubleSpinBox{this};
    m_boxes[2] = new QDoubleSpinBox{this};

    for(QDoubleSpinBox* box : m_boxes)
    {
        box->setMinimum(-9999);
        box->setMaximum(9999);
        box->setValue(0);

        connect(box, &QDoubleSpinBox::editingFinished,
                this, &Vec3DEdit::changed);

        lay->addWidget(box);
    }
}

void Vec3DEdit::setValue(vec3f v)
{
    constexpr const int n = v.size();
    for(int i = 0; i < n; i++)
    {
        m_boxes[i]->setValue(v[i]);
    }
}

vec3f Vec3DEdit::value() const
{
    State::vec3f v;
    ossia::transform(m_boxes, v.begin(), [] (auto ptr) { return ptr->value(); });
    return v;
}

}
