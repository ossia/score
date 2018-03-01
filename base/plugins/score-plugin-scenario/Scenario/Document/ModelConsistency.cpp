// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ModelConsistency.hpp"
namespace Scenario
{
ModelConsistency::ModelConsistency(QObject* parent)
    : QObject(parent), m_valid(true)
{
}

ModelConsistency::ModelConsistency(const ModelConsistency& other) : QObject{}
{
  setValid(other.isValid());
}

ModelConsistency& ModelConsistency::operator=(const ModelConsistency& other)
{
  setValid(other.isValid());
  return *this;
}

bool ModelConsistency::isValid() const
{
  return m_valid;
}

bool ModelConsistency::warning() const
{
  return m_warning;
}

void ModelConsistency::setValid(bool arg)
{
  if (m_valid == arg)
    return;

  m_valid = arg;
  validChanged(arg);
}

void ModelConsistency::setWarning(bool warning)
{
  if (m_warning == warning)
    return;

  m_warning = warning;
  warningChanged(warning);
}
}
