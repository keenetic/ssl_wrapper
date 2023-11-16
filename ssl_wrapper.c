// Copyright (c) 2014 Cesanta Software Limited
// All rights reserved
//
// This software is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation. For the terms of this
// license, see <http://www.gnu.org/licenses/>.
//
// You are free to use this software under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this software under a commercial
// license, as set out in <http://cesanta.com/products.html>.
//
// $Date$

#include "net_skeleton.h"
#include "ssl_wrapper.h"

#include <pwd.h>
#include <grp.h>

static void ev_handler(struct ns_connection *nc, enum ns_event ev, void *p) {
  const char *target_addr = (const char *) nc->mgr->user_data;
  struct ns_connection *pc = (struct ns_connection *) nc->connection_data;
  struct iobuf *io = &nc->recv_iobuf;

  (void) p;
  switch (ev) {
    case NS_ACCEPT:
      // Create a connection to the target, and interlink both connections
      nc->connection_data = ns_connect(nc->mgr, target_addr, nc);
      if (nc->connection_data == NULL) {
        nc->flags |= NSF_CLOSE_IMMEDIATELY;
      }
      break;

    case NS_CLOSE:
      // If either connection closes, unlink them and schedule closing
      if (pc != NULL) {
        pc->flags |= NSF_FINISHED_SENDING_DATA;
        pc->connection_data = NULL;
      }
      nc->connection_data = NULL;
      break;

    case NS_RECV:
      // Forward arrived data to the other connection, and discard from buffer
      if (pc != NULL) {
        ns_send(pc, io->buf, io->len);
        iobuf_remove(io, io->len);
      }
      break;

    default:
      break;
  }
}

void *ssl_wrapper_init(const char *local_addr, const char *target_addr,
                       const char **err_msg) {
  struct ns_mgr *mgr = (struct ns_mgr *) calloc(1, sizeof(mgr[0]));
  *err_msg = NULL;

  if (mgr == NULL) {
    *err_msg = "malloc failed";
  } else {
    ns_mgr_init(mgr, (void *) target_addr, ev_handler);
    if (ns_bind(mgr, local_addr, NULL) == NULL) {
      *err_msg = "ns_bind() failed: bad listening_port";
      ns_mgr_free(mgr);
      free(mgr);
      mgr = NULL;
    }
  }

  return mgr;
}

void ssl_wrapper_serve(void *param, volatile int *quit) {
  struct ns_mgr *mgr = (struct ns_mgr *) param;

  while (*quit == 0) {
    ns_mgr_poll(mgr, 1000);
  }
  ns_mgr_free(mgr);
  free(mgr);
}

#ifndef SSL_WRAPPER_USE_AS_LIBRARY
static int s_received_signal = 0;

static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler);
  s_received_signal = sig_num;
}

static void show_usage_and_exit(const char *prog) {
  fprintf(stderr, "Usage: %s <listening_address> <target_address>\n", prog);
  exit(EXIT_FAILURE);
}

static int drop_privileges(const char *user)
{
    if (geteuid() == 0) {
        struct group *grp;
        struct passwd *pwd;

        errno = 0;
        pwd = getpwnam(user);

        if (pwd == NULL) {
            fprintf(stderr, "Unable to get UID for user \"%s\": %s",
                user, strerror(errno));
            return 0;
        }

        errno = 0;
        grp = getgrnam(user);

        if (grp == NULL) {
            fprintf(stderr, "Unable to get GID for group \"%s\": %s",
                user, strerror(errno));
            return 0;
        }

        if (setgid(grp->gr_gid) == -1) {
            fprintf(stderr, "Unable to set new group \"%s\": %s",
                user, strerror(errno));
            return 0;
        }

        if (setuid(pwd->pw_uid) == -1) {
            fprintf(stderr, "Unable to set new user \"%s\": %s",
                user, strerror(errno));
            return 0;
        }
    }

    return 1;
}

int main(int argc, char *argv[]) {
  void *wrapper;
  const char *err_msg;

  if (argc != 4) {
    show_usage_and_exit(argv[0]);
  }

  // Setup signal handlers
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  if ((wrapper = ssl_wrapper_init(argv[1], argv[2], &err_msg)) == NULL) {
    fprintf(stderr, "Error: %s\n", err_msg);
    exit(EXIT_FAILURE);
  }

  if (!drop_privileges(argv[3])) {
      exit(EXIT_FAILURE);
  }

  ssl_wrapper_serve(wrapper, &s_received_signal);

  return EXIT_SUCCESS;
}
#endif // SSL_WRAPPER_USE_AS_LIBRARY
