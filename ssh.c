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

/**
 * This file handles the ssh connections to the conversion server.
 *
 * virt-p2v will open several connections over the lifetime of
 * the conversion process.
 *
 * In C<test_connection>, it will first open a connection (to check it
 * is possible) and query virt-v2v on the server to ensure it exists,
 * it is the right version, and so on.  This connection is then
 * closed, because in the GUI case we don't want to deal with keeping
 * it alive in case the administrator has set up an autologout.
 *
 * Once we start conversion, we will open a control connection to send
 * the libvirt configuration data and to start up virt-v2v, and we
 * will open up one data connection per local hard disk.  The data
 * connection(s) have a reverse port forward to the local NBD server
 * which is serving the content of that hard disk.  The remote port
 * for each data connection is assigned by ssh.  See
 * C<open_data_connection> and C<start_remote_conversion>.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <locale.h>
#include <assert.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "ignore-value.h"

#include "miniexpect.h"
#include "p2v.h"

#define SSH_TIMEOUT 60          /* seconds */

char *v2v_version = NULL;
char **input_drivers = NULL;
char **output_drivers = NULL;

static void free_globals (void) __attribute__((destructor));
static void
free_globals (void)
{
  pcre2_substring_free ((PCRE2_UCHAR *)v2v_version);
}

static char *ssh_error;

static void set_ssh_error (const char *fs, ...)
  __attribute__((format(printf,1,2)));

static void
set_ssh_error (const char *fs, ...)
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

  free (ssh_error);
  ssh_error = msg;
}

const char *
get_ssh_error (void)
{
  return ssh_error;
}

/* Like set_ssh_error, but for errors that aren't supposed to happen. */
#define set_ssh_internal_error(fs, ...)				   \
  set_ssh_error ("%s:%d: internal error: " fs, __FILE__, __LINE__, \
		 ##__VA_ARGS__)
#define set_ssh_mexp_error(fn) \
  set_ssh_internal_error ("%s: %m", fn)
#define set_ssh_pcre_error() \
  set_ssh_internal_error ("pcre error: %d\n", mexp_get_pcre_error (h))

#define set_ssh_unexpected_eof(fs, ...)                               \
  set_ssh_error ("remote server closed the connection unexpectedly, " \
                 "waiting for: " fs, ##__VA_ARGS__)
#define set_ssh_unexpected_timeout(fs, ...)               \
  set_ssh_error ("remote server timed out unexpectedly, " \
                 "waiting for: " fs, ##__VA_ARGS__)

static void compile_regexps (void) __attribute__((constructor));
static void free_regexps (void) __attribute__((destructor));

static pcre2_code *password_re;
static pcre2_code *ssh_message_re;
static pcre2_code *sudo_password_re;
static pcre2_code *prompt_re;
static pcre2_code *version_re;
static pcre2_code *feature_libguestfs_rewrite_re;
static pcre2_code *feature_colours_option_re;
static pcre2_code *feature_input_re;
static pcre2_code *feature_output_re;
static pcre2_code *portfwd_re;

static void
compile_regexps (void)
{
  int errorcode;
  PCRE2_SIZE offset;
  char errormsg[256];

#define COMPILE(re,pattern)                                             \
  do {                                                                  \
    re = pcre2_compile ((PCRE2_SPTR) (pattern),                         \
                        PCRE2_ZERO_TERMINATED,                          \
                        0, &errorcode, &offset, NULL);                  \
    if (re == NULL) {                                                   \
      pcre2_get_error_message (errorcode,                               \
                               (PCRE2_UCHAR *) errormsg, sizeof errormsg); \
      ignore_value (write (2, errormsg, strlen (errormsg)));            \
      abort ();                                                         \
    }                                                                   \
  } while (0)

  COMPILE (password_re, "password:");
  /* Note that (?:.)* is required in order to work around a problem
   * with partial matching and PCRE in RHEL 5.
   */
  COMPILE (ssh_message_re, "(ssh: (?:.)*)");
  COMPILE (sudo_password_re, "sudo: a password is required");
  /* The magic synchronization strings all match this expression.  See
   * start_ssh function below.
   */
  COMPILE (prompt_re,
	   "###((?:[0123456789abcdefghijklmnopqrstuvwxyz]){8})### ");
  /* Note that (?:.)* is required in order to work around a problem
   * with partial matching and PCRE in RHEL 5.
   */
  COMPILE (version_re, "virt-v2v ([1-9](?:.)*)");
  COMPILE (feature_libguestfs_rewrite_re, "libguestfs-rewrite");
  COMPILE (feature_colours_option_re, "colours-option");
  /* The input and output regexps must match the same pattern in
   * v2v/modules_list.ml.
   */
  COMPILE (feature_input_re, "input:((?:[-\\w])+)");
  COMPILE (feature_output_re, "output:((?:[-\\w])+)");
  COMPILE (portfwd_re, "Allocated port ((?:\\d)+) for remote forward");
}

static void
free_regexps (void)
{
  pcre2_code_free (password_re);
  pcre2_code_free (ssh_message_re);
  pcre2_code_free (sudo_password_re);
  pcre2_code_free (prompt_re);
  pcre2_code_free (version_re);
  pcre2_code_free (feature_libguestfs_rewrite_re);
  pcre2_code_free (feature_colours_option_re);
  pcre2_code_free (feature_input_re);
  pcre2_code_free (feature_output_re);
  pcre2_code_free (portfwd_re);
}

/**
 * Download URL to local file using the external 'curl' command.
 */
static int
curl_download (const char *url, const char *local_file)
{
  char curl_config_file[] = "/tmp/curl.XXXXXX";
  int fd;
  size_t i, len;
  FILE *fp;
  gboolean ret;
  const char *argv[] = { "curl", "-f", "-s", "-S", "-o", local_file,
                         "-K", curl_config_file, NULL };
  CLEANUP_FREE char *stderr = NULL;
  gint exit_status;
  GError *gerror = NULL;

  /* Use a secure curl config file because escaping is easier. */
  fd = mkstemp (curl_config_file);
  if (fd == -1)
    error (EXIT_FAILURE, errno, "mkstemp: %s", curl_config_file);
  fp = fdopen (fd, "w");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, "fdopen: %s", curl_config_file);
  fprintf (fp, "url = \"");
  len = strlen (url);
  for (i = 0; i < len; ++i) {
    switch (url[i]) {
    case '\\': fprintf (fp, "\\\\"); break;
    case '"':  fprintf (fp, "\\\""); break;
    case '\t': fprintf (fp, "\\t");  break;
    case '\n': fprintf (fp, "\\n");  break;
    case '\r': fprintf (fp, "\\r");  break;
    case '\v': fprintf (fp, "\\v");  break;
    default:   fputc (url[i], fp);
    }
  }
  fprintf (fp, "\"\n");
  fclose (fp);

  /* Run curl to download the URL to a file. */
  ret = g_spawn_sync (NULL, /* working directory; inherit */
                      (gchar **) argv,
                      NULL, /* environment; inherit */
                      G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL,
                      NULL, NULL, /* no child setup */
                      NULL, &stderr, &exit_status, &gerror);
  /* unlink (curl_config_file); - useful for debugging */
  if (!ret) {
    set_ssh_error ("g_spawn_sync: %s: %s", url, gerror->message);
    g_error_free (gerror);
    return -1;
  }

  /* Did curl subprocess fail? */
  if (WIFEXITED (exit_status) && WEXITSTATUS (exit_status) != 0) {
    set_ssh_error ("%s: %s", url, stderr);
    return -1;
  }
  else if (!WIFEXITED (exit_status)) {
    set_ssh_internal_error ("curl subprocess got a signal (%d)", exit_status);
    return -1;
  }

  return 0;
}

/**
 * Re-cache the C<config-E<gt>identity.url> if needed.
 */
static int
cache_ssh_identity (struct config *config)
{
  int fd;

  /* If it doesn't need downloading, return. */
  if (config->auth.identity.url == NULL ||
      !config->auth.identity.file_needs_update)
    return 0;

  /* Generate a random filename. */
  free (config->auth.identity.file);
  config->auth.identity.file = strdup ("/tmp/id.XXXXXX");
  if (config->auth.identity.file == NULL)
    error (EXIT_FAILURE, errno, "strdup");
  fd = mkstemp (config->auth.identity.file);
  if (fd == -1)
    error (EXIT_FAILURE, errno, "mkstemp");
  close (fd);

  /* Curl download URL to file. */
  if (curl_download (config->auth.identity.url, config->auth.identity.file) == -1) {
    free (config->auth.identity.file);
    config->auth.identity.file = NULL;
    config->auth.identity.file_needs_update = 1;
    return -1;
  }

  return 0;
}

/* GCC complains about the argv array in the next function which it
 * thinks might grow to an unbounded size.  Since we control
 * extra_args, this is not in fact a problem.
 */
#if P2V_GCC_VERSION >= 40800 /* gcc >= 4.8.0 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
#endif

/**
 * Start ssh subprocess with the standard arguments and possibly some
 * optional arguments.  Also handles authentication.
 */
static mexp_h *
start_ssh (unsigned spawn_flags, struct config *config,
           char **extra_args, int wait_prompt)
{
  size_t i = 0;
  const size_t MAX_ARGS =
    64 + (extra_args != NULL ? guestfs_int_count_strings (extra_args) : 0);
  const char *argv[MAX_ARGS];
  char port_str[64];
  char connect_timeout_str[128];
  mexp_h *h;
  CLEANUP_PCRE2_MATCH_DATA pcre2_match_data *match_data =
    pcre2_match_data_create (4, NULL);
  int saved_timeout;
  int using_password_auth;
  size_t count;

  if (cache_ssh_identity (config) == -1)
    return NULL;

  /* Are we using password or identity authentication? */
  using_password_auth = config->auth.identity.file == NULL;

  ADD_ARG (argv, i, "ssh");
  ADD_ARG (argv, i, "-p");      /* Port. */
  snprintf (port_str, sizeof port_str, "%d", config->remote.port);
  ADD_ARG (argv, i, port_str);
  ADD_ARG (argv, i, "-l");      /* Username. */
  ADD_ARG (argv, i, config->auth.username ? config->auth.username : "root");
  ADD_ARG (argv, i, "-o");      /* Host key will always be novel. */
  ADD_ARG (argv, i, "StrictHostKeyChecking=no");
  ADD_ARG (argv, i, "-o");      /* ConnectTimeout */
  snprintf (connect_timeout_str, sizeof connect_timeout_str,
            "ConnectTimeout=%d", SSH_TIMEOUT);
  ADD_ARG (argv, i, connect_timeout_str);
  ADD_ARG (argv, i, "-o");      /* Send ping packets every 5 mins to sshd. */
  ADD_ARG (argv, i, "ServerAliveInterval=300");
  ADD_ARG (argv, i, "-o");
  ADD_ARG (argv, i, "ServerAliveCountMax=6");
  if (using_password_auth) {
    /* Only use password authentication. */
    ADD_ARG (argv, i, "-o");
    ADD_ARG (argv, i, "PreferredAuthentications=keyboard-interactive,password");
  }
  else {
    /* Use identity file (private key). */
    ADD_ARG (argv, i, "-o");
    ADD_ARG (argv, i, "PreferredAuthentications=publickey");
    ADD_ARG (argv, i, "-i");
    ADD_ARG (argv, i, config->auth.identity.file);
  }
  if (extra_args != NULL) {
    for (size_t j = 0; extra_args[j] != NULL; ++j)
      ADD_ARG (argv, i, extra_args[j]);
  }
  ADD_ARG (argv, i, config->remote.server); /* Conversion server. */
  ADD_ARG (argv, i, NULL);

#if DEBUG_STDERR
  fputs ("ssh command: ", stderr);
  for (i = 0; argv[i] != NULL; ++i) {
    if (i > 0) fputc (' ', stderr);
    fputs (argv[i], stderr);
  }
  fputc ('\n', stderr);
#endif

  /* Create the miniexpect handle. */
  h = mexp_spawnvf (spawn_flags, "ssh", (char **) argv);
  if (h == NULL) {
    set_ssh_internal_error ("ssh: mexp_spawnvf: %m");
    return NULL;
  }
#if DEBUG_STDERR
  mexp_set_debug_file (h, stderr);
#endif

  /* We want the ssh ConnectTimeout to be less than the miniexpect
   * timeout, so that if the server is completely unresponsive we
   * still see the error from ssh, not a timeout from miniexpect.  The
   * obvious solution to this is to set ConnectTimeout (above) and to
   * set the miniexpect timeout to be a little bit larger.
   */
  mexp_set_timeout (h, SSH_TIMEOUT + 20);

  if (using_password_auth &&
      config->auth.password && strlen (config->auth.password) > 0) {
    CLEANUP_PCRE2_SUBSTRING_FREE PCRE2_UCHAR *ssh_message = NULL;
    PCRE2_SIZE ssh_msglen;

    /* Wait for the password prompt. */
  wait_password_again:
    switch (mexp_expect (h,
                         (mexp_regexp[]) {
                           { 100, .re = password_re },
                           { 101, .re = ssh_message_re },
                           { 0 }
                         }, match_data)) {
    case 100:                   /* Got password prompt. */
      if (mexp_printf_password (h, "%s", config->auth.password) == -1 ||
          mexp_printf (h, "\n") == -1) {
        set_ssh_mexp_error ("mexp_printf");
        mexp_close (h);
        return NULL;
      }
      break;

    case 101:
      pcre2_substring_free (ssh_message);
      pcre2_substring_get_bynumber (match_data, 1, &ssh_message, &ssh_msglen);
      goto wait_password_again;

    case MEXP_EOF:
      /* This is where we get to if the user enters an incorrect or
       * impossible hostname or port number.  Hopefully ssh printed an
       * error message, and we picked it up and put it in
       * 'ssh_message' in case 101 above.  If not we have to print a
       * generic error instead.
       */
      if (ssh_message)
        set_ssh_error ("%s", (char *) ssh_message);
      else
        set_ssh_error ("ssh closed the connection without printing an error.");
      mexp_close (h);
      return NULL;

    case MEXP_TIMEOUT:
      set_ssh_unexpected_timeout ("password prompt");
      mexp_close (h);
      return NULL;

    case MEXP_ERROR:
      set_ssh_mexp_error ("mexp_expect");
      mexp_close (h);
      return NULL;

    case MEXP_PCRE_ERROR:
      set_ssh_pcre_error ();
      mexp_close (h);
      return NULL;
    }
  }

  if (!wait_prompt)
    return h;

  /* Ensure we are running bash, set environment variables, and
   * synchronize with the command prompt and set it to a known
   * string.  There are multiple issues being solved here:
   *
   * We cannot control the initial shell prompt.  It would involve
   * changing the remote SSH configuration (AcceptEnv).  However what
   * we can do is to repeatedly send 'export PS1=<magic>' commands
   * until we synchronize with the remote shell.
   *
   * Since we parse error messages, we must set LANG=C.
   *
   * We don't know if the user is using a Bourne-like shell (eg sh,
   * bash) or csh/tcsh.  Setting environment variables works
   * differently.
   *
   * We don't know how command line editing is set up
   * (https://bugzilla.redhat.com/1314244#c9).
   */
  if (mexp_printf (h, "exec bash --noediting --noprofile --norc\n") == -1) {
    set_ssh_mexp_error ("mexp_printf");
    mexp_close (h);
    return NULL;
  }

  saved_timeout = mexp_get_timeout_ms (h);
  mexp_set_timeout (h, 2);

  for (count = 0; count < 30; ++count) {
    char magic[9];
    PCRE2_UCHAR *matched;
    PCRE2_SIZE matchlen;
    int r;

    if (guestfs_int_random_string (magic, 8) == -1) {
      set_ssh_internal_error ("random_string: %m");
      mexp_close (h);
      return NULL;
    }

    /* The purpose of the '' inside the string is to ensure we don't
     * mistake the command echo for the prompt.
     */
    if (mexp_printf (h, "export LANG=C PS1='###''%s''### '\n", magic) == -1) {
      set_ssh_mexp_error ("mexp_printf");
      mexp_close (h);
      return NULL;
    }

    /* Wait for the prompt. */
  wait_again:
    switch (mexp_expect (h,
                         (mexp_regexp[]) {
                           { 100, .re = password_re },
                           { 101, .re = prompt_re },
                           { 0 }
                         }, match_data)) {
    case 100:                    /* Got password prompt unexpectedly. */
      set_ssh_error ("Login failed.  Probably the username and/or password is wrong.");
      mexp_close (h);
      return NULL;

    case 101:
      /* Got a prompt.  However it might be an earlier prompt.  If it
       * doesn't match the PS1 string we sent, then repeat the expect.
       */
      r = pcre2_substring_get_bynumber (match_data, 1, &matched, &matchlen);
      if (r < 0)
        error (EXIT_FAILURE, 0, "pcre error reading substring (%d)", r);
      r = STREQ (magic, (char *) matched);
      pcre2_substring_free (matched);
      if (!r)
        goto wait_again;
      goto got_prompt;

    case MEXP_EOF:
      set_ssh_unexpected_eof ("the command prompt");
      mexp_close (h);
      return NULL;

    case MEXP_TIMEOUT:
      /* Timeout here is not an error, since ssh may "eat" commands that
       * we send before the shell at the other end is ready.  Just loop.
       */
      break;

    case MEXP_ERROR:
      set_ssh_mexp_error ("mexp_expect");
      mexp_close (h);
      return NULL;

    case MEXP_PCRE_ERROR:
      set_ssh_pcre_error ();
      mexp_close (h);
      return NULL;
    }
  }

  set_ssh_error ("Failed to synchronize with remote shell after 60 seconds.");
  mexp_close (h);
  return NULL;

 got_prompt:
  mexp_set_timeout_ms (h, saved_timeout);

  return h;
}

#if P2V_GCC_VERSION >= 40800 /* gcc >= 4.8.0 */
#pragma GCC diagnostic pop
#endif

/**
 * Upload file(s) to remote using L<scp(1)>.
 *
 * Note that the target (directory or file) comes before the list of
 * local files, because the list of local files is a varargs list.
 *
 * This is a simplified version of L</start_ssh> above.
 */
int
scp_file (struct config *config, const char *target, const char *local, ...)
{
  size_t i = 0;
  va_list args;
  const size_t MAX_ARGS = 64;
  const char *argv[MAX_ARGS];
  char port_str[64];
  char connect_timeout_str[128];
  CLEANUP_FREE char *remote = NULL;
  mexp_h *h;
  CLEANUP_PCRE2_MATCH_DATA pcre2_match_data *match_data =
    pcre2_match_data_create (4, NULL);
  int using_password_auth;

  if (cache_ssh_identity (config) == -1)
    return -1;

  /* Are we using password or identity authentication? */
  using_password_auth = config->auth.identity.file == NULL;

  ADD_ARG (argv, i, "scp");
  ADD_ARG (argv, i, "-P");      /* Port. */
  snprintf (port_str, sizeof port_str, "%d", config->remote.port);
  ADD_ARG (argv, i, port_str);
  ADD_ARG (argv, i, "-o");      /* Host key will always be novel. */
  ADD_ARG (argv, i, "StrictHostKeyChecking=no");
  ADD_ARG (argv, i, "-o");      /* ConnectTimeout */
  snprintf (connect_timeout_str, sizeof connect_timeout_str,
            "ConnectTimeout=%d", SSH_TIMEOUT);
  ADD_ARG (argv, i, connect_timeout_str);
  if (using_password_auth) {
    /* Only use password authentication. */
    ADD_ARG (argv, i, "-o");
    ADD_ARG (argv, i, "PreferredAuthentications=keyboard-interactive,password");
  }
  else {
    /* Use identity file (private key). */
    ADD_ARG (argv, i, "-o");
    ADD_ARG (argv, i, "PreferredAuthentications=publickey");
    ADD_ARG (argv, i, "-i");
    ADD_ARG (argv, i, config->auth.identity.file);
  }

  /* Source files or directories.
   * Strictly speaking this could abort() if the list of files is
   * too long, but that never happens in virt-p2v. XXX
   */
  va_start (args, local);
  do ADD_ARG (argv, i, local);
  while ((local = va_arg (args, const char *)) != NULL);
  va_end (args);

  /* The target file or directory.  We need to rewrite this as
   * "username@server:target".
   */
  if (asprintf (&remote, "%s@%s:%s",
                config->auth.username ? config->auth.username : "root",
                config->remote.server, target) == -1)
    error (EXIT_FAILURE, errno, "asprintf");
  ADD_ARG (argv, i, remote);

  ADD_ARG (argv, i, NULL);

#if DEBUG_STDERR
  fputs ("scp command: ", stderr);
  for (i = 0; argv[i] != NULL; ++i) {
    if (i > 0) fputc (' ', stderr);
    fputs (argv[i], stderr);
  }
  fputc ('\n', stderr);
#endif

  /* Create the miniexpect handle. */
  h = mexp_spawnv ("scp", (char **) argv);
  if (h == NULL) {
    set_ssh_internal_error ("scp: mexp_spawnv: %m");
    return -1;
  }
#if DEBUG_STDERR
  mexp_set_debug_file (h, stderr);
#endif

  /* We want the ssh ConnectTimeout to be less than the miniexpect
   * timeout, so that if the server is completely unresponsive we
   * still see the error from ssh, not a timeout from miniexpect.  The
   * obvious solution to this is to set ConnectTimeout (above) and to
   * set the miniexpect timeout to be a little bit larger.
   */
  mexp_set_timeout (h, SSH_TIMEOUT + 20);

  if (using_password_auth &&
      config->auth.password && strlen (config->auth.password) > 0) {
    CLEANUP_PCRE2_SUBSTRING_FREE PCRE2_UCHAR *ssh_message = NULL;
    PCRE2_SIZE ssh_msglen;

    /* Wait for the password prompt. */
  wait_password_again:
    switch (mexp_expect (h,
                         (mexp_regexp[]) {
                           { 100, .re = password_re },
                           { 101, .re = ssh_message_re },
                           { 0 }
                         }, match_data)) {
    case 100:                   /* Got password prompt. */
      if (mexp_printf_password (h, "%s", config->auth.password) == -1 ||
          mexp_printf (h, "\n") == -1) {
        set_ssh_mexp_error ("mexp_printf");
        mexp_close (h);
        return -1;
      }
      break;

    case 101:
      pcre2_substring_free (ssh_message);
      pcre2_substring_get_bynumber (match_data, 1, &ssh_message, &ssh_msglen);
      goto wait_password_again;

    case MEXP_EOF:
      /* This is where we get to if the user enters an incorrect or
       * impossible hostname or port number.  Hopefully scp printed an
       * error message, and we picked it up and put it in
       * 'ssh_message' in case 101 above.  If not we have to print a
       * generic error instead.
       */
      if (ssh_message)
        set_ssh_error ("%s", (char *) ssh_message);
      else
        set_ssh_error ("scp closed the connection without printing an error.");
      mexp_close (h);
      return -1;

    case MEXP_TIMEOUT:
      set_ssh_unexpected_timeout ("password prompt");
      mexp_close (h);
      return -1;

    case MEXP_ERROR:
      set_ssh_mexp_error ("mexp_expect");
      mexp_close (h);
      return -1;

    case MEXP_PCRE_ERROR:
      set_ssh_pcre_error ();
      mexp_close (h);
      return -1;
    }
  }

  /* Wait for the scp subprocess to finish. */
  switch (mexp_expect (h, NULL, NULL)) {
  case MEXP_EOF:
    break;

  case MEXP_TIMEOUT:
    set_ssh_unexpected_timeout ("copying (scp) file");
    mexp_close (h);
    return -1;

  case MEXP_ERROR:
    set_ssh_mexp_error ("mexp_expect");
    mexp_close (h);
    return -1;

  case MEXP_PCRE_ERROR:
    set_ssh_pcre_error ();
    mexp_close (h);
    return -1;
  }

  if (mexp_close (h) == -1) {
    set_ssh_internal_error ("scp: mexp_close: %m");
    return -1;
  }

  return 0;
}

static void add_input_driver (const char *name);
static void add_output_driver (const char *name);
static int compatible_version (const char *v2v_version_p);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn" /* WTF? */
#endif
int
test_connection (struct config *config)
{
  mexp_h *h;
  int feature_libguestfs_rewrite = 0;
  int status;
  CLEANUP_PCRE2_MATCH_DATA pcre2_match_data *match_data =
    pcre2_match_data_create (4, NULL);
  PCRE2_SIZE verlen;

  h = start_ssh (0, config, NULL, 1);
  if (h == NULL)
    return -1;

  /* Clear any previous version information since we may be connecting
   * to a different server.
   */
  pcre2_substring_free ((PCRE2_UCHAR *)v2v_version);
  v2v_version = NULL;

  /* Send 'virt-v2v --version' command and hope we get back a version string.
   * Note old virt-v2v did not understand -V option.
   */
  if (mexp_printf (h,
                   "%svirt-v2v --version\n",
                   config->auth.sudo ? "sudo -n " : "") == -1) {
    set_ssh_mexp_error ("mexp_printf");
    mexp_close (h);
    return -1;
  }

  for (;;) {
    switch (mexp_expect (h,
                         (mexp_regexp[]) {
                           { 100, .re = version_re },
                           { 101, .re = sudo_password_re },
                           { 102, .re = prompt_re },
                           { 0 }
                         }, match_data)) {
    case 100:                   /* Got version string. */
      pcre2_substring_free ((PCRE2_UCHAR *)v2v_version);
      pcre2_substring_get_bynumber (match_data, 1,
                                    (PCRE2_UCHAR **) &v2v_version, &verlen);
#if DEBUG_STDERR
      fprintf (stderr, "%s: remote virt-v2v version: %s\n",
               g_get_prgname (), v2v_version);
#endif
      break;

    case 101:
      set_ssh_error ("sudo for user \"%s\" requires a password.  Edit /etc/sudoers on the conversion server to ensure the \"NOPASSWD:\" option is set for this user.",
                     config->auth.username);
      mexp_close (h);
      return -1;

    case 102:             /* Got the prompt. */
      goto end_of_version;

    case MEXP_EOF:
      set_ssh_unexpected_eof ("\"virt-v2v --version\" output");
      mexp_close (h);
      return -1;

    case MEXP_TIMEOUT:
      set_ssh_unexpected_timeout ("\"virt-v2v --version\" output");
      mexp_close (h);
      return -1;

    case MEXP_ERROR:
      set_ssh_mexp_error ("mexp_expect");
      mexp_close (h);
      return -1;

    case MEXP_PCRE_ERROR:
      set_ssh_pcre_error ();
      mexp_close (h);
      return -1;
    }
  }
 end_of_version:

  /* Got the prompt but no version number. */
  if (v2v_version == NULL) {
    set_ssh_error ("virt-v2v is not installed on the conversion server, "
                   "or it might be a too old version.");
    mexp_close (h);
    return -1;
  }

  /* Check the version of virt-v2v is compatible with virt-p2v. */
  if (!compatible_version (v2v_version)) {
    mexp_close (h);
    return -1;
  }

  /* Clear any previous driver information since we may be connecting
   * to a different server.
   */
  guestfs_int_free_string_list (input_drivers);
  guestfs_int_free_string_list (output_drivers);
  input_drivers = output_drivers = NULL;

  /* Get virt-v2v features.  See: v2v/cmdline.ml */
  if (mexp_printf (h, "%svirt-v2v --machine-readable\n",
                   config->auth.sudo ? "sudo -n " : "") == -1) {
    set_ssh_mexp_error ("mexp_printf");
    mexp_close (h);
    return -1;
  }

  for (;;) {
    PCRE2_UCHAR *driver;
    PCRE2_SIZE drvrlen;

    switch (mexp_expect (h,
                         (mexp_regexp[]) {
                           { 100, .re = feature_libguestfs_rewrite_re },
                           { 101, .re = feature_colours_option_re },
                           { 102, .re = feature_input_re },
                           { 103, .re = feature_output_re },
                           { 104, .re = prompt_re },
                           { 0 }
                         }, match_data)) {
    case 100:                   /* libguestfs-rewrite. */
      feature_libguestfs_rewrite = 1;
      break;

    case 101:                   /* virt-v2v supports --colours option */
#if DEBUG_STDERR
  fprintf (stderr, "%s: remote virt-v2v supports --colours option\n",
           g_get_prgname ());
#endif
      feature_colours_option = 1;
      break;

    case 102:
      /* input:<driver-name> corresponds to an -i option in virt-v2v. */
      pcre2_substring_get_bynumber (match_data, 1, &driver, &drvrlen);
      add_input_driver ((char *) driver);
      pcre2_substring_free (driver);
      break;

    case 103:
      /* output:<driver-name> corresponds to an -o option in virt-v2v. */
      pcre2_substring_get_bynumber (match_data, 1, &driver, &drvrlen);
      add_output_driver ((char *) driver);
      pcre2_substring_free (driver);
      break;

    case 104:                   /* Got prompt, so end of output. */
      goto end_of_machine_readable;

    case MEXP_EOF:
      set_ssh_unexpected_eof ("\"virt-v2v --machine-readable\" output");
      mexp_close (h);
      return -1;

    case MEXP_TIMEOUT:
      set_ssh_unexpected_timeout ("\"virt-v2v --machine-readable\" output");
      mexp_close (h);
      return -1;

    case MEXP_ERROR:
      set_ssh_mexp_error ("mexp_expect");
      mexp_close (h);
      return -1;

    case MEXP_PCRE_ERROR:
      set_ssh_pcre_error ();
      mexp_close (h);
      return -1;
    }
  }
 end_of_machine_readable:

  if (!feature_libguestfs_rewrite) {
    set_ssh_error ("Invalid output of \"virt-v2v --machine-readable\" command.");
    mexp_close (h);
    return -1;
  }

  /* Test finished, shut down ssh. */
  if (mexp_printf (h, "exit\n") == -1) {
    set_ssh_mexp_error ("mexp_printf");
    mexp_close (h);
    return -1;
  }

  switch (mexp_expect (h, NULL, NULL)) {
  case MEXP_EOF:
    break;

  case MEXP_TIMEOUT:
    set_ssh_unexpected_timeout ("end of ssh session");
    mexp_close (h);
    return -1;

  case MEXP_ERROR:
    set_ssh_mexp_error ("mexp_expect");
    mexp_close (h);
    return -1;

  case MEXP_PCRE_ERROR:
    set_ssh_pcre_error ();
    mexp_close (h);
    return -1;
  }

  status = mexp_close (h);
  if (status == -1) {
    set_ssh_internal_error ("mexp_close: %m");
    return -1;
  }
  if (WIFSIGNALED (status) && WTERMSIG (status) == SIGHUP)
    return 0; /* not an error */
  if (!WIFEXITED (status) || WEXITSTATUS (status) != 0) {
    set_ssh_internal_error ("unexpected close status from ssh subprocess (%d)",
                            status);
    return -1;
  }
  return 0;
}

static void
add_option (const char *type, char ***drivers, const char *name)
{
  size_t n;

  if (*drivers == NULL)
    n = 0;
  else
    n = guestfs_int_count_strings (*drivers);

  n++;

  *drivers = realloc (*drivers, (n+1) * sizeof (char *));
  if (*drivers == NULL)
    error (EXIT_FAILURE, errno, "malloc");

  (*drivers)[n-1] = strdup (name);
  if ((*drivers)[n-1] == NULL)
    error (EXIT_FAILURE, errno, "strdup");
  (*drivers)[n] = NULL;

#if DEBUG_STDERR
  fprintf (stderr, "%s: remote virt-v2v supports %s driver %s\n",
           g_get_prgname (), type, (*drivers)[n-1]);
#endif
}

static void
add_input_driver (const char *name)
{
  add_option ("input", &input_drivers, name);
}

static void
add_output_driver (const char *name)
{
  /* Ignore the 'vdsm' driver, since that should only be used by VDSM.
   * Ignore the 'openstack' and 'rhv-upload' drivers, since we do not
   * support passing all the options for them.
   */
  if (STRNEQ (name, "vdsm") &&
      STRNEQ (name, "openstack") &&
      STRNEQ (name, "rhv-upload"))
    add_option ("output", &output_drivers, name);
}

static int
compatible_version (const char *v2v_version_p)
{
  unsigned v2v_major, v2v_minor;

  /* The major version must always be 1 or 2. */
  if (!STRPREFIX (v2v_version_p, "1.") && !STRPREFIX (v2v_version_p, "2.")) {
    set_ssh_error ("virt-v2v major version is neither 1 nor 2 (\"%s\"), "
                   "this version of virt-p2v is not compatible.",
                   v2v_version_p);
    return 0;
  }

  /* The version of virt-v2v must be >= 1.28, just to make sure
   * someone isn't (a) using one of the experimental 1.27 releases
   * that we published during development, nor (b) using old virt-v2v.
   * We should remain compatible with any virt-v2v after 1.28.
   */
  if (sscanf (v2v_version_p, "%u.%u", &v2v_major, &v2v_minor) != 2) {
    set_ssh_internal_error ("cannot parse virt-v2v version string (\"%s\")",
                            v2v_version_p);
    return 0;
  }

  if (v2v_major == 1 && v2v_minor < 28) {
    set_ssh_error ("virt-v2v version is < 1.28 (\"%s\"), "
                   "you must upgrade to virt-v2v >= 1.28 on "
                   "the conversion server.", v2v_version_p);
    return 0;
  }

  return 1;                     /* compatible */
}

mexp_h *
open_data_connection (struct config *config, int local_port, int *remote_port)
{
  mexp_h *h;
  char remote_arg[32];
  const char *extra_args[] = {
    "-R", remote_arg,
    "-N",
    NULL
  };
  PCRE2_UCHAR *port_str;
  PCRE2_SIZE portlen;
  CLEANUP_PCRE2_MATCH_DATA pcre2_match_data *match_data =
    pcre2_match_data_create (4, NULL);

  snprintf (remote_arg, sizeof remote_arg, "0:localhost:%d", local_port);

  h = start_ssh (0, config, (char **) extra_args, 0);
  if (h == NULL)
    return NULL;

  switch (mexp_expect (h,
                       (mexp_regexp[]) {
                         { 100, .re = portfwd_re },
                         { 0 }
                       }, match_data)) {
  case 100:                     /* Ephemeral port. */
    pcre2_substring_get_bynumber (match_data, 1, &port_str, &portlen);
    if (port_str == NULL) {
      set_ssh_internal_error ("strndup: %m");
      mexp_close (h);
      return NULL;
    }
    if (sscanf ((char *) port_str, "%d", remote_port) != 1) {
      set_ssh_internal_error ("cannot extract the port number from '%s'",
                              port_str);
      pcre2_substring_free (port_str);
      mexp_close (h);
      return NULL;
    }
    pcre2_substring_free (port_str);
    break;

  case MEXP_EOF:
    set_ssh_unexpected_eof ("\"ssh -R\" output");
    mexp_close (h);
    return NULL;

  case MEXP_TIMEOUT:
    set_ssh_unexpected_timeout ("\"ssh -R\" output");
    mexp_close (h);
    return NULL;

  case MEXP_ERROR:
    set_ssh_mexp_error ("mexp_expect");
    mexp_close (h);
    return NULL;

  case MEXP_PCRE_ERROR:
    set_ssh_pcre_error ();
    mexp_close (h);
    return NULL;
  }

  return h;
}

/* Wait for the prompt. */
static int
wait_for_prompt (mexp_h *h)
{
  CLEANUP_PCRE2_MATCH_DATA pcre2_match_data *match_data =
    pcre2_match_data_create (4, NULL);

  switch (mexp_expect (h,
                       (mexp_regexp[]) {
                         { 100, .re = prompt_re },
                         { 0 }
                       }, match_data)) {
  case 100:                     /* Got the prompt. */
    return 0;

  case MEXP_EOF:
    set_ssh_unexpected_eof ("command prompt");
    return -1;

  case MEXP_TIMEOUT:
    set_ssh_unexpected_timeout ("command prompt");
    return -1;

  case MEXP_ERROR:
    set_ssh_mexp_error ("mexp_expect");
    return -1;

  case MEXP_PCRE_ERROR:
    set_ssh_pcre_error ();
    return -1;
  }

  return 0;
}

mexp_h *
start_remote_connection (struct config *config, const char *remote_dir)
{
  mexp_h *h;

  /* This connection is opened in cooked mode so that we can send ^C
   * if the conversion needs to be cancelled.  However that also means
   * we must be careful not to accidentally send any control
   * characters over this connection at other times.
   */
  h = start_ssh (MEXP_SPAWN_COOKED_MODE, config, NULL, 1);
  if (h == NULL)
    return NULL;

  /* Create the remote directory. */
  if (mexp_printf (h, "mkdir %s\n", remote_dir) == -1) {
    set_ssh_mexp_error ("mexp_printf");
    goto error;
  }

  if (wait_for_prompt (h) == -1)
    goto error;

  /* It's simplest to create the remote 'time' file by running the date
   * command, as that won't send any special control characters.
   */
  if (mexp_printf (h, "date > %s/time\n", remote_dir) == -1) {
    set_ssh_mexp_error ("mexp_printf");
    goto error;
  }

  if (wait_for_prompt (h) == -1)
    goto error;

  return h;

 error:
  mexp_close (h);
  return NULL;
}
