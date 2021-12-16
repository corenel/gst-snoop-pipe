#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include "gst_snoop_pipe.h"
#include "opencv2/opencv.hpp"

int main(int argc, char *argv[]) {
  // init gstreamer
  gst_init(&argc, &argv);

  // init snoop pipe
  std::string pipeline_src =
      "videotestsrc num-buffers=600 ! "
      "video/x-raw,width=(int)640,height=(int)480,"
      "format=(string)RGB,framerate=(fraction)30/1 ! queue ! appsink "
      "name=testsink";
  std::string pipeline_sink =
      "appsrc name=testsource caps=video/x-raw,width=(int)640,height=(int)480,"
      "format=(string)RGB,framerate=(fraction)30/1 ! queue ! videoconvert ! "
      "fpsdisplaysink";
  GstSnoopPipe pipe(pipeline_src, pipeline_sink);
  pipe.Init();

  // register callback
  // pipe.RegisterCallback([](GstMapInfo *map) {
  //   g_print("%lu\n", map->size);
  // });
  pipe.RegisterCallback([](GstMapInfo *map) {
    // on screen display
    cv::Mat frame(480, 640, CV_8UC3, (char *)map->data, cv::Mat::AUTO_STEP);
    cv::line(frame, cv::Point(0, 240), cv::Point(640, 240),
             cv::Scalar(0, 255, 0), 2);
    cv::putText(frame, "Hello World", cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

    // do canny to simulate a time consuming operation
    cv::Mat src_gray, detected_edges, dst;
    cv::cvtColor(frame, src_gray, cv::COLOR_BGR2GRAY);
    cv::blur(src_gray, detected_edges, cv::Size(3, 3));
    cv::Canny(detected_edges, detected_edges, 0, 0, 3,
              true);  // true: use L2 norm
  });

  // start pipeline
  pipe.Start();
  pipe.Stop();

  return 0;
}