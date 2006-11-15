#include <clutter/clutter.h>

int
main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *behave;
  ClutterActor     *stage, *hand;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  GdkPixbuf        *pixbuf;

  ClutterKnot       knots[] = {{ 100, 100 }, { 100, 200 }, { 200, 200 },
                               { 200, 100 }, {100, 100 }};

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a hand */
  hand = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_actor_set_position (hand, 100, 100);
  clutter_group_add (CLUTTER_GROUP (stage), hand);

  /* Make a timeline */
  timeline = clutter_timeline_new (100, 60); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, 0);  

  /* Set an alpha func to power behaviour - ramp is constant rise/fall */
  alpha = clutter_alpha_new_full (timeline,
                                  CLUTTER_ALPHA_RAMP,
                                  NULL, NULL);

  /* Create a behaviour for that alpha */
  behave = clutter_behaviour_opacity_new (alpha, 0X33, 0xff); 

  /* Apply it to our actor */
  clutter_behaviour_apply (behave, hand);

  /* Make a path behaviour and apply that too */
  behave = clutter_behaviour_path_new (alpha, knots, 5); 
  clutter_behaviour_apply (behave, hand);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_group_show_all (CLUTTER_GROUP (stage));

  clutter_main();

  return 0;
}
