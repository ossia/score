#include "ModelConsistency.hpp"

ModelConsistency::ModelConsistency(QObject *parent) :
    QObject(parent),
    m_valid(true)
{
}

ModelConsistency::ModelConsistency(const ModelConsistency &other) :
    QObject{}
{
    setValid(other.isValid());
}

ModelConsistency &ModelConsistency::operator=(const ModelConsistency &other)
{
    setValid(other.isValid());
    return *this;
}

bool ModelConsistency::isValid() const
{
    return m_valid;
}

void ModelConsistency::setValid(bool arg)
{
    if (m_valid == arg)
        return;

    m_valid = arg;
    emit validChanged(arg);
}
