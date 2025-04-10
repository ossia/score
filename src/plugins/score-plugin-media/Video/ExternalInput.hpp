#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Video/VideoInterface.hpp>

#include <score_plugin_media_export.h>

namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT ExternalInput : public VideoInterface
{
public:
  virtual ~ExternalInput();
  virtual bool start() noexcept = 0;
  virtual void stop() noexcept = 0;
};
}
#endif
