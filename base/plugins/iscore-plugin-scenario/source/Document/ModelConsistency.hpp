#pragma once
#include <QObject>



class ModelConsistency : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool valid READ isValid WRITE setValid NOTIFY validChanged)

    bool m_valid;

public:
    explicit ModelConsistency(QObject *parent = 0);
    ModelConsistency(const ModelConsistency& other);
    ModelConsistency& operator=(const ModelConsistency& other);

bool isValid() const;

signals:

void validChanged(bool arg);

public slots:

void setValid(bool arg);
};

