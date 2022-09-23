/* virt-p2v
 * Copyright (C) 2009-2019 Red Hat Inc.
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

#ifndef P2V_H
#define P2V_H

#include <glib.h>

#include <stdio.h>
#include <stdbool.h>

/* Send various debug information to stderr.  Harmless and useful, so
 * can be left enabled in production builds.
 */
#define DEBUG_STDERR 1

#include "miniexpect.h"
#include "p2v-config.h"

#include "guestfs-utils.h"

#if defined(__GNUC__) && !defined(__clang__)
# define P2V_GCC_VERSION \
    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
# define P2V_GCC_VERSION 0
#endif

/* All network interfaces discovered when the program started.  Do not change
 * this.
 */
extern char **all_interfaces;

/* True if running inside the virt-p2v ISO environment.  Various
 * dangerous functions such as the "Reboot" button are disabled if
 * this is false.
 */
extern int is_iso_environment;

/* True if virt-v2v supports the --colours option. */
extern int feature_colours_option;

/* virt-p2v --colours option (used by ansi_* macros). */
extern int force_colour;

/* cpuid.c */
struct cpu_topo {
  unsigned sockets;
  unsigned cores;
  unsigned threads;
};
extern void get_cpu_topology (struct cpu_topo *topo);
extern void get_cpu_config (struct cpu_config *);

/* disks.c */
extern void find_all_disks (char ***disks, char ***removable);

/* rtc.c */
extern void get_rtc_config (struct rtc_config *);

/* kernel-cmdline.c */
extern char **parse_cmdline_string (const char *cmdline);
extern char **parse_proc_cmdline (void);
extern const char *get_cmdline_key (char **cmdline, const char *key);

#define CMDLINE_SOURCE_COMMAND_LINE 1 /* --cmdline */
#define CMDLINE_SOURCE_PROC_CMDLINE 2 /* /proc/cmdline */

/* kernel-config.c */
extern void update_config_from_kernel_cmdline (struct config *config, char **cmdline);

/* kernel.c */
extern void kernel_conversion (struct config *, char **cmdline, int cmdline_source);

/* gui.c */
extern void gui_conversion (struct config *config,
                            const char * const *disks,
                            const char * const *removable);

/* conversion.c */
struct data_conn {          /* Data per NBD connection / physical disk. */
  mexp_h *h;                /* miniexpect handle to ssh */
  pid_t nbd_pid;            /* NBD server PID */
  int nbd_remote_port;      /* remote NBD port on conversion server */
};

extern int start_conversion (struct config *, void (*notify_ui) (int type, const char *data));
#define NOTIFY_LOG_DIR        1  /* location of remote log directory */
#define NOTIFY_REMOTE_MESSAGE 2  /* log message from remote virt-v2v */
#define NOTIFY_STATUS         3  /* stage in conversion process */
extern const char *get_conversion_error (void);
extern void cancel_conversion (void);
extern int conversion_is_running (void);

/* physical-xml.c */
extern void generate_physical_xml (struct config *, struct data_conn *, const char *filename);

/* inhibit.c */
extern int inhibit_power_saving (void);

/* ssh.c */
extern int test_connection (struct config *);
extern mexp_h *open_data_connection (struct config *, int local_port, int *remote_port);
extern mexp_h *start_remote_connection (struct config *, const char *remote_dir);
extern const char *get_ssh_error (void);
extern int scp_file (struct config *config, const char *target, const char *local, ...) __attribute__((sentinel));

/* nbd.c */
extern void test_nbd_server (void);
extern pid_t start_nbd_server (int *port, const char *device);
const char *get_nbd_error (void);

/* utils.c */
extern uint64_t get_blockdev_size (const char *dev);
extern char *get_blockdev_model (const char *dev);
extern char *get_blockdev_serial (const char *dev);
extern char *get_if_addr (const char *if_name);
extern char *get_if_vendor (const char *if_name, int truncate);
extern void wait_network_online (const struct config *);
extern int compare_strings (const void *vp1, const void *vp2);

/* virt-v2v version and features (read from remote). */
extern char *v2v_version;

/* input and output drivers (read from remote). */
extern char **input_drivers;
extern char **output_drivers;

/* about-authors.c */
extern const char *authors[];
extern const char *qa[];
extern const char *documenters[];
extern const char *others[];

#endif /* P2V_H */
