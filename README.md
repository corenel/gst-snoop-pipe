# gst-snoop-pipe

This is a very simple application concentrating to use appsink and appsrc by constructing a pipeline to generate video, stream it into an application's code, modify the data and then stream it back into your video output device i.e filesrc.  

## Examples

- `app/test_gst_snoop_pipe.cc`: simple demo of provided class `GstSnoopPipe`:
  - receive data from GStreamer
  - modify data by user defined callback function
  - push data to GStreamer

## Acknowlegements

- [Video Data Modification in Gstreamer Application](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/81231874/Video+Data+Modification+in+Gstreamer+Application)
- [Short-cutting the pipeline](https://gstreamer.freedesktop.org/documentation/tutorials/basic/short-cutting-the-pipeline.html)
- [Pipeline manipulation](https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html)
- [appsink-src.c](https://github.com/GStreamer/gst-plugins-base/blob/master/tests/examples/app/appsink-src.c)