#pragma once

#include <Process/TimeValue.hpp>
#include <QObject>
#include <score/selection/Selectable.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/Metadata.hpp>
#include <score/model/Identifier.hpp>

class DataStream;
class JSONObject;
class QTextDocument;

namespace Scenario
{
class CommentBlockModel final : public IdentifiedObject<CommentBlockModel>
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS

public:
  Selectable selection;

  CommentBlockModel(
      const Id<CommentBlockModel>& id,
      const TimeVal& date,
      double yPos,
      QObject* parent);

  template <typename DeserializerVisitor>
  CommentBlockModel(DeserializerVisitor&& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }

  void setDate(const TimeVal& date);
  const TimeVal& date() const;

  double heightPercentage() const;
  void setHeightPercentage(double y);

  const QString content() const;
  void setContent(const QString content);

Q_SIGNALS:
  void dateChanged(const TimeVal&);
  void heightPercentageChanged(bool);
  void contentChanged(QString);

private:
  TimeVal m_date{std::chrono::seconds{0}};
  double m_yposition{0};

  QString m_HTMLcontent{
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" "
      "\"http://www.w3.org/TR/REC-html40/strict.dtd\">\n<html><head><meta "
      "name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\np, li { "
      "white-space: pre-wrap; }\n</style></head><body style=\" "
      "font-family:'Ubuntu'; font-size:10pt; font-weight:400; "
      "font-style:normal;\">\n<p style=\" margin-top:0px; margin-bottom:0px; "
      "margin-left:0px; margin-right:0px; -qt-block-indent:0; "
      "text-indent:0px;\">New Comment</p></body></html>"};
};
}

DEFAULT_MODEL_METADATA(Scenario::CommentBlockModel, "Comment Block")
