#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include <functional>
#include <iostream>
#include <string>

#include "glibconfig.h"
#include "gst/gstpad.h"

class GstSnoopPipe {
 public:
  GstSnoopPipe(std::string pipeline_src, std::string pipeline_sink);
  ~GstSnoopPipe();

 public:
  bool Init();
  void Start();
  void Stop();
  void RegisterCallback(std::function<void(GstMapInfo *)> callback);

 private:
  /**
   * @brief user modify the data here
   */
  GstFlowReturn ModifyInData(GstMapInfo *map);

  /**
   * @brief called when the appsink notifies us that there is a new buffer ready
   * for processing
   */
  static GstFlowReturn OnNewSampleFromSink(GstElement *elt,
                                           GstSnoopPipe *user_data);

  /**
   * @brief called when we get a GstMessage from the source pipeline when we
   * get EOS, we notify the appsrc of it.
   */
  static gboolean OnSourceMessage(GstBus *bus, GstMessage *message,
                                  GstSnoopPipe *user_data);

  /**
   * @brief called when we get a GstMessage from the sink pipeline when we get
   * EOS, we exit the mainloop and this testapp.
   */
  static gboolean OnSinkMessage(GstBus *bus, GstMessage *message,
                                GstSnoopPipe *user_data);

 private:
  GMainLoop *loop_;
  GstElement *src_;
  GstElement *sink_;
  std::function<void(GstMapInfo *)> callback_;

  std::string pipeline_src_;
  std::string pipeline_sink_;
};