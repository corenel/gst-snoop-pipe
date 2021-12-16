# gst-snoop-pipe

This is a very simple application of building a GStreamer pipeline using `appsink` and `appsrc`, which generates video, streams it to the application's code, modifies the data, and then streams it back to your video output device i.e. filesrc.

## Prerequisites

- CMake 2.8.9 or later
- GStreamer 1.0.0 or later (e.g., `libgstreamer1.0-dev`)
- GStreamer plugins 1.0.0 or later (e.g., `libgstreamer-plugins-base1.0-dev`)
- Glib / GObject (e.g., `libglib2.0-dev`)
- (optional) OpenCV

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