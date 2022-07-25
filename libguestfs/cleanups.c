/* libguestfs
 * Copyright (C) 2013 Red Hat Inc.
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

/**
 * Libguestfs uses C<CLEANUP_*> macros to simplify temporary
 * allocations.  They are implemented using the
 * C<__attribute__((cleanup))> feature of gcc and clang.  Typical
 * usage is:
 *
 *  fn ()
 *  {
 *    CLEANUP_FREE char *str = NULL;
 *    str = safe_asprintf (g, "foo");
 *    // str is freed automatically when the function returns
 *  }
 *
 * There are a few catches to be aware of with the cleanup mechanism:
 *
 * =over 4
 *
 * =item *
 *
 * If a cleanup variable is not initialized, then you can end up
 * calling L<free(3)> with an undefined value, resulting in the
 * program crashing.  For this reason, you should usually initialize
 * every cleanup variable with something, eg. C<NULL>
 *
 * =item *
 *
 * Don't mark variables holding return values as cleanup variables.
 *
 * =item *
 *
 * The C<main()> function shouldn't use cleanup variables since it is
 * normally exited by calling L<exit(3)>, and that doesn't call the
 * cleanup handlers.
 *
 * =back
 *
 * The functions in this file are used internally by the C<CLEANUP_*>
 * macros.  Don't call them directly.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "guestfs-utils.h"

/* Stdlib cleanups. */

void
guestfs_int_cleanup_free (void *ptr)
{
  free (* (void **) ptr);
}

void
guestfs_int_cleanup_fclose (void *ptr)
{
  FILE *f = * (FILE **) ptr;

  if (f)
    fclose (f);
}

void
guestfs_int_cleanup_pclose (void *ptr)
{
  FILE *f = * (FILE **) ptr;

  if (f)
    pclose (f);
}

void
guestfs_int_cleanup_free_string_list (char ***ptr)
{
  guestfs_int_free_string_list (*ptr);
}

void
guestfs_int_cleanup_pcre2_match_data_free (void *ptr)
{
  pcre2_match_data *match_data = * (pcre2_match_data **) ptr;
  pcre2_match_data_free (match_data);
}

void
guestfs_int_cleanup_pcre2_substring_free (void *ptr)
{
  PCRE2_UCHAR *str = * (PCRE2_UCHAR **) ptr;
  pcre2_substring_free (str);
}
