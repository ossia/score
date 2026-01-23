#include "Bw64Drop.hpp"

#if defined(SCORE_HAS_BW64_ADM)
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/BW64/Bw64AdmReader.hpp>
#include <Media/Sound/SoundModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <score/tools/File.hpp>

#include <QFileInfo>

namespace Media::BW64
{

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  // We handle .wav files that have ADM metadata
  return {"wav", "bwf"};
}

// Convert AdmAutomation points to curve segments
static std::vector<Curve::SegmentData>
automationToCurveSegments(const AdmAutomation& autom)
{
  std::vector<Curve::SegmentData> segments;

  if(autom.points.size() < 2)
    return segments;

  const double range = autom.maxValue - autom.minValue;
  if(range <= 0)
    return segments;

  // Filter and deduplicate points
  std::vector<std::pair<double, double>> filteredPoints;
  filteredPoints.reserve(autom.points.size());

  for(const auto& pt : autom.points)
  {
    double x = std::clamp(pt.time, 0.0, 1.0);
    double y = std::clamp((pt.value - autom.minValue) / range, 0.0, 1.0);

    // Skip if same x as previous point (keep the last value at each time)
    if(!filteredPoints.empty() && filteredPoints.back().first >= x)
    {
      filteredPoints.back().second = y;
    }
    else
    {
      filteredPoints.emplace_back(x, y);
    }
  }

  if(filteredPoints.size() < 2)
    return segments;

  // Create IDs for all segments
  std::vector<Id<Curve::SegmentModel>> ids;
  ids.reserve(filteredPoints.size() - 1);

  for(std::size_t i = 0; i < filteredPoints.size() - 1; i++)
  {
    ids.push_back(Curve::getSegmentId(ids));
  }

  // Create segments
  for(std::size_t i = 0; i < filteredPoints.size() - 1; i++)
  {
    const auto& pt1 = filteredPoints[i];
    const auto& pt2 = filteredPoints[i + 1];

    Curve::SegmentData seg;
    seg.id = ids[i];
    seg.start = Curve::Point{pt1.first, pt1.second};
    seg.end = Curve::Point{pt2.first, pt2.second};

    // Link segments
    if(i > 0)
      seg.previous = ids[i - 1];
    if(i < filteredPoints.size() - 2)
      seg.following = ids[i + 1];

    // Use linear segments
    seg.type = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    seg.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});

    segments.push_back(std::move(seg));
  }

  return segments;
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  // First check if this is a BW64 file with ADM metadata
  if(!Bw64AdmReader::isBw64WithAdm(filename.absolute))
  {
    // Not a BW64/ADM file, let the regular sound handler deal with it
    return;
  }

  // Parse the ADM data
  auto admData = Bw64AdmReader::parse(filename.absolute);
  if(!admData || !admData->isValid())
  {
    return;
  }

  // Create processes for each ADM audio object
  for(const auto& obj : admData->objects)
  {
    // Create Sound process for this object's audio channel
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get();
      p.creation.prettyName = obj.name;
      p.duration = admData->duration;

      const int channelIndex = obj.audioChannelIndex;
      p.setup = [f = score::relativizeFilePath(filename.absolute, ctx), channelIndex,
                 &ctx](Process::ProcessModel& m, score::Dispatcher& disp) {
        auto& proc = static_cast<Media::Sound::ProcessModel&>(m);
        // Set the audio file with specific channel/stream
        disp.submit(new Media::ChangeAudioFile{proc, std::move(f), ctx});
        // Note: For mono extraction from multichannel, we'd need additional
        // support in the Sound process. For now, this loads the full file.
      };
      vec.push_back(std::move(p));
    }

    // Create Automation processes for each parameter
    for(const auto& autom : obj.automations)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Automation::ProcessModel>::get();
      p.creation.prettyName = autom.name;
      p.duration = admData->duration;

      p.setup = [autom](Process::ProcessModel& m, score::Dispatcher& disp) {
        auto& automProc = static_cast<Automation::ProcessModel&>(m);

        // Set min/max values
        disp.submit(new Automation::SetMin{automProc, autom.minValue});
        disp.submit(new Automation::SetMax{automProc, autom.maxValue});

        // Convert automation points to curve segments
        auto segments = automationToCurveSegments(autom);
        if(!segments.empty())
        {
          disp.submit(
              new Curve::UpdateCurve{automProc.curve(), std::move(segments)});
        }
      };
      vec.push_back(std::move(p));
    }
  }
}

}
#endif // SCORE_HAS_BW64_ADM
