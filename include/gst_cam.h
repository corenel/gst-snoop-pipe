#pragma once

extern "C" {
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
}

#include <stdexcept>

namespace gstcam {

class GstCam {
 public:
  GstCam(std::string gst_config);
  ~GstCam();

 public:
  bool Init();
  bool InitStream();
  void PublishStream();
  void CleanupStream();

  void Run();

 private:
  // General gstreamer configuration
  std::string gsconfig_;

  // Gstreamer structures
  GstElement *pipeline_;
  GstElement *sink_;

  // Appsink configuration
  bool sync_sink_;
  bool preroll_;
  bool reopen_on_eof_;
  bool use_gst_timestamps_;

  // camera configuration
  std::string frame_id_;
  int width_, height_;
  std::string image_encoding_;

  // other
  bool running_;
};

}  // namespace gstcam
