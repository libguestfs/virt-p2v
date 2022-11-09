/* virt-p2v
 * Copyright (C) 2009-2022 Red Hat Inc.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
/* else it's in sys/types.h, included above */
#endif

#include "p2v.h"

/**
 * Get parent device of a partition.
 *
 * Returns C<0> if no parent device could be found.
 */
static dev_t
partition_parent (dev_t part_dev)
{
  CLEANUP_FCLOSE FILE *fp = NULL;
  CLEANUP_FREE char *path = NULL, *content = NULL;
  size_t len = 0;
  unsigned parent_major, parent_minor;

  if (asprintf (&path, "/sys/dev/block/%ju:%ju/../dev",
                (uintmax_t) major (part_dev),
                (uintmax_t) minor (part_dev)) == -1)
    error (EXIT_FAILURE, errno, "asprintf");

  fp = fopen (path, "r");
  if (fp == NULL)
    return 0;

  if (getline (&content, &len, fp) == -1)
    error (EXIT_FAILURE, errno, "getline");

  if (sscanf (content, "%u:%u", &parent_major, &parent_minor) != 2)
    return 0;

  return makedev (parent_major, parent_minor);
}

/**
 * Return true if the named device (eg. C<dev == "sda">) is a Removable Media
 * SCSI Disk with no media inserted. This covers floppy drives, but not CD-ROM
 * drives (intentionally).
 */
static int
device_has_no_media (const char *dev)
{
  int ret;
  gchar *sysfs_pathname;
  gchar *sysfs_contents;
  gsize sysfs_size;
  gchar *dev_pathname;
  int dev_fd;

  ret = 0;

  if (!STRPREFIX (dev, "sd"))
    return ret;

  sysfs_pathname = g_strdup_printf ("/sys/block/%s/removable", dev);

  if (!g_file_get_contents (sysfs_pathname, &sysfs_contents, &sysfs_size, NULL))
    goto free_sysfs_pathname;

  if (sysfs_size < 2 || sysfs_contents[0] != '1' || sysfs_contents[1] != '\n')
    goto free_sysfs_contents;

  dev_pathname = g_strdup_printf ("/dev/%s", dev);

  dev_fd = open (dev_pathname, O_RDONLY | O_CLOEXEC);
  if (dev_fd == -1) {
    if (errno == ENOMEDIUM)
      ret = 1;

    goto free_dev_pathname;
  }
  close (dev_fd);

free_dev_pathname:
  g_free (dev_pathname);

free_sysfs_contents:
  g_free (sysfs_contents);

free_sysfs_pathname:
  g_free (sysfs_pathname);

  return ret;
}

/**
 * Return true if the named device (eg. C<dev == "sda">) contains the
 * root filesystem.  C<root_device> is the major:minor of the root
 * filesystem (eg. C<8:1> if the root filesystem was F</dev/sda1>).
 *
 * This doesn't work for LVs and so on.  However we only really care
 * if this test works on the P2V ISO where the root device is a
 * regular partition.
 */
static int
device_contains (const char *dev, dev_t root_device)
{
  struct stat statbuf;
  CLEANUP_FREE char *dev_name = NULL;
  dev_t root_device_parent;

  if (asprintf (&dev_name, "/dev/%s", dev) == -1)
    error (EXIT_FAILURE, errno, "asprintf");

  if (stat (dev_name, &statbuf) == -1)
    return 0;

  /* See if dev is the root_device. */
  if (statbuf.st_rdev == root_device)
    return 1;

  /* See if dev is the parent device of the root_device. */
  root_device_parent = partition_parent (root_device);
  if (root_device_parent == 0)
    return 0;
  if (statbuf.st_rdev == root_device_parent)
    return 1;

  return 0;
}

/**
 * Enumerate all disks in F</sys/block> and return them in the C<disks> and
 * C<removable> arrays.
 */
void
find_all_disks (char ***disks, char ***removable)
{
  DIR *dir;
  struct dirent *d;
  size_t nr_disks = 0, nr_removable = 0;
  char **ret_disks = NULL, **ret_removable = NULL;
  dev_t root_device = 0;
  struct stat statbuf;

  if (stat ("/", &statbuf) == 0)
    root_device = statbuf.st_dev;

  /* The default list of disks is everything in /sys/block which
   * matches the common patterns for disk names.
   */
  dir = opendir ("/sys/block");
  if (!dir)
    error (EXIT_FAILURE, errno, "opendir");

  for (;;) {
    errno = 0;
    d = readdir (dir);
    if (!d) break;

    if (STRPREFIX (d->d_name, "cciss!") ||
        STRPREFIX (d->d_name, "hd") ||
        STRPREFIX (d->d_name, "nvme") ||
        STRPREFIX (d->d_name, "sd") ||
        STRPREFIX (d->d_name, "ubd") ||
        STRPREFIX (d->d_name, "vd")) {
      char *p;
      /* Skip SCSI disk drives with removable media that have no media inserted
       * -- effectively, empty floppy drives. Note that SCSI CD-ROMs are named
       * C<sr*> and thus handled on the other branch.
       */
      if (device_has_no_media (d->d_name))
        continue;

      /* Skip the device containing the root filesystem. */
      if (device_contains (d->d_name, root_device))
        continue;

      nr_disks++;
      ret_disks = realloc (ret_disks, sizeof (char *) * (nr_disks + 1));
      if (!ret_disks)
        error (EXIT_FAILURE, errno, "realloc");

      ret_disks[nr_disks-1] = strdup (d->d_name);

      /* cciss device /dev/cciss/c0d0 will be /sys/block/cciss!c0d0 */
      p = strchr (ret_disks[nr_disks-1], '!');
      if (p) *p = '/';

      ret_disks[nr_disks] = NULL;
    }
    else if (STRPREFIX (d->d_name, "sr")) {
      nr_removable++;
      ret_removable = realloc (ret_removable,
                               sizeof (char *) * (nr_removable + 1));
      if (!ret_removable)
        error (EXIT_FAILURE, errno, "realloc");
      ret_removable[nr_removable-1] = strdup (d->d_name);
      ret_removable[nr_removable] = NULL;
    }
  }

  /* Check readdir didn't fail */
  if (errno != 0)
    error (EXIT_FAILURE, errno, "readdir: %s", "/sys/block");

  /* Close the directory handle */
  if (closedir (dir) == -1)
    error (EXIT_FAILURE, errno, "closedir: %s", "/sys/block");

  if (ret_disks)
    qsort (ret_disks, nr_disks, sizeof (char *), compare_strings);
  if (ret_removable)
    qsort (ret_removable, nr_removable, sizeof (char *), compare_strings);

  *disks = ret_disks;
  *removable = ret_removable;
}
