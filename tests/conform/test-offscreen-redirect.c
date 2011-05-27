#include <clutter/clutter.h>

#include "test-conform-common.h"

typedef struct _FooActor      FooActor;
typedef struct _FooActorClass FooActorClass;

struct _FooActorClass
{
  ClutterActorClass parent_class;
};

struct _FooActor
{
  ClutterActor parent;

  guint8 last_paint_opacity;
  int paint_count;
};

typedef struct
{
  ClutterActor *stage;
  FooActor *foo_actor;
  ClutterActor *parent_container;
  ClutterActor *container;
  ClutterActor *child;
  ClutterActor *unrelated_actor;
} Data;

GType foo_actor_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (FooActor, foo_actor, CLUTTER_TYPE_ACTOR);

static void
foo_actor_paint (ClutterActor *actor)
{
  FooActor *foo_actor = (FooActor *) actor;
  ClutterActorBox allocation;

  foo_actor->last_paint_opacity = clutter_actor_get_paint_opacity (actor);
  foo_actor->paint_count++;

  clutter_actor_get_allocation_box (actor, &allocation);

  /* Paint a red rectangle with the right opacity */
  cogl_set_source_color4ub (255,
                            0,
                            0,
                            foo_actor->last_paint_opacity);
  cogl_rectangle (allocation.x1,
                  allocation.y1,
                  allocation.x2,
                  allocation.y2);
}

static gboolean
foo_actor_get_paint_volume (ClutterActor *actor,
                            ClutterPaintVolume *volume)
{
  return clutter_paint_volume_set_from_allocation (volume, actor);
}

static gboolean
foo_actor_has_overlaps (ClutterActor *actor)
{
  return FALSE;
}

static void
foo_actor_class_init (FooActorClass *klass)
{
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  actor_class->paint = foo_actor_paint;
  actor_class->get_paint_volume = foo_actor_get_paint_volume;
  actor_class->has_overlaps = foo_actor_has_overlaps;
}

static void
foo_actor_init (FooActor *self)
{
}

typedef struct _FooGroup      FooGroup;
typedef struct _FooGroupClass FooGroupClass;

struct _FooGroupClass
{
  ClutterGroupClass parent_class;
};

struct _FooGroup
{
  ClutterGroup parent;
};

G_DEFINE_TYPE (FooGroup, foo_group, CLUTTER_TYPE_GROUP);

static gboolean
foo_group_has_overlaps (ClutterActor *actor)
{
  return FALSE;
}

static void
foo_group_class_init (FooGroupClass *klass)
{
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  actor_class->has_overlaps = foo_group_has_overlaps;
}

static void
foo_group_init (FooGroup *self)
{
}

static void
verify_results (Data *data,
                guint8 expected_color_red,
                guint8 expected_color_green,
                guint8 expected_color_blue,
                int expected_paint_count,
                int expected_paint_opacity)
{
  guchar *pixel;

  data->foo_actor->paint_count = 0;

  /* Read a pixel at the center of the to determine what color it
     painted. This should cause a redraw */
  pixel = clutter_stage_read_pixels (CLUTTER_STAGE (data->stage),
                                     50, 50, /* x/y */
                                     1, 1 /* width/height */);

  g_assert_cmpint (expected_paint_count, ==, data->foo_actor->paint_count);
  g_assert_cmpint (expected_paint_opacity,
                   ==,
                   data->foo_actor->last_paint_opacity);

  g_assert_cmpint (ABS ((int) expected_color_red - (int) pixel[0]), <=, 2);
  g_assert_cmpint (ABS ((int) expected_color_green - (int) pixel[1]), <=, 2);
  g_assert_cmpint (ABS ((int) expected_color_blue - (int) pixel[2]), <=, 2);

  g_free (pixel);
}

static void
verify_redraw (Data *data, int expected_paint_count)
{
  GMainLoop *main_loop = g_main_loop_new (NULL, TRUE);
  guint paint_handler;

  paint_handler = g_signal_connect_data (data->stage,
                                         "paint",
                                         G_CALLBACK (g_main_loop_quit),
                                         main_loop,
                                         NULL,
                                         G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  /* Queue a redraw on the stage */
  clutter_actor_queue_redraw (data->stage);

  data->foo_actor->paint_count = 0;

  /* Wait for it to paint */
  g_main_loop_run (main_loop);

  g_signal_handler_disconnect (data->stage, paint_handler);

  g_assert_cmpint (data->foo_actor->paint_count, ==, expected_paint_count);
}

static gboolean
timeout_cb (gpointer user_data)
{
  Data *data = user_data;

  /* By default the actor shouldn't be redirected so the redraw should
     cause the actor to be painted */
  verify_results (data,
                  255, 0, 0,
                  1,
                  255);

  /* Make the actor semi-transparent and verify the paint opacity */
  clutter_actor_set_opacity (data->container, 127);
  verify_results (data,
                  255, 127, 127,
                  1,
                  127);

  /* Enable offscreen for opacity so it should now paint through the
     fbo. The first paint will still cause the actor to draw because
     it needs to fill the cache first. It should be painted with full
     opacity */
  clutter_actor_set_offscreen_redirect
    (data->container, CLUTTER_OFFSCREEN_REDIRECT_ALWAYS_FOR_OPACITY);
  verify_results (data,
                  255, 127, 127,
                  1,
                  255);

  /* The second time the actor is painted it should be cached */
  verify_results (data,
                  255, 127, 127,
                  0,
                  255);

  /* We should be able to change the opacity without causing the actor
     to redraw */
  clutter_actor_set_opacity (data->container, 64);
  verify_results (data,
                  255, 191, 191,
                  0,
                  255);

  /* Changing it back to fully opaque should cause it not to go
     through the FBO so it will draw */
  clutter_actor_set_opacity (data->container, 255);
  verify_results (data,
                  255, 0, 0,
                  1,
                  255);

  /* Tell it to always redirect through the FBO. This should cause a
     paint of the actor because the last draw didn't go through the
     FBO */
  clutter_actor_set_offscreen_redirect (data->container,
                                        CLUTTER_OFFSCREEN_REDIRECT_ALWAYS);
  verify_results (data,
                  255, 0, 0,
                  1,
                  255);

  /* We should be able to change the opacity without causing the actor
     to redraw */
  clutter_actor_set_opacity (data->container, 64);
  verify_results (data,
                  255, 191, 191,
                  0,
                  255);

  /* Even changing it back to fully opaque shouldn't cause a redraw */
  clutter_actor_set_opacity (data->container, 255);
  verify_results (data,
                  255, 0, 0,
                  0,
                  255);

  /* Queueing a redraw on the actor should cause a redraw */
  clutter_actor_queue_redraw (data->container);
  verify_redraw (data, 1);

  /* Queueing a redraw on a child should cause a redraw */
  clutter_actor_queue_redraw (data->child);
  verify_redraw (data, 1);

  /* Modifying the transformation on the parent should cause a
     redraw */
  clutter_actor_set_anchor_point (data->parent_container, 0, 1);
  verify_redraw (data, 1);

  /* Redrawing an unrelated actor shouldn't cause a redraw */
  clutter_actor_set_position (data->unrelated_actor, 0, 1);
  verify_redraw (data, 0);

  clutter_main_quit ();

  return FALSE;
}

void
test_offscreen_redirect (TestConformSimpleFixture *fixture,
                         gconstpointer test_data)
{
  if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
    {
      Data data;

      data.stage = clutter_stage_get_default ();

      data.parent_container = clutter_group_new ();

      data.container = g_object_new (foo_group_get_type (), NULL);

      data.foo_actor = g_object_new (foo_actor_get_type (), NULL);
      clutter_actor_set_size (CLUTTER_ACTOR (data.foo_actor), 100, 100);

      clutter_container_add_actor (CLUTTER_CONTAINER (data.container),
                                   CLUTTER_ACTOR (data.foo_actor));

      clutter_container_add_actor (CLUTTER_CONTAINER (data.parent_container),
                                   data.container);

      clutter_container_add_actor (CLUTTER_CONTAINER (data.stage),
                                   data.parent_container);

      data.child = clutter_rectangle_new ();
      clutter_actor_set_size (data.child, 1, 1);
      clutter_container_add_actor (CLUTTER_CONTAINER (data.container),
                                   data.child);

      data.unrelated_actor = clutter_rectangle_new ();
      clutter_actor_set_size (data.child, 1, 1);
      clutter_container_add_actor (CLUTTER_CONTAINER (data.stage),
                                   data.unrelated_actor);

      clutter_actor_show (data.stage);

      /* Start the test after a short delay to allow the stage to
         render its initial frames without affecting the results */
      g_timeout_add_full (G_PRIORITY_LOW, 250, timeout_cb, &data, NULL);

      clutter_main ();

      if (g_test_verbose ())
        g_print ("OK\n");
    }
  else if (g_test_verbose ())
    g_print ("Skipping\n");
}

