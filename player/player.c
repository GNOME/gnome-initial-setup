/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Based on examples/video-player.c in clutter-gst sources
 *     Copyright 2007,2008 OpenedHand
 */

#include "config.h"

#include <stdlib.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter-gst/clutter-gst.h>

static int ret = EXIT_SUCCESS;

#define FADE_OUT_TIME 500
#define BG_COLOR "#d3d8ce"
static ClutterColor bg_color;

static ClutterActor *stage;

static void
fade_out (ClutterActor *actor)
{
  clutter_actor_save_easing_state (actor);
  clutter_actor_set_easing_duration (actor, FADE_OUT_TIME);
  clutter_actor_set_opacity (actor, 0);
  clutter_actor_restore_easing_state (actor);
}

static void
fade_out_and_quit (void)
{
  fade_out (stage);
  g_signal_connect (stage, "transitions-completed",
                    G_CALLBACK (clutter_main_quit), NULL);
}

static void
on_video_texture_eos (ClutterMedia *media)
{
  fade_out_and_quit ();
}

static gboolean
key_press_cb (ClutterActor *stage,
              ClutterEvent *event)
{
  switch (clutter_event_get_key_symbol ( (event)))
    {
    case CLUTTER_Escape:
      ret = EXIT_FAILURE;
      clutter_actor_destroy (stage);
      break;
    }

  return TRUE;
}

/* XXX: not sure how to keep the frame data on screen during
 * fade out, so just have a solid idle material for now. */
static void
set_idle_material (ClutterGstVideoTexture *video_texture)
{
  CoglColor transparent;
  CoglHandle material;

  cogl_color_init_from_4ub (&transparent, 0, 0, 0, 0);
  material = cogl_material_new ();
  cogl_material_set_color (material, &transparent);
  clutter_gst_video_texture_set_idle_material (video_texture, material);
  cogl_handle_unref (material);
}

static void
set_above_and_fullscreen (void)
{
  Display *dpy = clutter_x11_get_default_display ();
  Window win = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));
  char *atom_names[2] = {
    "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE_ABOVE",
  };
  Atom states[G_N_ELEMENTS (atom_names)];

  XInternAtoms (dpy, atom_names, G_N_ELEMENTS (atom_names),
                False, states);

  XChangeProperty (dpy, win,
                   XInternAtom (dpy, "_NET_WM_STATE", False),
                   XA_ATOM, 32, PropModeReplace,
                   (unsigned char *) states, G_N_ELEMENTS (atom_names));
}

int
main (int argc, char *argv[])
{
  ClutterActor *video;

  /* So we can fade out at the end. */
  clutter_x11_set_use_argb_visual (TRUE);

  if (clutter_gst_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  if (argc < 2)
    {
      g_print ("Usage: %s [OPTIONS] <video file>\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!clutter_color_from_string (&bg_color, BG_COLOR))
    {
      g_warning ("Invalid BG_COLOR");
      exit (1);
    }

  stage = clutter_stage_new ();

  /* Clutter's full-screening code does not allow us to
   * set both that and _NET_WM_STATE_ABOVE, so do the state
   * management ourselves for now. */
#if 0
  clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), TRUE);
#endif

  /* Clutter will set maximum size restrictions (meaning not
   * full screen) unless I set this. */
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

  clutter_stage_set_use_alpha (CLUTTER_STAGE (stage), TRUE);

  clutter_actor_set_background_color (stage, &bg_color);
  clutter_actor_set_layout_manager (stage,
                                    clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
                                                            CLUTTER_BIN_ALIGNMENT_FIXED));

  clutter_actor_realize (stage);
  set_above_and_fullscreen ();

  video = clutter_gst_video_texture_new ();
  clutter_actor_set_x_expand (video, TRUE);
  clutter_actor_set_y_expand (video, TRUE);
  clutter_actor_set_x_align (video, CLUTTER_ACTOR_ALIGN_CENTER);
  clutter_actor_set_y_align (video, CLUTTER_ACTOR_ALIGN_CENTER);
  set_idle_material (CLUTTER_GST_VIDEO_TEXTURE (video));

  g_signal_connect (video,
                    "eos",
                    G_CALLBACK (on_video_texture_eos),
                    NULL);

  g_signal_connect (stage,
                    "destroy",
                    G_CALLBACK (clutter_main_quit),
                    NULL);

  clutter_media_set_filename (CLUTTER_MEDIA (video), argv[1]);
  clutter_stage_hide_cursor (CLUTTER_STAGE (stage));

  clutter_actor_add_child (stage, video);

  g_signal_connect (stage, "key-press-event", G_CALLBACK (key_press_cb), NULL);

  clutter_media_set_playing (CLUTTER_MEDIA (video), TRUE);
  clutter_actor_show (stage);
  clutter_main ();

  return ret;
}
