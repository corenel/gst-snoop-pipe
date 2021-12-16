# gst-snoop-pipe

This is a very simple application concentrating to use appsink and appsrc by constructing a pipeline to generate video, stream it into an application's code, modify the data and then stream it back into your video output device i.e filesrc.  

## Examples

- `app/test_gst_snoop_pipe.cc`: simple demo of provided class `GstSnoopPipe`:
  - receive data from GStreamer
  - modify data by user defined callback function
  - push data to GStreamer
