#pragma once
#include <QObject>

class Selectable : public QObject
{
        Q_OBJECT
    public:
        using QObject::QObject;
        virtual ~Selectable()
        {
            set(false);
        }

        bool get() const
        {
            return m_val;
        }

        void set(bool v)
        {
            if(m_val != v)
            {
                m_val = v;
                emit changed(v);
            }
        }

    signals:
        void changed(bool);

    private:
        bool m_val{};
};

