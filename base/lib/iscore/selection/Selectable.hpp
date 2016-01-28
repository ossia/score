#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>

/**
 * @brief The Selectable class
 *
 * A component that allows a class to be selected (or not).
 */
class ISCORE_LIB_BASE_EXPORT Selectable final : public QObject
{
        Q_OBJECT
    public:
        Selectable()
        {
            connect(this, &Selectable::set, this, &Selectable::set_impl);
        }

        virtual ~Selectable()
        {
            set(false);
        }

        bool get() const
        {
            return m_val;
        }

        void set_impl(bool v)
        {
            if(m_val != v)
            {
                m_val = v;
                emit changed(v);
            }
        }

    signals:
        void set(bool) const;
        void changed(bool);

    private:
        bool m_val{};
};

