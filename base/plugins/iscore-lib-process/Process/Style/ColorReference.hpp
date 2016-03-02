#pragma once
#include <QColor>
#include <utility>
#include <Process/Style/Skin.hpp>
#include <eggs/variant.hpp>
struct ColorRef
{
        using Color = eggs::variant<QColor, const QColor*>;
        Color color;

        template<typename Fun>
        void setColor(Fun f)
        {
            // Set color by reference
            color = &(Skin::instance().*fun);
        }

        void setColor(const QColor& col)
        {
            // Set color by value
            color = col;
        }

        QColor getColor() const
        {
            switch(color.which())
            {
                case 0:
                    return eggs::variants::get<0>(color);
                    break;
                case 1:
                    return eggs::variants::get<1>(color);
                    break;
                default:
                    return Qt::black;
            }
        }
};
