#pragma once

#include <QObject>

#include <score_plugin_curve_export.h>

#include <verdigris>

namespace Curve
{
enum class Tool
{
  Disabled,
  Select,
  Create,
  SetSegment,
  CreatePen,
  RemovePen,
  Playing
};
enum class AddPointBehaviour
{
  LinearBefore,
  LinearAfter,
  DuplicateSegment
};
enum class RemovePointBehaviour
{
  Remove,
  RemoveAndAddSegment
};

class SCORE_PLUGIN_CURVE_EXPORT EditionSettings : public QObject
{
  W_OBJECT(EditionSettings)

  bool m_lockBetweenPoints{true};
  bool m_suppressOnOverlap{true};
  bool m_stretchBothBounds{false};
  Curve::AddPointBehaviour m_addPointBehaviour{AddPointBehaviour::DuplicateSegment};
  Curve::RemovePointBehaviour m_removePointBehaviour{RemovePointBehaviour::RemoveAndAddSegment};
  Curve::Tool m_tool{Curve::Tool::Disabled};

public:
  bool lockBetweenPoints() const;
  bool suppressOnOverlap() const;
  bool stretchBothBounds() const;
  Curve::AddPointBehaviour addPointBehaviour() const;
  Curve::RemovePointBehaviour removePointBehaviour() const;
  Curve::Tool tool() const;

  void setLockBetweenPoints(bool);
  void setSuppressOnOverlap(bool);
  void setStretchBothBounds(bool);
  void setAddPointBehaviour(Curve::AddPointBehaviour);
  void setRemovePointBehaviour(Curve::RemovePointBehaviour removePointBehaviour);
  void setTool(Curve::Tool tool);

public:
  void lockBetweenPointsChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, lockBetweenPointsChanged, arg_1)
  void suppressOnOverlapChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, suppressOnOverlapChanged, arg_1)
  void stretchBothBoundsChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, stretchBothBoundsChanged, arg_1)
  void addPointBehaviourChanged(Curve::AddPointBehaviour arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, addPointBehaviourChanged, arg_1)
  void removePointBehaviourChanged(Curve::RemovePointBehaviour removePointBehaviour)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, removePointBehaviourChanged, removePointBehaviour)
  void toolChanged(Curve::Tool tool) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, toolChanged, tool)

  W_PROPERTY(Curve::Tool, tool READ tool WRITE setTool NOTIFY toolChanged)

  W_PROPERTY(
      Curve::RemovePointBehaviour,
      removePointBehaviour READ removePointBehaviour WRITE setRemovePointBehaviour NOTIFY
          removePointBehaviourChanged)

  W_PROPERTY(
      Curve::AddPointBehaviour,
      addPointBehaviour READ addPointBehaviour WRITE setAddPointBehaviour NOTIFY
          addPointBehaviourChanged)

  W_PROPERTY(
      bool,
      stretchBothBounds READ stretchBothBounds WRITE setStretchBothBounds NOTIFY
          stretchBothBoundsChanged)

  W_PROPERTY(
      bool,
      suppressOnOverlap READ suppressOnOverlap WRITE setSuppressOnOverlap NOTIFY
          suppressOnOverlapChanged)

  W_PROPERTY(
      bool,
      lockBetweenPoints READ lockBetweenPoints WRITE setLockBetweenPoints NOTIFY
          lockBetweenPointsChanged)
};
}

Q_DECLARE_METATYPE(Curve::Tool)
W_REGISTER_ARGTYPE(Curve::Tool)
W_REGISTER_ARGTYPE(Curve::AddPointBehaviour)
W_REGISTER_ARGTYPE(Curve::RemovePointBehaviour)
