#include "Bw64AdmReader.hpp"

#if defined(SCORE_HAS_BW64_ADM)
#include <adm/adm.hpp>
#include <adm/parse.hpp>
#include <bw64/bw64.hpp>

#include <QDebug>

#include <sstream>

namespace Media::BW64
{

bool Bw64AdmReader::isBw64WithAdm(const QString& path)
{
  try
  {
    auto reader = bw64::readFile(path.toStdString());
    return reader->axmlChunk() != nullptr && reader->chnaChunk() != nullptr;
  }
  catch(...)
  {
    return false;
  }
}

// Helper to extract time in seconds from ADM time types
static double admTimeToSeconds(const adm::Time& time)
{
  auto ns = time.asNanoseconds();
  return static_cast<double>(ns.get()) / 1e9;
}

// Helper to get duration in seconds
static double admDurationToSeconds(const adm::Duration& dur)
{
  auto ns = dur.asNanoseconds();
  return static_cast<double>(ns.get()) / 1e9;
}

std::optional<Bw64AdmData> Bw64AdmReader::parse(const QString& path)
{
  try
  {
    // Open BW64 file
    auto reader = bw64::readFile(path.toStdString());

    auto axmlChunk = reader->axmlChunk();
    auto chnaChunk = reader->chnaChunk();

    if(!axmlChunk || !chnaChunk)
    {
      qDebug() << "BW64 file missing axml or chna chunk:" << path;
      return std::nullopt;
    }

    // Parse ADM XML
    std::istringstream xmlStream(axmlChunk->data());
    auto document = adm::parseXml(xmlStream);

    if(!document)
    {
      qDebug() << "Failed to parse ADM XML in:" << path;
      return std::nullopt;
    }

    // Calculate total duration from audio data
    const double totalDurationSec
        = static_cast<double>(reader->numberOfFrames()) / reader->sampleRate();

    Bw64AdmData result;
    result.filePath = path;
    result.sampleRate = reader->sampleRate();
    result.totalChannels = reader->channels();
    result.duration = TimeVal::fromMsecs(totalDurationSec * 1000.0);

    // Build a map from AudioTrackUID to channel index using chna chunk
    std::map<std::string, int> trackUidToChannel;
    for(const auto& audioId : chnaChunk->audioIds())
    {
      // trackIndex is 1-based in BW64, convert to 0-based
      std::string uid = audioId.uid();
      // Trim whitespace
      uid.erase(uid.find_last_not_of(' ') + 1);
      trackUidToChannel[uid] = audioId.trackIndex() - 1;
    }

    // Process each AudioObject in the document
    for(const auto& audioObject : document->getElements<adm::AudioObject>())
    {
      AdmAudioObject obj;
      obj.name = QString::fromStdString(
          audioObject->get<adm::AudioObjectName>().get());

      // Get the AudioTrackUIDs referenced by this object
      auto trackUids = audioObject->getReferences<adm::AudioTrackUid>();
      if(trackUids.empty())
        continue;

      // Use the first track UID to determine the channel
      auto firstTrackUid = trackUids.front();
      std::string uidStr = adm::formatId(firstTrackUid->get<adm::AudioTrackUidId>());
      auto channelIt = trackUidToChannel.find(uidStr);
      if(channelIt != trackUidToChannel.end())
      {
        obj.audioChannelIndex = channelIt->second;
      }
      else
      {
        // Try to find by iterating
        for(const auto& [uid, ch] : trackUidToChannel)
        {
          if(uid.find(uidStr) != std::string::npos
             || uidStr.find(uid) != std::string::npos)
          {
            obj.audioChannelIndex = ch;
            break;
          }
        }
      }

      // Get the AudioChannelFormat to access AudioBlockFormats
      auto packFormats = audioObject->getReferences<adm::AudioPackFormat>();
      for(const auto& packFormat : packFormats)
      {
        auto channelFormats = packFormat->getReferences<adm::AudioChannelFormat>();
        for(const auto& channelFormat : channelFormats)
        {
          // Check if this is an Objects type (has position automation)
          auto typeDescriptor = channelFormat->get<adm::TypeDescriptor>();
          if(typeDescriptor != adm::TypeDefinition::OBJECTS)
            continue;

          // Get all AudioBlockFormatObjects
          auto blockFormats
              = channelFormat->getElements<adm::AudioBlockFormatObjects>();

          if(blockFormats.empty())
            continue;

          // Prepare automation containers
          std::vector<AdmAutomationPoint> xPoints, yPoints, zPoints;
          std::vector<AdmAutomationPoint> gainPoints;
          std::vector<AdmAutomationPoint> widthPoints, heightPoints, depthPoints;
          std::vector<AdmAutomationPoint> diffusePoints;

          for(const auto& block : blockFormats)
          {
            // Get timing
            double startTime = 0.0;
            if(block.has<adm::Rtime>())
            {
              startTime = admTimeToSeconds(block.get<adm::Rtime>().get());
            }

            // Normalize time to 0-1
            double normalizedTime
                = (totalDurationSec > 0) ? (startTime / totalDurationSec) : 0.0;
            normalizedTime = std::clamp(normalizedTime, 0.0, 1.0);

            // Extract position
            double x = 0, y = 0, z = 0;

            if(block.has<adm::CartesianPosition>())
            {
              auto pos = block.get<adm::CartesianPosition>();
              x = pos.get<adm::X>().get();
              y = pos.get<adm::Y>().get();
              z = pos.has<adm::Z>() ? pos.get<adm::Z>().get() : 0.0;
            }
            else if(block.has<adm::SphericalPosition>())
            {
              auto pos = block.get<adm::SphericalPosition>();
              double azimuth = pos.get<adm::Azimuth>().get();
              double elevation = pos.get<adm::Elevation>().get();
              double distance = pos.has<adm::Distance>() ? pos.get<adm::Distance>().get() : 1.0;
              sphericalToCartesian(azimuth, elevation, distance, x, y, z);
            }

            xPoints.push_back({normalizedTime, x});
            yPoints.push_back({normalizedTime, y});
            zPoints.push_back({normalizedTime, z});

            // Extract gain
            if(block.has<adm::Gain>())
            {
              auto gain = block.get<adm::Gain>();
              double gainValue = gain.isLinear()
                  ? gain.asLinear()
                  : std::pow(10.0, gain.asDb() / 20.0);
              gainPoints.push_back({normalizedTime, gainValue});
            }

            // Extract width/height/depth (extent)
            if(!block.isDefault<adm::Width>())
            {
              widthPoints.push_back({normalizedTime, block.get<adm::Width>().get()});
            }
            if(!block.isDefault<adm::Height>())
            {
              heightPoints.push_back({normalizedTime, block.get<adm::Height>().get()});
            }
            if(!block.isDefault<adm::Depth>())
            {
              depthPoints.push_back({normalizedTime, block.get<adm::Depth>().get()});
            }

            // Extract diffuse
            if(!block.isDefault<adm::Diffuse>())
            {
              diffusePoints.push_back(
                  {normalizedTime, block.get<adm::Diffuse>().get()});
            }
          }

          // Create automation objects for parameters that have data
          auto addAutomation = [&](AdmAutomation::Parameter param,
                                   const QString& suffix,
                                   std::vector<AdmAutomationPoint>& points,
                                   double minVal, double maxVal) {
            if(points.size() >= 2)
            {
              AdmAutomation autom;
              autom.param = param;
              autom.name = obj.name + " " + suffix;
              autom.minValue = minVal;
              autom.maxValue = maxVal;
              autom.points = std::move(points);
              obj.automations.push_back(std::move(autom));
            }
          };

          addAutomation(AdmAutomation::X, "X", xPoints, -1.0, 1.0);
          addAutomation(AdmAutomation::Y, "Y", yPoints, -1.0, 1.0);
          addAutomation(AdmAutomation::Z, "Z", zPoints, -1.0, 1.0);
          addAutomation(AdmAutomation::Gain, "Gain", gainPoints, 0.0, 1.0);
          addAutomation(AdmAutomation::Width, "Width", widthPoints, 0.0, 360.0);
          addAutomation(AdmAutomation::Height, "Height", heightPoints, 0.0, 360.0);
          addAutomation(AdmAutomation::Depth, "Depth", depthPoints, 0.0, 360.0);
          addAutomation(AdmAutomation::Diffuse, "Diffuse", diffusePoints, 0.0, 1.0);
        }
      }

      // Only add objects that have automations or valid channel mapping
      if(!obj.automations.empty() || obj.audioChannelIndex >= 0)
      {
        result.objects.push_back(std::move(obj));
      }
    }

    if(result.objects.empty())
    {
      qDebug() << "No ADM audio objects found in:" << path;
      return std::nullopt;
    }

    return result;
  }
  catch(const std::exception& e)
  {
    qDebug() << "Error parsing BW64/ADM file:" << path << "-" << e.what();
    return std::nullopt;
  }
  catch(...)
  {
    qDebug() << "Unknown error parsing BW64/ADM file:" << path;
    return std::nullopt;
  }
}

}
#endif // SCORE_HAS_BW64_ADM
