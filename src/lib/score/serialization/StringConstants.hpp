#pragma once
#include <QString>

#include <score_lib_base_export.h>

#include <string>
namespace score
{
struct StringConstants
{
  const std::string k;
  const std::string v;
  const std::string id;
  const std::string none;
  const std::string Identifiers;
  const std::string Type;
  const std::string Value;
  const std::string Address;
  const std::string Message;
  const std::string value;
  const std::string address;
  const std::string LHS;
  const std::string Op;
  const std::string RHS;
  const std::string Previous;
  const std::string Following;
  const std::string User;
  const std::string Priorities;
  const std::string Process;
  const std::string Name;
  const std::string ObjectName;
  const std::string ObjectId;
  const std::string Children;
  const std::string Min;
  const std::string Max;
  const std::string Values;
  const std::string Device;
  const std::string Path;
  const std::string ioType;
  const std::string ClipMode;
  const std::string Unit;
  const std::string unit;
  const std::string RepetitionFilter;
  const std::string RefreshRate;
  const std::string Priority;
  const std::string Tags;
  const std::string Domain;
  const std::string Protocol;
  const std::string Duration;
  const std::string Metadata;
  const std::string lowercase_true;
  const std::string lowercase_false;
  const std::string True;
  const std::string False;
  const std::string lowercase_yes;
  const std::string lowercase_no;
  const std::string Yes;
  const std::string No;
  const std::string Start;
  const std::string End;
  const std::string ScriptingName;
  const std::string Comment;
  const std::string Color;
  const std::string Label;
  const std::string Extended;
  const std::string Touched;
  const std::string uuid;
  const std::string Description;
  const std::string Components;
  const std::string Parents;
  const std::string Accessors;
  const std::string Data;
  const std::string Power;
  const std::string DefaultDuration;
  const std::string MinDuration;
  const std::string MaxDuration;
  const std::string GuiDuration;
  const std::string Rigidity;
  const std::string MinNull;
  const std::string MaxInf;
  const std::string Processes;
  const std::string Racks;
  const std::string FullView;
  const std::string Slots;
  const std::string StartState;
  const std::string EndState;
  const std::string Date;
  const std::string StartDate;
  const std::string PreviousInterval;
  const std::string NextInterval;
  const std::string Events;
  const std::string Extent;
  const std::string Active;
  const std::string Expression;
  const std::string Trigger;
  const std::string Event;
  const std::string HeightPercentage;
  const std::string Messages;
  const std::string StateProcesses;
  const std::string TimeSync;
  const std::string States;
  const std::string Condition;
  const std::string Offset;
  const std::string Segments;
  const std::string SmallViewRack;
  const std::string FullViewRack;
  const std::string Zoom;
  const std::string Center;
  const std::string SmallViewShown;
  const std::string Height;
  const std::string Width;
  const std::string Inlet;
  const std::string Outlet;
  const std::string Inlets;
  const std::string Outlets;
  const std::string Hidden;
  const std::string Custom;
  const std::string Exposed;
  const std::string Propagate;
  const std::string Source;
  const std::string Sink;
  const std::string AutoTrigger;
  const std::string Text;
  const std::string Host;
  const std::string Rate;
  const std::string Pitch;
  const std::string Velocity;
  const std::string Channel;
  const std::string Init;
};

SCORE_LIB_BASE_EXPORT const StringConstants& StringConstant();

SCORE_LIB_BASE_EXPORT
QString toNumber(double v) noexcept;
QString toNumber(int v) noexcept;
}
