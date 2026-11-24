#pragma once
#include <QGraphicsObject>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>

#include <score_lib_process_export.h>

#include <verdigris>

#include <cstdint>
#include <optional>
#include <vector>

class QPainter;
class QRectF;
namespace Process
{
/**
 * What a recording has recorded so far, for the views of the box it records
 * into to show while it records. It is no part of the document: the take
 * becomes processes when the recording ends, in one undoable step.
 *
 * GUI thread only: the recording updates it and emits changed().
 */
class SCORE_LIB_PROCESS_EXPORT LiveTake final : public QObject
{
  W_OBJECT(LiveTake)
public:
  enum Kind
  {
    Audio,
    Midi,
    Values,
    Video
  };

  LiveTake(QObject* interval, Kind kind, QString name);
  ~LiveTake() override;

  //! The box it records into
  QPointer<QObject> interval;
  Kind kind{};
  QString name;
  //! Where it will be in the node view, if it was asked for
  std::optional<QPointF> nodePosition;
  //! For a take placed in a new box of a scenario of `interval`: where that
  //! box goes, its height in the scenario between 0 and 1
  std::optional<double> scenarioY;
  //! It becomes a new process (not merged into one), which the node view shows
  bool newProcess{true};

  //! Recording, or waiting for its start
  bool recording{};
  //! Waiting while the count-in clicks
  bool countingIn{};
  //! Dates in the box, in flicks
  int64_t start{};
  int64_t end{};

  //! Audio: the loudest sample of each `peakPeriod` flicks from `start`, 0 to 1
  std::vector<float> peaks;
  int64_t peakPeriod{1};

  struct Note
  {
    int64_t start{};
    //! -1 while held
    int64_t end{-1};
    uint8_t pitch{};
    uint8_t velocity{};
  };
  std::vector<Note> notes;

  struct Point
  {
    int64_t date{};
    double value{};
  };
  std::vector<Point> values;

  int64_t frames{};

  void changed() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, changed)

  //! Draws what was recorded between box dates `from` and `to` in `rect`
  void paintContent(QPainter& p, const QRectF& rect, int64_t from, int64_t to) const;
  //! "Recording 0:03", "Counting in" or "Waiting"
  QString status() const;
};

}
W_REGISTER_ARGTYPE(Process::LiveTake*)
namespace Process
{
/**
 * The takes being recorded, of every document; views of a box show those
 * recorded into it.
 */
class SCORE_LIB_PROCESS_EXPORT LiveTakes final : public QObject
{
  W_OBJECT(LiveTakes)
public:
  static LiveTakes& instance();

  void add(LiveTake* t);
  void remove(LiveTake* t);
  const std::vector<LiveTake*>& takes() const noexcept { return m_takes; }

  void added(LiveTake* t) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, added, t)
  void removed(LiveTake* t) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, removed, t)

private:
  std::vector<LiveTake*> m_takes;
};

class ProcessModel;
/**
 * The processes armed for recording: what they play is kept while playback
 * runs, for the Record button and for Capture. UI state of the session: not
 * in the document, not undoable.
 */
class SCORE_LIB_PROCESS_EXPORT RecordArm final : public QObject
{
  W_OBJECT(RecordArm)
public:
  static RecordArm& instance();

  //! Set by the recording plug-in: the views show the arm toggles
  bool enabled{};

  bool armed(const ProcessModel& p) const noexcept;
  void setArmed(const ProcessModel& p, bool b);
  //! The armed processes of a document (those that still exist)
  std::vector<const ProcessModel*> armedIn(const QObject& document) const;
  //! The processes a toggle is shown for: those with an output to record
  static bool canArm(const ProcessModel& p) noexcept;

  void armedChanged(const ProcessModel* p, bool b)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, armedChanged, p, b)

private:
  std::vector<QPointer<const ProcessModel>> m_armed;
};

/**
 * A take being recorded, as a node of the node view of its box: where it will
 * be placed, with what it recorded so far.
 */
class SCORE_LIB_PROCESS_EXPORT LiveTakeNode final : public QGraphicsObject
{
public:
  static constexpr double width = 300.;
  static constexpr double height = 120.;
  LiveTakeNode(LiveTake& take, QGraphicsItem* parent);
  QRectF boundingRect() const override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  LiveTake* take() const noexcept { return m_take; }

private:
  QPointer<LiveTake> m_take;
};

/**
 * A take being recorded, over the dates of its box it covers, in the
 * timeline.
 */
class SCORE_LIB_PROCESS_EXPORT LiveTakeSpan final : public QGraphicsObject
{
public:
  LiveTakeSpan(LiveTake& take, QGraphicsItem* parent);
  //! Flicks per pixel, and the height of the box's rack
  void setGeometry(double zoom, double top, double height);
  QRectF boundingRect() const override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;
  LiveTake* take() const noexcept { return m_take; }

private:
  void layout();
  QPointer<LiveTake> m_take;
  double m_zoom{1.};
  double m_top{};
  double m_height{40.};
  QRectF m_rect;
};
}
