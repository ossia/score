#pragma once
#include <tools/IdentifiedObject.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>

class TimeNodeModel : public IdentifiedObject
{
    Q_OBJECT

    public:
        TimeNodeModel(int id, QObject* parent);
        TimeNodeModel(int id, int date, QObject *parent);
        ~TimeNodeModel();

        void addEvent(int);
        bool removeEvent(int);

        double top() const;
        double bottom() const;
        int date() const;

        void setTop(double);
        void setBottom(double);
        void setDate(int);

        double y() const;
        void setY(double y);

    signals:

    public slots:

    private:
        int m_date{0};
        double m_y{0.0};

        QVector<int> m_events;

        // @todo : maybe not useful ...
        double m_topY{0.0};
        double m_bottomY{0.0};
};
