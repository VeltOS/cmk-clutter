/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * SECTION:clutter-util
 * @short_description: Misc utility functions.
 *
 * Various misc utilility functions.
 */


#include "clutter-util.h"
#include "clutter-main.h"

static int TrappedErrorCode = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
  TrappedErrorCode = error->error_code;
  return 0;
}

/**
 * clutter_util_trap_x_errors:
 *
 * Trap X errors so they don't cause an abort.
 */
void
clutter_util_trap_x_errors(void)
{
  TrappedErrorCode  = 0;
  old_error_handler = XSetErrorHandler(error_handler);
}

/**
 * clutter_util_untrap_x_errors:
 *
 * Stop trapping X errors.
 *
 * Return value: 0 if there was no error, or the last X error that occurred.
 */
int
clutter_util_untrap_x_errors(void)
{
  XSetErrorHandler(old_error_handler);
  return TrappedErrorCode;
}

/**
 * clutter_util_next_p2:
 * @a: Value to get the next power
 *
 * Calculates the next power greater than @a.
 *
 * Return value: The next power after @a.
 */
int 
clutter_util_next_p2 (int a)
{
  int rval=1;

  while(rval < a) 
    rval <<= 1;

  return rval;
}
