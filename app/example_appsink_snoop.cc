/* GStreamer
 *
 * appsink-snoop.c: example for modify data in video pipeline
 * using appsink and appsrc.
 *
 * Refer to
 * https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/81231874/Video+Data+Modification+in+Gstreamer+Application
 *
 * Based on the appsink-src.c example
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include <iostream>
#include <string>

#include "glibconfig.h"
#include "gst/gstpad.h"

typedef struct {
  GMainLoop *loop;
  GstElement *source;
  GstElement *sink;
} ProgramData;

/* user modify the data here */

static GstFlowReturn modify_in_data(GstMapInfo *map) {
  gsize dataLength;
  guint8 *rdata;

  dataLength = map->size;
  rdata = map->data;
  g_print("%s dataLen = %lu\n", __func__, dataLength);

  /* Modify half of frame to plane white */
  for (gsize i = 0; i <= dataLength / 2; i++) {
    rdata[i] = 0xff;
  }

  return GST_FLOW_OK;
}

/* called when the appsink notifies us that there is a new buffer ready for
 * processing */
static GstFlowReturn on_new_sample_from_sink(GstElement *elt,
                                             ProgramData *data) {
  GstSample *sample;
  GstBuffer *app_buffer, *buffer;
  GstElement *source;
  GstFlowReturn ret;
  GstMapInfo map;
  //   guint8 *rdata;
  //   int dataLength;
  //   int i;
  g_print("%s\n", __func__);

  /* get the sample from appsink */
  sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
  buffer = gst_sample_get_buffer(sample);

  /* make a copy */
  app_buffer = gst_buffer_copy_deep(buffer);

  gst_buffer_map(app_buffer, &map, GST_MAP_WRITE);

  /* Here You modify your buffer data */
  modify_in_data(&map);

  /* get source an push new buffer */
  source = gst_bin_get_by_name(GST_BIN(data->sink), "testsource");
  ret = gst_app_src_push_buffer(GST_APP_SRC(source), app_buffer);
  gst_sample_unref(sample);
  /* we don't need the appsink sample anymore */
  gst_buffer_unmap(app_buffer, &map);
  gst_object_unref(source);

  return ret;
}

/* called when we get a GstMessage from the source pipeline when we get EOS, we
 * notify the appsrc of it. */
static gboolean on_source_message(GstBus *bus, GstMessage *message,
                                  ProgramData *data) {
  GstElement *source;
  g_print("%s\n", __func__);
  g_print("%s\n", GST_MESSAGE_TYPE_NAME(message));

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
      g_print("The source got dry\n");
      source = gst_bin_get_by_name(GST_BIN(data->sink), "testsource");
      gst_app_src_end_of_stream(GST_APP_SRC(source));
      gst_object_unref(source);
      break;
    case GST_MESSAGE_ERROR:
      g_print("Received error\n");
      g_main_loop_quit(data->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

/* called when we get a GstMessage from the sink pipeline when we get EOS, we
 * exit the mainloop and this testapp. */
static gboolean on_sink_message(GstBus *bus, GstMessage *message,
                                ProgramData *data) {
  /* nil */
  g_print("%s\n", __func__);
  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
      g_print("Finished playback\n");
      g_main_loop_quit(data->loop);
      break;
    case GST_MESSAGE_ERROR:
      g_print("Received error\n");
      g_main_loop_quit(data->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

int main(int argc, char *argv[]) {
  ProgramData *data = NULL;
  gchar *string = NULL;
  GstBus *bus = NULL;
  GstElement *testsink = NULL;
  GstElement *testsource = NULL;

  gst_init(&argc, &argv);

  data = g_new0(ProgramData, 1);

  data->loop = g_main_loop_new(NULL, FALSE);

  /* setting up source pipeline, we read from a file and convert to our desired
   * caps. */
  string = g_strdup_printf(
      "videotestsrc num-buffers=600 ! video/x-raw,width=(int)640,height=(int)480,"
      "format=(string)RGB,framerate=(fraction)30/1 ! appsink name=testsink");
  data->source = gst_parse_launch(string, NULL);
  g_free(string);

  if (data->source == NULL) {
    g_print("Bad source\n");
    g_main_loop_unref(data->loop);
    g_free(data);
    return -1;
  }

  g_print("Capture bin launched\n");
  /* to be notified of messages from this pipeline, mostly EOS */
  bus = gst_element_get_bus(data->source);
  gst_bus_add_watch(bus, (GstBusFunc)on_source_message, data);
  gst_object_unref(bus);

  /* we use appsink in push mode, it sends us a signal when data is available
   * and we pull out the data in the signal callback. */
  testsink = gst_bin_get_by_name(GST_BIN(data->source), "testsink");
  g_object_set(G_OBJECT(testsink), "emit-signals", TRUE, "sync", FALSE, NULL);
  g_signal_connect(testsink, "new-sample", G_CALLBACK(on_new_sample_from_sink),
                   data);
  gst_object_unref(testsink);

  /* setting up sink pipeline, we push video data into this pipeline that will
   * then copy in file using the default filesink. We have no blocking
   * behaviour on the src which means that we will push the entire file into
   * memory. */
  // string =
  //     g_strdup_printf("appsrc name=testsource ! filesink location=tmp.yuv");
  // string = g_strdup_printf(
  //     "appsrc name=testsource caps=video/x-raw,width=640,height=480,"
  //     "format=RGB,framerate=(fraction)5/1 ! jpegenc ! filesink location=test.jpg -e");
  string = g_strdup_printf(
      "appsrc name=testsource caps=video/x-raw,width=(int)640,height=(int)480,"
      "format=(string)RGB,framerate=(fraction)5/1 ! queue ! videoconvert ! autovideosink");
  data->sink = gst_parse_launch(string, NULL);
  g_free(string);

  if (data->sink == NULL) {
    g_print("Bad sink\n");
    gst_object_unref(data->source);
    g_main_loop_unref(data->loop);
    g_free(data);
    return -1;
  }
  g_print("Play bin launched\n");

  testsource = gst_bin_get_by_name(GST_BIN(data->sink), "testsource");
  /* configure for time-based format */
  g_object_set(testsource, "format", GST_FORMAT_TIME, NULL);
  /* uncomment the next line to block when appsrc has buffered enough */
  /* g_object_set (testsource, "block", TRUE, NULL); */
  gst_object_unref(testsource);

  bus = gst_element_get_bus(data->sink);
  gst_bus_add_watch(bus, (GstBusFunc)on_sink_message, data);
  gst_object_unref(bus);

  g_print("Going to set state to play\n");
  /* launching things */
  gst_element_set_state(data->sink, GST_STATE_PLAYING);
  gst_element_set_state(data->source, GST_STATE_PLAYING);

  /* let's run !, this loop will quit when the sink pipeline goes EOS or when an
   * error occurs in the source or sink pipelines. */
  g_print("Let's run!\n");
  g_main_loop_run(data->loop);
  g_print("Going out\n");

  gst_element_set_state(data->source, GST_STATE_NULL);
  gst_element_set_state(data->sink, GST_STATE_NULL);

  gst_object_unref(data->source);
  gst_object_unref(data->sink);
  g_main_loop_unref(data->loop);
  g_free(data);

  return 0;
}