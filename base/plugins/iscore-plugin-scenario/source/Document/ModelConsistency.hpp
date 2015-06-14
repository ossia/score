#pragma once
#include <QObject>



class ModelConsistency : public QObject
{
        Q_OBJECT

        Q_PROPERTY(bool valid READ isValid WRITE setValid NOTIFY validChanged)
        Q_PROPERTY(bool warning READ warning WRITE setWarning NOTIFY warningChanged)

        bool m_valid;
        bool m_warning;

    public:
        explicit ModelConsistency(QObject *parent = 0);
        ModelConsistency(const ModelConsistency& other);
        ModelConsistency& operator=(const ModelConsistency& other);

        bool isValid() const;
        bool warning() const;

    signals:
        void validChanged(bool arg);
        void warningChanged(bool warning);

    public slots:
        void setValid(bool arg);
        void setWarning(bool warning);
};

