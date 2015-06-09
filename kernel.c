/* virt-p2v
 * Copyright (C) 2009-2015 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Kernel-driven configuration, non-interactive. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <locale.h>
#include <libintl.h>

#include "p2v.h"

static void notify_ui_callback (int type, const char *data);

void
kernel_configuration (struct config *config, char **cmdline, int cmdline_source)
{
  const char *p;

  p = get_cmdline_key (cmdline, "p2v.server");
  assert (p); /* checked by caller */
  free (config->server);
  config->server = strdup (p);

  p = get_cmdline_key (cmdline, "p2v.port");
  if (p) {
    if (sscanf (p, "%d", &config->port) != 1) {
      fprintf (stderr, "%s: cannot parse p2v.port from kernel command line",
               guestfs_int_program_name);
      exit (EXIT_FAILURE);
    }
  }

  p = get_cmdline_key (cmdline, "p2v.username");
  if (p) {
    free (config->username);
    config->username = strdup (p);
  }

  p = get_cmdline_key (cmdline, "p2v.password");
  if (p) {
    free (config->password);
    config->password = strdup (p);
  }

  p = get_cmdline_key (cmdline, "p2v.sudo");
  if (p)
    config->sudo = 1;

  /* We should now be able to connect and interrogate virt-v2v
   * on the conversion server.
   */
  if (test_connection (config) == -1) {
    const char *err = get_ssh_error ();

    fprintf (stderr, "%s: error opening control connection to %s:%d: %s\n",
             guestfs_int_program_name, config->server, config->port, err);
    exit (EXIT_FAILURE);
  }

  p = get_cmdline_key (cmdline, "p2v.name");
  if (p) {
    free (config->guestname);
    config->guestname = strdup (p);
  }

  p = get_cmdline_key (cmdline, "p2v.vcpus");
  if (p) {
    if (sscanf (p, "%d", &config->vcpus) != 1) {
      fprintf (stderr, "%s: cannot parse p2v.vcpus from kernel command line\n",
               guestfs_int_program_name);
      exit (EXIT_FAILURE);
    }
  }

  p = get_cmdline_key (cmdline, "p2v.memory");
  if (p) {
    char mem_code[2];

    if (sscanf (p, "%" SCNu64 "%c", &config->memory, mem_code) != 1) {
      fprintf (stderr, "%s: cannot parse p2v.memory from kernel command line\n",
               guestfs_int_program_name);
      exit (EXIT_FAILURE);
    }
    config->memory *= 1024;
    if (mem_code[0] == 'M' || mem_code[0] == 'G')
      config->memory *= 1024;
    if (mem_code[0] == 'G')
      config->memory *= 1024;
    if (mem_code[0] != 'M' && mem_code[0] != 'G') {
      fprintf (stderr, "%s: p2v.memory on kernel command line must be followed by 'G' or 'M'\n",
               guestfs_int_program_name);
      exit (EXIT_FAILURE);
    }
  }

  p = get_cmdline_key (cmdline, "p2v.disks");
  if (p) {
    CLEANUP_FREE char *t;

    t = strdup (p);
    guestfs_int_free_string_list (config->disks);
    config->disks = guestfs_int_split_string (',', t);
  }

  p = get_cmdline_key (cmdline, "p2v.removable");
  if (p) {
    CLEANUP_FREE char *t;

    t = strdup (p);
    guestfs_int_free_string_list (config->removable);
    config->removable = guestfs_int_split_string (',', t);
  }

  p = get_cmdline_key (cmdline, "p2v.interfaces");
  if (p) {
    CLEANUP_FREE char *t;

    t = strdup (p);
    guestfs_int_free_string_list (config->interfaces);
    config->interfaces = guestfs_int_split_string (',', t);
  }

  p = get_cmdline_key (cmdline, "p2v.network");
  if (p) {
    CLEANUP_FREE char *t;

    t = strdup (p);
    guestfs_int_free_string_list (config->network_map);
    config->network_map = guestfs_int_split_string (',', t);
  }

  p = get_cmdline_key (cmdline, "p2v.o");
  if (p) {
    free (config->output);
    config->output = strdup (p);
  }

  p = get_cmdline_key (cmdline, "p2v.oa");
  if (p) {
    if (STREQ (p, "sparse"))
      config->output_allocation = OUTPUT_ALLOCATION_SPARSE;
    else if (STREQ (p, "preallocated"))
      config->output_allocation = OUTPUT_ALLOCATION_PREALLOCATED;
    else
      fprintf (stderr, "%s: warning: don't know what p2v.oa=%s means\n",
               guestfs_int_program_name, p);
  }

  p = get_cmdline_key (cmdline, "p2v.oc");
  if (p) {
    free (config->output_connection);
    config->output_connection = strdup (p);
  }

  p = get_cmdline_key (cmdline, "p2v.of");
  if (p) {
    free (config->output_format);
    config->output_format = strdup (p);
  }

  p = get_cmdline_key (cmdline, "p2v.os");
  if (p) {
    free (config->output_storage);
    config->output_storage = strdup (p);
  }

  /* Perform the conversion in text mode. */
  if (start_conversion (config, notify_ui_callback) == -1) {
    const char *err = get_conversion_error ();

    fprintf (stderr, "%s: error during conversion: %s\n",
             guestfs_int_program_name, err);
    exit (EXIT_FAILURE);
  }
}

static void
notify_ui_callback (int type, const char *data)
{
  switch (type) {
  case NOTIFY_LOG_DIR:
    printf ("%s: remote log directory location: %s\n", guestfs_int_program_name, data);
    break;

  case NOTIFY_REMOTE_MESSAGE:
    printf ("%s", data);
    break;

  case NOTIFY_STATUS:
    printf ("%s: %s\n", guestfs_int_program_name, data);
    break;

  default:
    printf ("%s: unknown message during conversion: type=%d data=%s\n",
            guestfs_int_program_name, type, data);
  }
}
