#pragma once
#include <score/tools/Debug.hpp>

#include <ossia/dataflow/graph_node.hpp>

#include <QDataStream>

#include <verdigris>

inline QDataStream&
operator<<(QDataStream& i, const ossia::exec_state_facade& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, ossia::exec_state_facade& sel)
{
  SCORE_ABORT;
  return i;
}
Q_DECLARE_METATYPE(ossia::exec_state_facade)
W_REGISTER_ARGTYPE(ossia::exec_state_facade)

W_REGISTER_ARGTYPE(QVector<int>)
W_REGISTER_ARGTYPE(QVector<double>)
W_REGISTER_ARGTYPE(QVector<QString>)
