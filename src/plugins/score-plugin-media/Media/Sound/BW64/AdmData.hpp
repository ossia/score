#pragma once
#include <Process/TimeValue.hpp>

#include <QString>

#include <vector>

namespace Media::BW64
{

struct AdmAutomationPoint
{
  double time;  // normalized 0-1
  double value;
};

struct AdmAutomation
{
  enum Parameter
  {
    X,
    Y,
    Z,
    Gain,
    Width,
    Height,
    Depth,
    Diffuse
  };

  Parameter param{X};
  QString name;
  double minValue{-1.0};
  double maxValue{1.0};
  std::vector<AdmAutomationPoint> points;
};

struct AdmAudioObject
{
  QString name;
  int audioChannelIndex{0}; // Channel index in the WAV file (0-based)
  std::vector<AdmAutomation> automations;
};

struct Bw64AdmData
{
  QString filePath;
  TimeVal duration;
  int sampleRate{48000};
  int totalChannels{0};
  std::vector<AdmAudioObject> objects;

  bool isValid() const noexcept { return !objects.empty() && duration > TimeVal::zero(); }
};

// Convert ADM spherical coordinates (azimuth, elevation, distance) to cartesian (x, y, z)
// ADM convention:
//   azimuth: 0° = front, +90° = left, -90° = right (degrees)
//   elevation: 0° = horizon, +90° = above, -90° = below (degrees)
//   distance: 0 to 1 (normalized)
inline void
sphericalToCartesian(double azimuth, double elevation, double distance, double& x, double& y, double& z)
{
  constexpr double deg_to_rad = 3.14159265358979323846 / 180.0;
  const double az_rad = azimuth * deg_to_rad;
  const double el_rad = elevation * deg_to_rad;

  // ADM/ossia convention: +Y is front, +X is left, +Z is up
  x = std::sin(az_rad) * std::cos(el_rad) * distance;
  y = std::cos(az_rad) * std::cos(el_rad) * distance;
  z = std::sin(el_rad) * distance;
}

}
