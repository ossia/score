#pragma once
#include <State/Value.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <QDoubleSpinBox>
#include <QWidget>
#include <QHBoxLayout>

namespace State
{
struct ISCORE_LIB_STATE_EXPORT IChanged
{
    public:
        virtual void changed() = 0;
        virtual ~IChanged() = default;
};

template<std::size_t N>
class ISCORE_LIB_STATE_EXPORT VecEdit :
        public QWidget,
        public IChanged
{
    public:
    VecEdit(QWidget* parent):
        QWidget{parent}
    {
        auto lay = new QHBoxLayout;
        this->setLayout(lay);

        for(std::size_t i = 0; i < N; i ++)
        {
            auto box = new QDoubleSpinBox{this};
            box->setMinimum(-9999);
            box->setMaximum(9999);
            box->setValue(0);

            connect(box, &QDoubleSpinBox::editingFinished,
                    this, [=] { changed(); });

            lay->addWidget(box);
            m_boxes[i] = box;
        }
    }

    void setValue(std::array<float, N> v)
    {
        for(std::size_t i = 0; i < N; i++)
        {
            m_boxes[i]->setValue(v[i]);
        }
    }

    State::vec3f value() const
    {
        std::array<float, N> v;
        for(std::size_t i = 0; i < N; i++)
        {
            v[i] = m_boxes[i]->value();
        }
        return v;
    }

    private:
        std::array<QDoubleSpinBox*, N> m_boxes;
};

class ISCORE_LIB_STATE_EXPORT Vec2DEdit final : public VecEdit<2>
{
        Q_OBJECT
    public:
        using VecEdit<2>::VecEdit;
        virtual ~Vec2DEdit() = default;
    signals:
        void changed() override;
};
class ISCORE_LIB_STATE_EXPORT Vec3DEdit final : public VecEdit<3>
{
        Q_OBJECT
    public:
        using VecEdit<3>::VecEdit;
        virtual ~Vec3DEdit() = default;
    signals:
        void changed() override;
};
class ISCORE_LIB_STATE_EXPORT Vec4DEdit final : public VecEdit<4>
{
        Q_OBJECT
    public:
        using VecEdit<4>::VecEdit;
        virtual ~Vec4DEdit() = default;
    signals:
        void changed() override;
};
}
