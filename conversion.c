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

/**
 * This file manages the p2v conversion.
 *
 * The conversion is actually done by L<virt-v2v(1)> running on the
 * remote conversion server.  This file manages running the remote
 * command and provides callbacks for displaying the output.
 *
 * When virt-p2v operates in GUI mode, this code runs in a separate
 * thread.  When virt-p2v operates in kernel mode, this runs
 * synchronously in the main thread.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <error.h>
#include <locale.h>
#include <libintl.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <pthread.h>

#include "ignore-value.h"

#include "miniexpect.h"
#include "p2v.h"

static void cleanup_data_conns (struct data_conn *data_conns, size_t nr);
static void generate_name (struct config *, const char *filename);
static void generate_wrapper_script (struct config *, const char *remote_dir, const char *filename);
static void generate_system_data (const char *dmesg_file, const char *lscpu_file, const char *lspci_file, const char *lsscsi_file, const char *lsusb_file);
static void generate_p2v_version_file (const char *p2v_version_file);
static void print_quoted (FILE *fp, const char *s);

static char *conversion_error;

static void set_conversion_error (const char *fs, ...)
  __attribute__((format(printf,1,2)));

static void
set_conversion_error (const char *fs, ...)
{
  va_list args;
  char *msg;
  int len;

  va_start (args, fs);
  len = vasprintf (&msg, fs, args);
  va_end (args);

  if (len < 0)
    error (EXIT_FAILURE, errno,
           "vasprintf (original error format string: %s)", fs);

  free (conversion_error);
  conversion_error = msg;
}

const char *
get_conversion_error (void)
{
  return conversion_error;
}

static pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
static int running = 0;
static pthread_mutex_t cancel_requested_mutex = PTHREAD_MUTEX_INITIALIZER;
static int cancel_requested = 0;
static mexp_h *control_h = NULL;

static int
is_running (void)
{
  int r;
  pthread_mutex_lock (&running_mutex);
  r = running;
  pthread_mutex_unlock (&running_mutex);
  return r;
}

static void
set_running (int r)
{
  pthread_mutex_lock (&running_mutex);
  running = r;
  pthread_mutex_unlock (&running_mutex);
}

static int
is_cancel_requested (void)
{
  int r;
  pthread_mutex_lock (&cancel_requested_mutex);
  r = cancel_requested;
  pthread_mutex_unlock (&cancel_requested_mutex);
  return r;
}

static void
set_cancel_requested (int r)
{
  pthread_mutex_lock (&cancel_requested_mutex);
  cancel_requested = r;

  /* Send ^C to the remote so that virt-v2v "knows" the connection has
   * been cancelled.  mexp_send_interrupt is a single write(2) call.
   */
  if (r && control_h)
    ignore_value (mexp_send_interrupt (control_h));

  pthread_mutex_unlock (&cancel_requested_mutex);
}

static void
set_control_h (mexp_h *new_h)
{
  pthread_mutex_lock (&cancel_requested_mutex);
  control_h = new_h;
  pthread_mutex_unlock (&cancel_requested_mutex);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#endif
int
start_conversion (struct config *config,
                  void (*notify_ui) (int type, const char *data))
{
  int ret = -1;
  int status;
  size_t i, len;
  const size_t nr_disks = guestfs_int_count_strings (config->disks);
  time_t now;
  struct tm tm;
  CLEANUP_FREE struct data_conn *data_conns = NULL;
  CLEANUP_FREE char *remote_dir = NULL;
  char tmpdir[]           = "/tmp/p2v.XXXXXX";
  char name_file[]        = "/tmp/p2v.XXXXXX/name";
  char physical_xml_file[] = "/tmp/p2v.XXXXXX/physical.xml";
  char wrapper_script[]   = "/tmp/p2v.XXXXXX/virt-v2v-wrapper.sh";
  char dmesg_file[]       = "/tmp/p2v.XXXXXX/dmesg";
  char lscpu_file[]       = "/tmp/p2v.XXXXXX/lscpu";
  char lspci_file[]       = "/tmp/p2v.XXXXXX/lspci";
  char lsscsi_file[]      = "/tmp/p2v.XXXXXX/lsscsi";
  char lsusb_file[]       = "/tmp/p2v.XXXXXX/lsusb";
  char p2v_version_file[] = "/tmp/p2v.XXXXXX/p2v-version";
  int inhibit_fd = -1;

#if DEBUG_STDERR
  print_config (config, stderr);
  fprintf (stderr, "\n");
#endif

  set_control_h (NULL);
  set_running (1);
  set_cancel_requested (0);

  inhibit_fd = inhibit_power_saving ();
#ifdef DEBUG_STDERR
  if (inhibit_fd == -1)
    fprintf (stderr, "warning: virt-p2v cannot inhibit power saving during conversion.\n");
#endif

  data_conns = malloc (sizeof (struct data_conn) * nr_disks);
  if (data_conns == NULL)
    error (EXIT_FAILURE, errno, "malloc");

  for (i = 0; config->disks[i] != NULL; ++i) {
    data_conns[i].h = NULL;
    data_conns[i].nbd_pid = 0;
    data_conns[i].nbd_remote_port = -1;
  }

  /* Start the data connections and NBD server processes, one per disk. */
  for (i = 0; config->disks[i] != NULL; ++i) {
    int nbd_local_port;
    CLEANUP_FREE char *device = NULL;

    if (config->disks[i][0] == '/') {
      device = strdup (config->disks[i]);
      if (!device) {
        perror ("strdup");
        cleanup_data_conns (data_conns, nr_disks);
        exit (EXIT_FAILURE);
      }
    }
    else if (asprintf (&device, "/dev/%s", config->disks[i]) == -1) {
      perror ("asprintf");
      cleanup_data_conns (data_conns, nr_disks);
      exit (EXIT_FAILURE);
    }

    if (notify_ui) {
      CLEANUP_FREE char *msg;
      if (asprintf (&msg,
                    _("Starting local NBD server for %s ..."),
                    config->disks[i]) == -1)
        error (EXIT_FAILURE, errno, "asprintf");
      notify_ui (NOTIFY_STATUS, msg);
    }

    /* Start NBD server listening on the given port number. */
    data_conns[i].nbd_pid = start_nbd_server (&nbd_local_port, device);
    if (data_conns[i].nbd_pid == 0) {
      set_conversion_error ("NBD server error: %s", get_nbd_error ());
      goto out;
    }

    if (notify_ui) {
      CLEANUP_FREE char *msg;
      if (asprintf (&msg,
                    _("Opening data connection for %s ..."),
                    config->disks[i]) == -1)
        error (EXIT_FAILURE, errno, "asprintf");
      notify_ui (NOTIFY_STATUS, msg);
    }

    /* Open the SSH data connection, with reverse port forwarding
     * back to the NBD server.
     */
    data_conns[i].h = open_data_connection (config, nbd_local_port,
                                            &data_conns[i].nbd_remote_port);
    if (data_conns[i].h == NULL) {
      const char *err = get_ssh_error ();

      set_conversion_error ("could not open data connection over SSH to the conversion server: %s", err);
      goto out;
    }

#if DEBUG_STDERR
    fprintf (stderr,
             "%s: data connection for %s: SSH remote port %d, local port %d\n",
             g_get_prgname (), device,
             data_conns[i].nbd_remote_port,
             nbd_local_port);
#endif
  }

  /* Create a remote directory name which will be used for libvirt
   * XML, log files and other stuff.  We don't delete this directory
   * after the run because (a) it's useful for debugging and (b) it
   * only contains small files.
   *
   * NB: This path MUST NOT require shell quoting.
   */
  time (&now);
  gmtime_r (&now, &tm);
  if (asprintf (&remote_dir,
                "/tmp/virt-p2v-%04d%02d%02d-XXXXXXXX",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday) == -1) {
    perror ("asprintf");
    cleanup_data_conns (data_conns, nr_disks);
    exit (EXIT_FAILURE);
  }
  len = strlen (remote_dir);
  guestfs_int_random_string (&remote_dir[len-8], 8);
  if (notify_ui)
    notify_ui (NOTIFY_LOG_DIR, remote_dir);

  /* Generate the local temporary directory. */
  if (mkdtemp (tmpdir) == NULL) {
    perror ("mkdtemp");
    cleanup_data_conns (data_conns, nr_disks);
    exit (EXIT_FAILURE);
  }
  memcpy (name_file, tmpdir, strlen (tmpdir));
  memcpy (physical_xml_file, tmpdir, strlen (tmpdir));
  memcpy (wrapper_script, tmpdir, strlen (tmpdir));
  memcpy (dmesg_file, tmpdir, strlen (tmpdir));
  memcpy (lscpu_file, tmpdir, strlen (tmpdir));
  memcpy (lspci_file, tmpdir, strlen (tmpdir));
  memcpy (lsscsi_file, tmpdir, strlen (tmpdir));
  memcpy (lsusb_file, tmpdir, strlen (tmpdir));
  memcpy (p2v_version_file, tmpdir, strlen (tmpdir));

  /* Generate the static files. */
  generate_name (config, name_file);
  generate_physical_xml (config, data_conns, physical_xml_file);
  generate_wrapper_script (config, remote_dir, wrapper_script);
  generate_system_data (dmesg_file,
                        lscpu_file, lspci_file, lsscsi_file, lsusb_file);
  generate_p2v_version_file (p2v_version_file);

  /* Open the control connection.  This also creates remote_dir. */
  if (notify_ui)
    notify_ui (NOTIFY_STATUS, _("Setting up the control connection ..."));

  set_control_h (start_remote_connection (config, remote_dir));
  if (control_h == NULL) {
    set_conversion_error ("could not open control connection over SSH to the conversion server: %s",
                          get_ssh_error ());
    goto out;
  }

  /* Copy the static files to the remote dir. */

  /* These three files must not fail, so check for errors here. */
  if (scp_file (config, remote_dir,
                name_file, physical_xml_file, wrapper_script, NULL) == -1) {
    set_conversion_error ("scp: %s: %s",
                          remote_dir, get_ssh_error ());
    goto out;
  }

  /* It's not essential that these files are copied, so ignore errors. */
  ignore_value (scp_file (config, remote_dir,
                          dmesg_file, lscpu_file, lspci_file, lsscsi_file,
                          lsusb_file, p2v_version_file, NULL));

  /* Do the conversion.  This runs until virt-v2v exits. */
  if (notify_ui)
    notify_ui (NOTIFY_STATUS, _("Doing conversion ..."));

  if (mexp_printf (control_h,
                   /* To simplify things in the wrapper script, it
                    * writes virt-v2v's exit status to
                    * /remote_dir/status, and here we read that and
                    * exit the ssh shell with the same status.
                    */
                   "%s/virt-v2v-wrapper.sh; "
                   "exit $(< %s/status)\n",
                   remote_dir, remote_dir) == -1) {
    set_conversion_error ("mexp_printf: virt-v2v: %m");
    goto out;
  }

  /* Read output from the virt-v2v process and echo it through the
   * notify function, until virt-v2v closes the connection.
   */
  while (!is_cancel_requested ()) {
    char buf[257];
    ssize_t r;

    r = read (mexp_get_fd (control_h), buf, sizeof buf - 1);
    if (r == -1) {
      /* See comment about this in miniexpect.c. */
      if (errno == EIO)
        break;                  /* EOF */
      set_conversion_error ("read: %m");
      goto out;
    }
    if (r == 0)
      break;                    /* EOF */
    buf[r] = '\0';
    if (notify_ui)
      notify_ui (NOTIFY_REMOTE_MESSAGE, buf);
  }

  if (is_cancel_requested ()) {
    set_conversion_error ("cancelled by user");
    if (notify_ui)
      notify_ui (NOTIFY_STATUS, _("Conversion cancelled by user."));
    goto out;
  }

  if (notify_ui)
    notify_ui (NOTIFY_STATUS, _("Control connection closed by remote."));

  ret = 0;
 out:
  if (control_h) {
    mexp_h *h = control_h;
    set_control_h (NULL);
    status = mexp_close (h);

    if (status == -1) {
      set_conversion_error ("mexp_close: %m");
      ret = -1;
    }
    else if (ret == 0 &&
             WIFEXITED (status) &&
             WEXITSTATUS (status) != 0) {
      set_conversion_error ("virt-v2v exited with status %d",
                            WEXITSTATUS (status));
      ret = -1;
    }
  }
  cleanup_data_conns (data_conns, nr_disks);

  if (inhibit_fd >= 0)
    close (inhibit_fd);

  set_running (0);

  return ret;
}

int
conversion_is_running (void)
{
  return is_running ();
}

void
cancel_conversion (void)
{
  set_cancel_requested (1);
}

static void
cleanup_data_conns (struct data_conn *data_conns, size_t nr)
{
  size_t i;

  for (i = 0; i < nr; ++i) {
    if (data_conns[i].h != NULL) {
      /* Because there is no SSH prompt (ssh -N), the only way to kill
       * these ssh connections is to send a signal.  Just closing the
       * pipe doesn't do anything.
       */
      kill (mexp_get_pid (data_conns[i].h), SIGHUP);
      mexp_close (data_conns[i].h);
    }

    if (data_conns[i].nbd_pid > 0) {
      /* Kill NBD process and clean up. */
      kill (data_conns[i].nbd_pid, SIGTERM);
      waitpid (data_conns[i].nbd_pid, NULL, 0);
    }
  }
}

/**
 * Write the guest name into C<filename>.
 */
static void
generate_name (struct config *config, const char *filename)
{
  FILE *fp;

  fp = fopen (filename, "w");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, "fopen: %s", filename);
  fprintf (fp, "%s\n", config->guestname);
  fclose (fp);
}

/**
 * Construct the virt-v2v wrapper script.
 *
 * This will be sent to the remote server, and is easier than trying
 * to "type" a long and complex single command line into the ssh
 * connection when we start the conversion.
 */
static void
generate_wrapper_script (struct config *config, const char *remote_dir,
                         const char *filename)
{
  FILE *fp;

  fp = fopen (filename, "w");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, "fopen: %s", filename);

  fprintf (fp, "#!/bin/bash -\n");
  fprintf (fp, "\n");

  fprintf (fp, "cd %s\n", remote_dir);
  fprintf (fp, "\n");

  /* The virt-v2v command, as a shell function called "v2v". */
  fprintf (fp, "v2v ()\n");
  fprintf (fp, "{\n");
  if (config->auth.sudo)
    fprintf (fp, "sudo -n ");
  fprintf (fp, "virt-v2v -v -x");
  if (feature_colours_option)
    fprintf (fp, " --colours");
  fprintf (fp, " -i libvirtxml");

  if (config->output.type) {         /* -o */
    fprintf (fp, " -o ");
    print_quoted (fp, config->output.type);
  }

  switch (config->output.allocation) { /* -oa */
  case OUTPUT_ALLOCATION_NONE:
    /* nothing */
    break;
  case OUTPUT_ALLOCATION_SPARSE:
    fprintf (fp, " -oa sparse");
    break;
  case OUTPUT_ALLOCATION_PREALLOCATED:
    fprintf (fp, " -oa preallocated");
    break;
  default:
    abort ();
  }

  if (config->output.format) {  /* -of */
    fprintf (fp, " -of ");
    print_quoted (fp, config->output.format);
  }

  if (config->output.storage) { /* -os */
    fprintf (fp, " -os ");
    print_quoted (fp, config->output.storage);
  }

  if (config->output.misc) { /* -oo */
    size_t i;

    for (i = 0; config->output.misc[i]; ++i) {
      fprintf (fp, " -oo ");
      print_quoted (fp, config->output.misc[i]);
    }
  }

  fprintf (fp, " --root first");
  fprintf (fp, " physical.xml");
  fprintf (fp, " </dev/null");  /* no stdin */
  fprintf (fp, "\n");
  fprintf (fp,
           "# Save the exit code of virt-v2v into the 'status' file.\n");
  fprintf (fp, "echo $? > status\n");
  fprintf (fp, "}\n");
  fprintf (fp, "\n");

  fprintf (fp,
           "# Write a pre-emptive error status, in case the virt-v2v\n"
           "# command doesn't get to run at all.  This will be\n"
           "# overwritten with the true exit code when virt-v2v runs.\n");
  fprintf (fp, "echo 99 > status\n");
  fprintf (fp, "\n");

  fprintf (fp, "log=virt-v2v-conversion-log.txt\n");
  fprintf (fp, "rm -f $log\n");
  fprintf (fp, "\n");

  fprintf (fp,
           "# Log the environment where virt-v2v will run.\n");
  fprintf (fp, "printenv > environment\n");
  fprintf (fp, "\n");

  fprintf (fp,
           "# Log the version of virt-v2v (for information only).\n");
  if (config->auth.sudo)
    fprintf (fp, "sudo -n ");
  fprintf (fp, "virt-v2v --version > v2v-version\n");
  fprintf (fp, "\n");

  fprintf (fp,
           "# Run virt-v2v.  Send stdout back to virt-p2v.  Send stdout\n"
           "# and stderr (debugging info) to the log file.\n");
  fprintf (fp, "v2v 2>> $log | tee -a $log\n");
  fprintf (fp, "\n");

  fprintf (fp,
           "# If virt-v2v failed then the error message (sent to stderr)\n"
           "# will not be seen in virt-p2v.  Send the last few lines of\n"
           "# the log back to virt-p2v in this case.\n");
  fprintf (fp,
           "if [ \"$(< status)\" -ne 0 ]; then\n"
           "    echo\n"
           "    echo\n"
           "    echo\n"
           "    echo -ne '\\e[1;31m'\n"
           "    echo '***' virt-v2v command failed '***'\n"
           "    echo\n"
           "    echo The full log is available on the conversion server in:\n"
           "    echo '   ' %s/$log\n"
           "    echo Only the last 50 lines are shown below.\n"
           "    echo -ne '\\e[0m'\n"
           "    echo\n"
           "    echo\n"
           "    echo\n"
           "    tail -50 $log\n"
           "fi\n",
           remote_dir);

  fprintf (fp, "\n");
  fprintf (fp, "# EOF\n");
  fclose (fp);

  if (chmod (filename, 0755) == -1)
    error (EXIT_FAILURE, errno, "chmod: %s", filename);
}

/**
 * Print a shell-quoted string on C<fp>.
 */
static void
print_quoted (FILE *fp, const char *s)
{
  fprintf (fp, "\"");
  while (*s) {
    if (*s == '$' || *s == '`' || *s == '\\' || *s == '"')
      fprintf (fp, "\\");
    fprintf (fp, "%c", *s);
    ++s;
  }
  fprintf (fp, "\"");
}

/**
 * Collect data about the system running virt-p2v such as the dmesg
 * output and lists of PCI devices.  This is useful for diagnosis when
 * things go wrong.
 *
 * If any command fails, this is non-fatal.
 */
static void
generate_system_data (const char *dmesg_file,
                      const char *lscpu_file,
                      const char *lspci_file,
                      const char *lsscsi_file,
                      const char *lsusb_file)
{
  CLEANUP_FREE char *cmd = NULL;

  if (asprintf (&cmd,
                "dmesg >%s 2>&1; "
                "lscpu >%s 2>&1; "
                "lspci -vvv >%s 2>&1; "
                "lsscsi -v >%s 2>&1; "
                "lsusb -v >%s 2>&1",
                dmesg_file, lscpu_file, lspci_file, lsscsi_file, lsusb_file)
      == -1)
    error (EXIT_FAILURE, errno, "asprintf");

  ignore_value (system (cmd));
}

/**
 * Generate a file containing the version of virt-p2v.
 *
 * The version of virt-v2v is contained in the conversion log.
 */
static void
generate_p2v_version_file (const char *p2v_version_file)
{
  FILE *fp = fopen (p2v_version_file, "w");
  if (fp == NULL) {
    perror (p2v_version_file);
    return;                     /* non-fatal */
  }
  fprintf (fp, "%s %s\n",
           g_get_prgname (), PACKAGE_VERSION_FULL);
  fclose (fp);
}
