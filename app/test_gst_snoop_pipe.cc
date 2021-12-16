#include "gst_snoop_pipe.h"

int main(int argc, char *argv[]) {
  gst_init(&argc, &argv);

  std::string pipeline_src =
      "videotestsrc num-buffers=600 ! "
      "video/x-raw,width=(int)640,height=(int)480,"
      "format=(string)RGB,framerate=(fraction)30/1 ! appsink name=testsink";
  std::string pipeline_sink =
      "appsrc name=testsource caps=video/x-raw,width=(int)640,height=(int)480,"
      "format=(string)RGB,framerate=(fraction)5/1 ! queue ! videoconvert ! "
      "autovideosink";
  GstSnoopPipe pipe(pipeline_src, pipeline_sink);

  pipe.Init();
  pipe.RegisterCallback([](GstMapInfo *map) {
    g_print("%lu\n", map->size);
  });
  pipe.Start();
  pipe.Stop();

  return 0;
}