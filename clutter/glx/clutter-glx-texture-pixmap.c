/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Johan Bilien  <johan.bilien@nokia.com>
 *
 * Copyright (C) 2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-glx-texture-pixmap
 * @short_description: A texture which displays the content of an X Pixmap.
 *
 * #ClutterGLXTexturePixmap is a class for displaying the content of an
 * X Pixmap as a ClutterActor. Used together with the X Composite extension,
 * it allows to display the content of X Windows inside Clutter.
 *
 * The class requires the GLX_EXT_texture_from_pixmap OpenGL extension
 * (http://people.freedesktop.org/~davidr/GLX_EXT_texture_from_pixmap.txt)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../x11/clutter-x11-texture-pixmap.h"
#include "clutter-glx-texture-pixmap.h"
#include "clutter-glx.h"
#include "clutter-backend-glx.h"

#include "../clutter-util.h"

#include "cogl.h"

enum
{
  PROP_PIXMAP = 1,
  PROP_PIXMAP_WIDTH,
  PROP_PIXMAP_HEIGHT,
  PROP_DEPTH
};

struct _ClutterGLXTexturePixmapPrivate
{
  COGLenum      target_type;
  guint         texture_id;
  GLXPixmap     glx_pixmap;
  gboolean      bound;
};

static ClutterBackendGLX *backend = NULL;

static void clutter_glx_texture_pixmap_class_init (ClutterGLXTexturePixmapClass *klass);
static void clutter_glx_texture_pixmap_init       (ClutterGLXTexturePixmap *self);
static GObject *clutter_glx_texture_pixmap_constructor (GType                  type,
                                                        guint                  n_construct_properties,
                                                        GObjectConstructParam *construct_properties);
static void clutter_glx_texture_pixmap_dispose    (GObject *object);
static void clutter_glx_texture_pixmap_notify     (GObject    *object,
                                                   GParamSpec *pspec);

static void clutter_glx_texture_pixmap_realize (ClutterActor *actor);
static void clutter_glx_texture_pixmap_unrealize (ClutterActor *actor);
static void clutter_glx_texture_pixmap_paint (ClutterActor *actor);

static void clutter_glx_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                                    gint x,
                                                    gint y,
                                                    gint width,
                                                    gint height);

static void clutter_glx_texture_pixmap_create_glx_pixmap (ClutterGLXTexturePixmap *texture);

G_DEFINE_TYPE (ClutterGLXTexturePixmap, clutter_glx_texture_pixmap, CLUTTER_X11_TYPE_TEXTURE_PIXMAP);

static void
clutter_glx_texture_pixmap_class_init (ClutterGLXTexturePixmapClass *klass)
{
  GObjectClass                 *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass            *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterX11TexturePixmapClass *x11_texture_class =
      CLUTTER_X11_TEXTURE_PIXMAP_CLASS (klass);
  ClutterBackend               *default_backend;

  g_type_class_add_private (klass, sizeof (ClutterGLXTexturePixmapPrivate));

  object_class->constructor = clutter_glx_texture_pixmap_constructor;
  object_class->dispose = clutter_glx_texture_pixmap_dispose;
  object_class->notify = clutter_glx_texture_pixmap_notify;

  actor_class->realize   = clutter_glx_texture_pixmap_realize;
  actor_class->unrealize = clutter_glx_texture_pixmap_unrealize;
  actor_class->paint     = clutter_glx_texture_pixmap_paint;

  x11_texture_class->update_area = clutter_glx_texture_pixmap_update_area;

  default_backend = clutter_get_default_backend ();
  if (!CLUTTER_IS_BACKEND_GLX (default_backend))
    {
      g_critical ("ClutterGLXTexturePixmap instanciated with a "
                  "non-GLX backend");
      return;
    }

  backend = (ClutterBackendGLX *)default_backend;
/*  backend->t_f_p = FALSE;*/

}

static void
clutter_glx_texture_pixmap_init (ClutterGLXTexturePixmap *self)
{
  ClutterGLXTexturePixmapPrivate *priv;

  priv = self->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                                   ClutterGLXTexturePixmapPrivate);


  if (clutter_feature_available (CLUTTER_FEATURE_TEXTURE_RECTANGLE))
    priv->target_type = CGL_TEXTURE_RECTANGLE_ARB;
  else
    priv->target_type = CGL_TEXTURE_2D;
}

static GObject *
clutter_glx_texture_pixmap_constructor (GType                  type,
                                        guint                  n_construct_properties,
                                        GObjectConstructParam *construct_properties)
{
  GObject *object = G_OBJECT_CLASS (clutter_glx_texture_pixmap_parent_class)->
      constructor (type, n_construct_properties, construct_properties);

  g_object_set (object,
                "sync-size", FALSE,
                NULL);

  return object;
}

static void
clutter_glx_texture_pixmap_dispose (GObject *object)
{
  ClutterGLXTexturePixmapPrivate *priv = CLUTTER_GLX_TEXTURE_PIXMAP (object)->priv;

  if (priv->glx_pixmap != None)
    {
      glXDestroyGLXPixmap (((ClutterBackendX11 *)backend)->xdpy,
                           priv->glx_pixmap);
      priv->glx_pixmap = None;
    }

  G_OBJECT_CLASS (clutter_glx_texture_pixmap_parent_class)->dispose (object);
}

static void
clutter_glx_texture_pixmap_notify (GObject *object, GParamSpec *pspec)
{
  if (g_str_equal (pspec->name, "pixmap"))
    {
      ClutterGLXTexturePixmap *texture = CLUTTER_GLX_TEXTURE_PIXMAP (object);

      clutter_glx_texture_pixmap_create_glx_pixmap (texture);
    }
}

static void
clutter_glx_texture_pixmap_realize (ClutterActor *actor)
{
  ClutterGLXTexturePixmapPrivate *priv =
      CLUTTER_GLX_TEXTURE_PIXMAP (actor)->priv;
  COGLenum                      pixel_type, pixel_format,
                                filter_quality;
  gboolean                      repeat_x, repeat_y;
  guint                         width, height;

  if (!backend->t_f_p)
    {
      CLUTTER_ACTOR_CLASS (clutter_glx_texture_pixmap_parent_class)->
          realize (actor);
      return;
    }

  g_object_get (actor,
                "pixel-type",     &pixel_type,
                "pixel-format",   &pixel_format,
                "repeat-x",       &repeat_x,
                "repeat-y",       &repeat_y,
                "filter-quality", &filter_quality,
                "pixmap-width",   &width,
                "pixmap-height",  &height,
                NULL);

  cogl_textures_create (1, &priv->texture_id);

  clutter_glx_texture_pixmap_update_area (CLUTTER_X11_TEXTURE_PIXMAP (actor),
                                          0, 0,
                                          width, height);

  cogl_texture_set_alignment (priv->target_type, 4, width);

  cogl_texture_set_filters (priv->target_type,
                            filter_quality ? CGL_LINEAR : CGL_NEAREST,
                            filter_quality ? CGL_LINEAR : CGL_NEAREST);

  cogl_texture_set_wrap (priv->target_type,
                         repeat_x ? CGL_REPEAT : CGL_CLAMP_TO_EDGE,
                         repeat_y ? CGL_REPEAT : CGL_CLAMP_TO_EDGE);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_glx_texture_pixmap_unrealize (ClutterActor *actor)
{
  ClutterGLXTexturePixmapPrivate *priv =
      CLUTTER_GLX_TEXTURE_PIXMAP (actor)->priv;
  Display                        *dpy = ((ClutterBackendX11 *)backend)->xdpy;

  if (!backend->t_f_p)
    {
      CLUTTER_ACTOR_CLASS (clutter_glx_texture_pixmap_parent_class)->
          unrealize (actor);
      return;
    }

  if (!CLUTTER_ACTOR_IS_REALIZED (actor))
    return;

  if (priv->bound && priv->glx_pixmap)
    {
      (backend->release_tex_image) (dpy,
                                    priv->glx_pixmap,
                                    GLX_FRONT_LEFT_EXT);
    }

  cogl_textures_destroy (1, &priv->texture_id);

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static void
texture_render_to_gl_quad (ClutterGLXTexturePixmap *texture,
                           int                      x_1,
                           int                      y_1,
                           int                      x_2,
                           int                      y_2)
{
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;

  int   qx1 = 0, qx2 = 0, qy1 = 0, qy2 = 0;
  int   qwidth = 0, qheight = 0;
  float tx, ty;
  guint width, height;

  g_object_get (texture,
                "pixmap-width",         &width,
                "pixmap-height",        &height,
                NULL);

  qwidth  = x_2 - x_1;
  qheight = y_2 - y_1;

  cogl_texture_bind (priv->target_type, priv->texture_id);

  if (priv->target_type == CGL_TEXTURE_2D) /* POT */
    {
      tx = (float) width  / clutter_util_next_p2 (width);
      ty = (float) height / clutter_util_next_p2 (height);
    }
  else
    {
      tx = (float) width;
      ty = (float) height;

    }

  qx1 = x_1; qx2 = x_2;
  qy1 = y_1; qy2 = y_2;

  cogl_texture_quad (x_1, x_2, y_1, y_2,
                     0,
                     0,
                     CLUTTER_FLOAT_TO_FIXED (tx),
                     CLUTTER_FLOAT_TO_FIXED (ty));
}

static void
clutter_glx_texture_pixmap_paint (ClutterActor *actor)
{
  ClutterGLXTexturePixmap        *texture = CLUTTER_GLX_TEXTURE_PIXMAP (actor);
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;

  gint            x_1, y_1, x_2, y_2;
  ClutterColor    col = { 0xff, 0xff, 0xff, 0xff };

  if (!backend->t_f_p)
    {
      CLUTTER_ACTOR_CLASS (clutter_glx_texture_pixmap_parent_class)->
          paint (actor);
      return;
    }

  if (!CLUTTER_ACTOR_IS_REALIZED (actor))
    clutter_actor_realize (actor);

  cogl_push_matrix ();

  switch (priv->target_type)
    {
      case CGL_TEXTURE_2D:
          cogl_enable (CGL_ENABLE_TEXTURE_2D|CGL_ENABLE_BLEND);
          break;
      case CGL_TEXTURE_RECTANGLE_ARB:
          cogl_enable (CGL_ENABLE_TEXTURE_RECT|CGL_ENABLE_BLEND);
          break;
      default:
          break;
    }

  col.alpha = clutter_actor_get_opacity (actor);

  cogl_color (&col);

  clutter_actor_get_coords (actor, &x_1, &y_1, &x_2, &y_2);

  texture_render_to_gl_quad (texture, 0, 0, x_2 - x_1, y_2 - y_1);

  cogl_pop_matrix ();
}

static GLXFBConfig *
get_fbconfig_for_depth (guint depth)
{
  GLXFBConfig *fbconfigs, *ret = NULL;
  int          n_elements, i, found;
  Display     *dpy = ((ClutterBackendX11 *)backend)->xdpy;
  int          db, stencil, alpha, mipmap, rgba, value;

  fbconfigs = glXGetFBConfigs (dpy,
                               ((ClutterBackendX11 *)backend)->xscreen_num,
                               &n_elements);

  db      = G_MAXSHORT;
  stencil = G_MAXSHORT;
  mipmap  = 0;
  rgba    = 0;

  found = n_elements;


  for (i = 0; i < n_elements; i++)
    {
      XVisualInfo *vi;
      int          visual_depth;

      vi = glXGetVisualFromFBConfig (dpy,
                                     fbconfigs[i]);
      if (vi == NULL)
        continue;

      visual_depth = vi->depth;

      XFree (vi);

      if (visual_depth != depth)
        continue;

      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_ALPHA_SIZE,
                            &alpha);
      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_BUFFER_SIZE,
                            &value);
      if (value != depth && (value - alpha) != depth)
        continue;

      value = 0;
      if (depth == 32)
        {
          glXGetFBConfigAttrib (dpy,
                                fbconfigs[i],
                                GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                &value);

          if (value)
            rgba = 1;
        }

      if (!value)
        {
          if (rgba)
            continue;

          glXGetFBConfigAttrib (dpy,
                                fbconfigs[i],
                                GLX_BIND_TO_TEXTURE_RGB_EXT,
                                &value);
          if (!value)
            continue;
        }

      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_DOUBLEBUFFER,
                            &value);
      if (value > db)
        continue;

      db = value;

      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_STENCIL_SIZE,
                            &value);
      if (value > stencil)
        continue;

      stencil = value;

      found = i;
    }

  if (found != n_elements)
    {
      ret = g_malloc (sizeof (GLXFBConfig));
      *ret = fbconfigs[found];
    }

  if (n_elements)
    XFree (fbconfigs);

  return ret;
}

static void
clutter_glx_texture_pixmap_create_glx_pixmap (ClutterGLXTexturePixmap *texture)
{
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;
  GLXPixmap                       glx_pixmap;
  int                             attribs[7], i = 0;
  GLXFBConfig                    *fbconfig;
  Display                        *dpy = ((ClutterBackendX11 *)backend)->xdpy;
  guint                           depth;
  Pixmap                          pixmap;

  g_object_get (texture,
                "depth",                &depth,
                "pixmap",               &pixmap,
                NULL);

  fbconfig = get_fbconfig_for_depth (depth);

  if (!fbconfig)
    {
      g_critical ("Could not find an FBConfig for selected pixmap");
      return;
    }

  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

  if (depth == 24)
    {
      attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
    }
  else if (depth == 32)
    {
      attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;
    }
  else
    {
      g_critical ("Pixmap with depth bellow 24 are not supported");
      return;
    }

  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = 0;

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;

  if (priv->target_type == CGL_TEXTURE_RECTANGLE_ARB)
    attribs[i++] = GLX_TEXTURE_RECTANGLE_EXT;
  else
    attribs[i++] = GLX_TEXTURE_2D_EXT;

  attribs[i++] = None;


  clutter_x11_trap_x_errors ();
  glx_pixmap = glXCreatePixmap (dpy,
                                *fbconfig,
                                pixmap,
                                attribs);
  XSync (dpy, FALSE);
  clutter_x11_untrap_x_errors ();

  g_free (fbconfig);

  if (glx_pixmap != None)
    {
      if (priv->glx_pixmap)
        {
          if (backend->t_f_p &&
              CLUTTER_ACTOR_IS_REALIZED (texture) &&
              priv->bound)
            {
              cogl_texture_bind (priv->target_type, priv->texture_id);

              (backend->release_tex_image) (dpy,
                                            priv->glx_pixmap,
                                            GLX_FRONT_LEFT_EXT);
            }

          clutter_x11_trap_x_errors ();
          glXDestroyGLXPixmap (dpy, priv->glx_pixmap);
          XSync (dpy, FALSE);
          clutter_x11_untrap_x_errors ();
        }

      priv->glx_pixmap = glx_pixmap;
    }
}

static void
clutter_glx_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                        gint                     x,
                                        gint                     y,
                                        gint                     width,
                                        gint                     height)
{
  ClutterGLXTexturePixmapPrivate       *priv;
  Display                              *dpy =
      ((ClutterBackendX11 *)backend)->xdpy;

  priv = CLUTTER_GLX_TEXTURE_PIXMAP (texture)->priv;

  if (!backend->t_f_p)
    {
      CLUTTER_X11_TEXTURE_PIXMAP_CLASS (texture)->update_area (texture,
                                                               x, y,
                                                               width, height);
      return;
    }

  if (!CLUTTER_ACTOR_IS_REALIZED (texture))
    return;

  cogl_texture_bind (priv->target_type, priv->texture_id);

  if (backend->t_f_p)
    {
      (backend->bind_tex_image) (dpy,
                                 priv->glx_pixmap,
                                 GLX_FRONT_LEFT_EXT,
                                 NULL);

      priv->bound = TRUE;
    }
}

/**
 * clutter_glx_texture_pixmap_new_with_pixmap:
 * @pixmap: the X Pixmap to which this texture should be bound
 * @width: the width of the X pixmap
 * @height: the height of the X pixmap
 * @depth: the depth of the X pixmap
 *
 * Return value: A new #ClutterGLXTexturePixmap bound to the given X Pixmap
 *
 * Since: 0.8
 **/
ClutterActor *
clutter_glx_texture_pixmap_new_with_pixmap (Pixmap pixmap,
					    guint  width,
					    guint  height,
					    guint  depth)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                        "pixmap", pixmap,
                        "pixmap-width",  width,
                        "pixmap-height", height,
                        "depth",  depth,
                        NULL);

  return actor;
}

/**
 * clutter_glx_texture_pixmap_new:
 *
 * Return value: A new #ClutterGLXTexturePixmap
 *
 * Since: 0.8
 **/
ClutterActor *
clutter_glx_texture_pixmap_new (void)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP, NULL);

  return actor;
}
