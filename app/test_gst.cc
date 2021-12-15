#include <gst/gst.h>
#include <gst/video/video-format.h>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

#define FPS 20

typedef struct _CustomData {
  GstElement *pipeline, *app_source;
  // GstElement *filesrc, *video_parse;
  GstElement *filesrc, *video_conv;
  // GstElement *video_queue, *video_sink;
  GstElement *video_sink;

  guint buffersize;

  guint64 num_frame;
  GstClockTime timestamp;

  cv::VideoCapture *cap;
  cv::Mat *frame;
  guint sourceid;

  GMainLoop *main_loop;
} CustomData;

static gboolean push_data(CustomData *data) {
  // GstBuffer *buffer;
  GstFlowReturn ret;
  GstMapInfo map;
  GstBuffer *buffer;

  cv::Mat frame;
  // cv::Mat frame_dst;
  guchar *img_data;

  *(data->cap) >> frame;
  cv::rectangle(frame, cv::Point(100, 100), cv::Point(150, 150),
                cv::Scalar(255, 0, 0));
  img_data = (guchar *)frame.data;

  /* Create a new empty buffer */

  // frame = cv::imread("./test.jpg");
  // img_data = (guchar*)data->frame->data;

  // frame = cv::imread("./test.jpg");
  // cv::resize(frame, frame_dst, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);
  // img_data = (guchar*)frame_dst.data;

  // std::cout
  //<< "[INFO][test_gst.cpp][push_data] Capture frame from stream." <<
  //std::endl;
  // std::cout << "frame cols: " << frame.cols
  //<< ", frame rows: " << frame.rows
  //<< ", frame channels: " << frame.channels()
  //<< ", frame elemSize: " << frame.elemSize()
  //<< ", frame elemSize1: " << frame.elemSize1()
  //<< std::endl;
  buffer = gst_buffer_new_and_alloc(data->buffersize);

  /* Set its timestamp and duration */
  GST_BUFFER_PTS(buffer) =
      gst_util_uint64_scale(data->num_frame, GST_SECOND, FPS);
  GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, FPS);
  data->num_frame++;
  // std::cout << "[INFO][test_gst][push_data] data->timestamp: " <<
  // data->timestamp << std::endl;

  /* Generate some psychodelic waveforms */
  gst_buffer_map(buffer, &map, GST_MAP_WRITE);
  memcpy((guchar *)map.data, img_data, data->buffersize);

  /* Push the buffer into the appsrc */
  g_signal_emit_by_name(data->app_source, "push-buffer", buffer, &ret);

  if (ret != GST_FLOW_OK) {
    /* We got some error, stop sending data */
    std::cout << "[ERROR][test_gst.cpp][push_data] Push data fail."
              << std::endl;
    return FALSE;
  }
  gst_buffer_unmap(buffer, &map);
  gst_buffer_unref(buffer);
  frame.release();

  return TRUE;
}

/* This signal callback triggers when appsrc needs data. Here, we add an idle
 * handler to the mainloop to start pushing data into the appsrc */
static void start_feed(GstElement *source, guint size, CustomData *data) {
  if (data->sourceid == 0) {
    // g_print ("Start feeding\n");
    data->sourceid = g_idle_add((GSourceFunc)push_data, data);
  }
}

/* This callback triggers when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void stop_feed(GstElement *source, CustomData *data) {
  if (data->sourceid != 0) {
    // g_print ("Stop feeding\n");
    g_source_remove(data->sourceid);
    data->sourceid = 0;
  }
}

/* This function is called when an error message is posted on the bus */
static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error(msg, &err, &debug_info);
  g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src),
             err->message);
  g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error(&err);
  g_free(debug_info);

  g_main_loop_quit(data->main_loop);
}

static gboolean print_field(GQuark field, const GValue *value, gpointer pfx) {
  gchar *str = gst_value_serialize(value);

  g_print("%s  %15s: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
  g_free(str);
  return TRUE;
}

static void print_caps(const GstCaps *caps, const gchar *pfx) {
  guint i;

  g_return_if_fail(caps != NULL);

  if (gst_caps_is_any(caps)) {
    g_print("%sANY\n", pfx);
    return;
  }
  if (gst_caps_is_empty(caps)) {
    g_print("%sEMPTY\n", pfx);
    return;
  }

  for (i = 0; i < gst_caps_get_size(caps); i++) {
    GstStructure *structure = gst_caps_get_structure(caps, i);

    g_print("%s%s\n", pfx, gst_structure_get_name(structure));
    gst_structure_foreach(structure, print_field, (gpointer)pfx);
  }
}

/* Shows the CURRENT capabilities of the requested pad in the given element */
static void print_pad_capabilities(GstElement *element, gchar *pad_name) {
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

  /* Retrieve pad */
  pad = gst_element_get_static_pad(element, pad_name);
  if (!pad) {
    g_printerr("Could not retrieve pad '%s'\n", pad_name);
    return;
  }

  /* Retrieve negotiated caps (or acceptable caps if negotiation is not finished
   * yet) */
  caps = gst_pad_get_current_caps(pad);
  if (!caps) caps = gst_pad_query_caps(pad, NULL);

  /* Print and free */
  g_print("Caps for the %s pad:\n", pad_name);
  print_caps(caps, "      ");
  gst_caps_unref(caps);
  gst_object_unref(pad);
}

int main(int argc, char *argv[]) {
  std::cout << "[INFO][test_gst.cpp] Application start." << std::endl;
  CustomData data;
  GstBus *bus;
  GstPad *src_pad;
  GstVideoFormatInfo info;
  cv::Mat frame;
  cv::Mat frame_dst;

  double frame_w, frame_h, frame_fps;

  /* Initialize cumstom data structure */
  memset(&data, 0, sizeof(data));
  // data.frame = &frame;
  // std::ifstream img_file;
  // std::cout << "[INFO][test_gst.cpp][main] Start reading yuv." <<std::endl;
  // img_file.open("./img.yuv", std::ios::in | std::ios::binary);
  // img_file.read(data.yuv_img, yuv_img_size);
  // std::cout << "[INFO][test_gst.cpp][main] Start reading yuv." <<std::endl;

  // std::cout << "[INFO][test_gst.cpp][main] Start reading jpg." <<std::endl;
  frame = cv::imread("./test.jpg");
  cv::resize(frame, frame_dst, cv::Size(640, 480), 0, 0, cv::INTER_LINEAR);
  data.frame = &frame_dst;
  // std::cout
  //<< "[INFO][test_gst.cpp][main] Finish reading." << std::endl;
  // std::cout << "frame cols: " << data.frame->cols
  //<< ", frame rows: " << data.frame->rows
  //<< ", frame channels: " << data.frame->channels()
  //<< ", frame elemSize: " << data.frame->elemSize()
  //<< ", frame elemSize1: " << data.frame->elemSize1()
  //<< std::endl;

  cv::VideoCapture cap("/dev/video2", cv::CAP_V4L2);
  if (!cap.isOpened()) {
    std::cout << "[ERROR][test_gst.cpp] Fail to open video." << std::endl;
    return -1;
  }
  frame_w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
  frame_h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
  frame_fps = cap.get(cv::CAP_PROP_FPS);
  std::cout << "[INFO][test_gst.cpp] camera info:" << std::endl;
  std::cout << "frame width: " << frame_w << std::endl;
  std::cout << "frame height: " << frame_h << std::endl;
  std::cout << "frame fps: " << frame_fps << std::endl;
  data.cap = &cap;

  data.buffersize = guint(640 * 480 * 3);

  // data.buffersize = guint(1920 * 1080 * 3 / 4);

  std::cout << "[DEBUG][test_gst.cpp] gst_init" << std::endl;
  /* Initialize GStreamer */
  gst_init(&argc, &argv);

  /* Create the elements */
  std::cout << "[DEBUG][test_gst.cpp] create element" << std::endl;
  data.app_source = gst_element_factory_make("appsrc", "app_source");
  // data.filesrc = gst_element_factory_make("filesrc", "filesrc");
  // data.video_queue = gst_element_factory_make("queue", "video_queue");
  data.video_conv = gst_element_factory_make("videoconvert", "conv");
  // data.video_parse = gst_element_factory_make("videoparse", "video_parse");
  // data.video_sink = gst_element_factory_make("fpsdisplaysink", "video_sink");
  // data.video_sink = gst_element_factory_make("autovideosink", "video_sink");
  data.video_sink = gst_element_factory_make("ximagesink", "video_sink");

  /* Configure element */
  std::cout << "[DEBUG][test_gst.cpp] configure element" << std::endl;
  GstCaps *caps = gst_caps_new_simple(
      "video/x-raw",
      //"format", G_TYPE_INT, GST_VIDEO_FORMAT_BGR,
      "format", G_TYPE_STRING, "BGR",
      //"format", G_TYPE_STRING, "YUY2",
      "framerate", GST_TYPE_FRACTION, FPS, 1,
      //"framerate", GST_TYPE_FRACTION, 1, 1,
      //"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
      //"width", G_TYPE_INT, 1920,
      //"height", G_TYPE_INT, 1080,
      "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480, NULL);
  // g_object_set(
  // G_OBJECT(data.app_source), "stream-type", 0,
  //"format", GST_FORMAT_TIME, "caps", caps, NULL);
  g_object_set(G_OBJECT(data.app_source), "caps", caps, NULL);

  // std::cout << "[DEBUG][test_gst.cpp] g_signal_connect" << std::endl;
  g_signal_connect(data.app_source, "need-data", G_CALLBACK(start_feed), &data);
  g_signal_connect(data.app_source, "enough-data", G_CALLBACK(stop_feed),
                   &data);

  // g_object_set(data.filesrc, "location", "./test.yuv", NULL);
  // g_object_set(data.video_parse,
  //"format", GST_VIDEO_FORMAT_BGR,
  //"width", 1920,
  //"height", 1080,
  // NULL);
  // g_object_set(data.video_parse,
  //"format", GST_VIDEO_FORMAT_YUY2,
  //"width", 1920,
  //"height", 1080,
  // NULL);

  // g_object_set(
  // G_OBJECT(data.video_queue),
  //"max-size-bytes", 1342177280);

  /* Create the empty pipeline */
  // std::cout << "[DEBUG][test_gst.cpp] gst_pipeline_new" << std::endl;
  data.pipeline = gst_pipeline_new("test-pipeline");

  // if (!data.pipeline || !data.app_source || ! data.video_parse ||
  // !data.video_queue if (!data.pipeline || !data.app_source || !
  // data.video_conv || !data.video_queue
  if (!data.pipeline || !data.app_source ||
      !data.video_conv
      // if (!data.pipeline || !data.app_source || !data.video_queue
      // if (!data.pipeline || !data.filesrc || ! data.video_parse ||
      // !data.video_queue
      || !data.video_sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  /* Link all elements because they have "Always" pads */
  // std::cout << "[DEBUG][test_gst.cpp] gst_bin_add_many" << std::endl;
  gst_bin_add_many(
      // GST_BIN (data.pipeline), data.app_source, data.video_queue,
      // data.video_parse, GST_BIN (data.pipeline), data.app_source,
      // data.video_queue, data.video_conv,
      GST_BIN(data.pipeline), data.app_source, data.video_conv,
      // GST_BIN (data.pipeline), data.app_source, data.video_queue,
      // GST_BIN (data.pipeline), data.filesrc, data.video_queue,
      // data.video_parse,
      data.video_sink, NULL);

  // std::cout << "[DEBUG][test_gst.cpp] gst_element_link_many" << std::endl;
  if (gst_element_link_many(
          // data.app_source, data.video_queue, data.video_parse,
          // data.app_source, data.video_queue, data.video_conv,
          data.app_source, data.video_conv,
          // data.app_source, data.video_queue,
          // data.filesrc, data.video_queue, data.video_parse,
          data.video_sink, NULL) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  g_object_set(G_OBJECT(data.app_source), "stream-type", 0, "format",
               GST_FORMAT_TIME, NULL);

  /* Get and set appsrc pad */
  // GstCaps *caps = gst_video_make_raw_caps(GST_VIDEO_FORMAT_BGR, 500);
  // const char* test = gst_video_format_to_string(GST_VIDEO_FORMAT_BGR);
  // std::cout << "[INFO][test_gst.cpp] video format: " << test << std::endl;
  // src_pad = gst_element_get_static_pad(data.app_source, "src");
  // if (!src_pad) {
  // std::cout << "[ERROR][test_gst.cpp] Fail to get pad of src" << std::endl;
  //}
  // if(!gst_pad_set_caps(src_pad, caps)) {
  // std::cout << "[ERROR][test_gst.cpp] gst_pad_set_caps failed." << std::endl;
  //}

  /* Print pad cap */
  // std::cout << "[DEBUG][test_gst.cpp] print_pad_capabilities app_source" <<
  // std::endl;
  print_pad_capabilities(data.app_source, "src");
  // std::cout << "[DEBUG][test_gst.cpp] print_pad_capabilities filesrc" <<
  // std::endl; print_pad_capabilities(data.filesrc, "src"); std::cout <<
  // "[DEBUG][test_gst.cpp] print_pad_capabilities video_queue" << std::endl;
  // print_pad_capabilities(data.video_queue, "sink");
  // print_pad_capabilities(data.video_queue, "src");
  // std::cout << "[DEBUG][test_gst.cpp] print_pad_capabilities video_parse" <<
  // std::endl; print_pad_capabilities(data.video_parse, "sink");
  // print_pad_capabilities(data.video_parse, "src");
  std::cout << "[DEBUG][test_gst.cpp] print_pad_capabilities video_conv"
            << std::endl;
  print_pad_capabilities(data.video_conv, "sink");
  print_pad_capabilities(data.video_conv, "src");
  std::cout << "[DEBUG][test_gst.cpp] print_pad_capabilities video_sink"
            << std::endl;
  print_pad_capabilities(data.video_sink, "sink");

  /* Instruct the bus to emit signals for each received message,
   * and connect to the interesting signals
   */
  bus = gst_element_get_bus(data.pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
  gst_object_unref(bus);

  /* Start playing the pipeline */
  gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  /* Create a GLib Main Loop and set it to run */
  data.main_loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.main_loop);

  // Start capture frame from video stream
  std::cout << "[DEBUG][test_gst] Read from VideoCapture." << std::endl;
  // while (true) {
  // cap >> frame;
  // data.frame = &frame;
  //}

  /* Free resources */
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}
