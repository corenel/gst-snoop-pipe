#include "gst_cam.h"

#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#include <iostream>
#include <utility>
extern "C" {
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
}

namespace gstcam {

GstCam::GstCam(std::string gst_config)
    : gsconfig_(std::move(gst_config)),
      pipeline_(NULL),
      sink_(NULL),
      sync_sink_(true),
      preroll_(false),
      reopen_on_eof_(false),
      use_gst_timestamps_(false),
      image_encoding_("RGB8") {}

GstCam::~GstCam() {}

bool GstCam::Init() { return true; }

bool GstCam::InitStream() {
  if (!gst_is_initialized()) {
    // Initialize gstreamer pipeline
    std::cout << "Initializing gstreamer..." << std::endl;
    gst_init(0, 0);
  }

  std::cout << "Gstreamer Version: " << gst_version_string() << std::endl;

  GError *error = 0;  // Assignment to zero is a gst requirement

  pipeline_ = gst_parse_launch(gsconfig_.c_str(), &error);
  if (pipeline_ == NULL) {
    std::cerr << error->message << std::endl;
    return false;
  }

  // Create RGB sink
  sink_ = gst_element_factory_make("appsink", NULL);
  GstCaps *caps = gst_app_sink_get_caps(GST_APP_SINK(sink_));

#if (GST_VERSION_MAJOR == 1)
  // http://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/html/section-types-definitions.html
  if (image_encoding_ == "RGB8") {
    caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB",
                               NULL);
  } else if (image_encoding_ == "MONO8") {
    caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "GRAY8",
                               NULL);
  } else if (image_encoding_ == "jpeg") {
    caps = gst_caps_new_simple("image/jpeg", NULL, NULL);
  }
#else
  if (image_encoding_ == "RGB8") {
    caps = gst_caps_new_simple("video/x-raw-rgb", NULL, NULL);
  } else if (image_encoding_ == "MONO8") {
    caps = gst_caps_new_simple("video/x-raw-gray", NULL, NULL);
  } else if (image_encoding_ == "jpeg") {
    caps = gst_caps_new_simple("image/jpeg", NULL, NULL);
  }
#endif

  gst_app_sink_set_caps(GST_APP_SINK(sink_), caps);
  gst_caps_unref(caps);

  // Set whether the sink should sync
  // Sometimes setting this to true can cause a large number of frames to be
  // dropped
  gst_base_sink_set_sync(GST_BASE_SINK(sink_), (sync_sink_) ? TRUE : FALSE);

  if (GST_IS_PIPELINE(pipeline_)) {
    GstPad *outpad = gst_bin_find_unlinked_pad(GST_BIN(pipeline_), GST_PAD_SRC);
    g_assert(outpad);

    GstElement *outelement = gst_pad_get_parent_element(outpad);
    g_assert(outelement);
    gst_object_unref(outpad);

    if (!gst_bin_add(GST_BIN(pipeline_), sink_)) {
      std::cerr << "gst_bin_add() failed" << std::endl;
      gst_object_unref(outelement);
      gst_object_unref(pipeline_);
      return false;
    }

    if (!gst_element_link(outelement, sink_)) {
      std::cerr << "GStreamer: cannot link outelement(\""
                << gst_element_get_name(outelement) << "\") -> sink\n"
                << std::endl;
      gst_object_unref(outelement);
      gst_object_unref(pipeline_);
      return false;
    }

    gst_object_unref(outelement);
  } else {
    GstElement *launchpipe = pipeline_;
    pipeline_ = gst_pipeline_new(NULL);
    g_assert(pipeline_);

    gst_object_unparent(GST_OBJECT(launchpipe));

    gst_bin_add_many(GST_BIN(pipeline_), launchpipe, sink_, NULL);

    if (!gst_element_link(launchpipe, sink_)) {
      std::cerr << "GStreamer: cannot link launchpipe -> sink" << std::endl;
      gst_object_unref(pipeline_);
      return false;
    }
  }

  gst_element_set_state(pipeline_, GST_STATE_PAUSED);

  if (gst_element_get_state(pipeline_, NULL, NULL, -1) ==
      GST_STATE_CHANGE_FAILURE) {
    std::cerr << "Failed to PAUSE stream, check your gstreamer configuration."
              << std::endl;
    return false;
  } else {
    std::cout << "Stream is PAUSED." << std::endl;
  }

  return true;
}

void GstCam::PublishStream() {
  std::cout << "Publishing stream..." << std::endl;

  // Pre-roll camera if needed
  if (preroll_) {
    std::cout << "Performing preroll..." << std::endl;

    // The PAUSE, PLAY, PAUSE, PLAY cycle is to ensure proper pre-roll
    // I am told this is needed and am erring on the side of caution.
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (gst_element_get_state(pipeline_, NULL, NULL, -1) ==
        GST_STATE_CHANGE_FAILURE) {
      std::cerr << "Failed to PLAY during preroll." << std::endl;
      return;
    } else {
      std::cerr << "Stream is PLAYING in preroll." << std::endl;
    }

    gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    if (gst_element_get_state(pipeline_, NULL, NULL, -1) ==
        GST_STATE_CHANGE_FAILURE) {
      std::cerr << "Failed to PAUSE." << std::endl;
      return;
    } else {
      std::cout << "Stream is PAUSED in preroll." << std::endl;
    }
  }

  if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    std::cout << "Could not start stream!" << std::endl;
    return;
  }
  std::cout << "Started stream." << std::endl;

  // Poll the data as fast a spossible
  running_ = true;
  while (running_) {
    // This should block until a new frame is awake, this way, we'll run at the
    // actual capture framerate of the device.
    // ROS_DEBUG("Getting data...");
#if (GST_VERSION_MAJOR == 1)
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink_));
    if (!sample) {
      std::cerr << "Could not get gstreamer sample." << std::endl;
      break;
    }
    GstBuffer *buf = gst_sample_get_buffer(sample);
    GstMemory *memory = gst_buffer_get_memory(buf, 0);
    GstMapInfo info;

    gst_memory_map(memory, &info, GST_MAP_READ);
    gsize &buf_size = info.size;
    guint8 *&buf_data = info.data;
#else
    GstBuffer *buf = gst_app_sink_pull_buffer(GST_APP_SINK(sink_));
    guint &buf_size = buf->size;
    guint8 *&buf_data = buf->data;
#endif
    GstClockTime bt = gst_element_get_base_time(pipeline_);
    // ROS_INFO("New buffer: timestamp %.6f %lu %lu %.3f",
    //         GST_TIME_AS_USECONDS(buf->timestamp+bt)/1e6+time_offset_,
    //         buf->timestamp, bt, time_offset_);

#if 0
      GstFormat fmt = GST_FORMAT_TIME;
      gint64 current = -1;

       Query the current position of the stream
      if (gst_element_query_position(pipeline_, &fmt, &current)) {
          std::cout << "Position "<< current << std::endl;
      }
#endif

    // Stop on end of stream
    if (!buf) {
      std::cout << "Stream ended." << std::endl;
      break;
    }

    // std::cerr << "Got data." << std::endl;

    // Get the image width and height
    GstPad *pad = gst_element_get_static_pad(sink_, "sink");
#if (GST_VERSION_MAJOR == 1)
    const GstCaps *caps = gst_pad_get_current_caps(pad);
#else
    const GstCaps *caps = gst_pad_get_negotiated_caps(pad);
#endif
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width_);
    gst_structure_get_int(structure, "height", &height_);

    // ROS_INFO("Image time stamp: %.3f",cinfo->header.stamp.toSec());
    if (image_encoding_ == "jpeg") {
      // img->data.resize(buf_size);
      // std::copy(buf_data, (buf_data) + (buf_size), img->data.begin());
    } else {
      // Complain if the returned buffer is smaller than we expect
      const unsigned int expected_frame_size =
          image_encoding_ == "RGB8" ? width_ * height_ * 3 : width_ * height_;
      if (buf_size < expected_frame_size) {
        std::cerr << "GStreamer image buffer underflow: Expected frame to be "
                  << expected_frame_size << " bytes but got only " << (buf_size)
                  << " bytes. (make sure frames are correctly encoded)"
                  << std::endl;
      }

      // Copy only the data we received
      // Since we're publishing shared pointers, we need to copy the image so
      // we can free the buffer allocated by gstreamer

      // img->data.resize(expected_frame_size);
      // if (image_encoding_ == "RGB8") {
      //   img->step = width_ * 3;
      // } else {
      //   img->step = width_;
      // }
      // std::copy(buf_data, (buf_data) + (buf_size), img->data.begin());
    }

    // Release the buffer
    if (buf) {
#if (GST_VERSION_MAJOR == 1)
      gst_memory_unmap(memory, &info);
      gst_memory_unref(memory);
#endif
      gst_buffer_unref(buf);
    }
  }
}

void GstCam::CleanupStream() {
  // Clean up
  std::cout << "Stopping gstreamer pipeline..." << std::endl;
  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = NULL;
  }
}

void GstCam::Run() {
  while (true) {
    if (!this->Init()) {
      std::cerr << "Failed to configure gscam!" << std::endl;
      break;
    }

    if (!this->InitStream()) {
      std::cerr << "Failed to initialize gscam stream!" << std::endl;
      break;
    }

    // Block while publishing
    this->PublishStream();

    this->CleanupStream();

    std::cout << "GStreamer stream stopped!" << std::endl;

    if (reopen_on_eof_) {
      std::cout << "Reopening stream..." << std::endl;
    } else {
      std::cout << "Cleaning up stream and exiting..." << std::endl;
      break;
    }
  }
}

}  // namespace gstcam