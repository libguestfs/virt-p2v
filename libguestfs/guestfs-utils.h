/* libguestfs
 * Copyright (C) 2013-2019 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef GUESTFS_UTILS_H_
#define GUESTFS_UTILS_H_

#include <string.h>

#include "cleanups.h"

#define _(str) dgettext(PACKAGE, (str))

#define STREQ(a,b) (strcmp((a),(b)) == 0)
#define STRCASEEQ(a,b) (strcasecmp((a),(b)) == 0)
#define STRNEQ(a,b) (strcmp((a),(b)) != 0)
#define STRPREFIX(a,b) (strncmp((a),(b),strlen((b))) == 0)

/* A simple (indeed, simplistic) way to build up short lists of
 * arguments.  Your code must define MAX_ARGS to a suitable "larger
 * than could ever be needed" value.  (If the value is exceeded then
 * your code will abort).  For more complex needs, use something else
 * more suitable.
 */
#define ADD_ARG(argv,i,v)                                               \
  do {                                                                  \
    if ((i) >= MAX_ARGS) {                                              \
      fprintf (stderr, "%s: %d: internal error: exceeded MAX_ARGS (%zu) when constructing the command line\n", __FILE__, __LINE__, (size_t) MAX_ARGS); \
      abort ();                                                         \
    }                                                                   \
    (argv)[(i)++] = (v);                                                \
  } while (0)

extern void guestfs_int_free_string_list (char **);
extern size_t guestfs_int_count_strings (char *const *);
extern char **guestfs_int_copy_string_list (char *const *);
extern char **guestfs_int_split_string (char sep, const char *);
extern int guestfs_int_random_string (char *ret, size_t len);
extern char *guestfs_int_drive_name (size_t index, char *ret);
extern int guestfs_int_is_true (const char *str);

/* ANSI colours.  These are defined as macros so that we don't have to
 * define the force_colour global variable in the library.
 */
#define ansi_green(fp)                           \
  do {                                           \
    if (force_colour || isatty (fileno (fp)))    \
      fputs ("\033[0;32m", (fp));                \
  } while (0)
#define ansi_red(fp)                             \
  do {                                           \
    if (force_colour || isatty (fileno (fp)))    \
      fputs ("\033[1;31m", (fp));                \
  } while (0)
#define ansi_blue(fp)                            \
  do {                                           \
    if (force_colour || isatty (fileno (fp)))    \
      fputs ("\033[1;34m", (fp));                \
  } while (0)
#define ansi_magenta(fp)                         \
  do {                                           \
    if (force_colour || isatty (fileno (fp)))    \
      fputs ("\033[1;35m", (fp));                \
  } while (0)
#define ansi_restore(fp)                         \
  do {                                           \
    if (force_colour || isatty (fileno (fp)))    \
      fputs ("\033[0m", (fp));                   \
  } while (0)

#endif /* GUESTFS_UTILS_H_ */
