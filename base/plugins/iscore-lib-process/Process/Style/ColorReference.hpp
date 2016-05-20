#pragma once
#include <QColor>
#include <QBrush>
#include <utility>
#include <Process/Style/Skin.hpp>
#include <eggs/variant.hpp>
#include <boost/optional.hpp>

struct ISCORE_LIB_PROCESS_EXPORT ColorRef
{
        friend bool operator==(ColorRef lhs, ColorRef rhs)
        {
            return lhs.ref == rhs.ref;
        }

        friend bool operator!=(ColorRef lhs, ColorRef rhs)
        {
            return lhs.ref != rhs.ref;
        }

    public:
        ColorRef() = default;
        ColorRef(const ColorRef& other) = default;
        ColorRef(ColorRef&& other) = default;
        ColorRef& operator=(const ColorRef& other) = default;
        ColorRef& operator=(ColorRef&& other) = default;

        ColorRef(QColor Skin::*s):
            ref{&(Skin::instance().*s)}
        {

        }

        ColorRef(const QColor* col):
            ref{col}
        {

        }

        void setColor(QColor Skin::*s)
        {
            // Set color by reference
            ref = &(Skin::instance().*s);
        }

        QColor getColor() const
        {
            return ref ? *ref : Qt::black;
        }

        QBrush getBrush() const
        {
            return getColor();
        }


        QString name() const
        {
            return Skin::instance().toString(ref);
        }

        static boost::optional<ColorRef> ColorFromString(const QString&);
        static boost::optional<ColorRef> SimilarColor(QColor other);

    private:
        const QColor* ref{};
};


Q_DECLARE_METATYPE(ColorRef)
