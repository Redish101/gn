// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>

#include "gn/commands.h"
#include "gn/setup.h"
#include "gn/standard_out.h"

namespace commands {

namespace {

class RedirectStdoutAndStderr {
 public:
  RedirectStdoutAndStderr(int out_fd, int err_fd)
      : prev_out_fd_(dup(STDOUT_FILENO)), prev_err_fd_(dup(STDERR_FILENO)) {
    dup2(out_fd, STDOUT_FILENO);
    dup2(err_fd, STDERR_FILENO);
  }

  ~RedirectStdoutAndStderr() {
    dup2(prev_out_fd_, STDOUT_FILENO);
    dup2(prev_err_fd_, STDERR_FILENO);
  }

 private:
  int prev_out_fd_;
  int prev_err_fd_;
};

std::vector<std::string> SplitArgs(const char* args, int len) {
  std::vector<std::string> str_args;
  std::string arg;
  for (int i = 0; i < len; i++) {
    if (args[i] == '\0') {
      str_args.push_back(arg);
      arg = "";
      continue;
    }
    arg += args[i];
  }
  return str_args;
}

void HandleClientRequest(int client_fd, Setup* setup) {
  if (client_fd == -1) {
    Err(Location(),
        std::string("Failed to accept client connection: ") + strerror(errno))
        .PrintToStdout();
    return;
  }

  struct msghdr msgh;
  msgh.msg_name = NULL;
  msgh.msg_namelen = 0;

  const size_t kBufSize = 4096;
  struct args_data {
    size_t len;
    // args, separated by \0
    char buf[kBufSize];
  } data;
  struct iovec iov;
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;
  iov.iov_base = &data;
  iov.iov_len = sizeof(data);

  union {
    char buf[CMSG_SPACE(2 * sizeof(int))];
    struct cmsghdr align;
  } control_msg;
  // Set 'msgh' fields to describe the ancillary data buffer.
  msgh.msg_control = control_msg.buf;
  msgh.msg_controllen = sizeof(control_msg.buf);

  if (recvmsg(client_fd, &msgh, 0) == -1) {
    Err(Location(),
        std::string("Failed to receive args from client: ") + strerror(errno))
        .PrintToStdout();
    return;
  }

  struct cmsghdr* cmsgp = CMSG_FIRSTHDR(&msgh);
  if (cmsgp == NULL || cmsgp->cmsg_len != CMSG_LEN(2 * sizeof(int)) ||
      cmsgp->cmsg_level != SOL_SOCKET || cmsgp->cmsg_type != SCM_RIGHTS) {
    Err(Location(), "Bad cmsg header").PrintToStdout();
    return;
  }

  int client_stdout_fd = ((int*)CMSG_DATA(cmsgp))[0];
  int client_stderr_fd = ((int*)CMSG_DATA(cmsgp))[1];
  RedirectStdoutAndStderr tmp_redirect(client_stdout_fd, client_stderr_fd);

  std::vector<std::string> str_args = SplitArgs(data.buf, data.len);
  if (str_args[0] == "desc") {
    if (RunDesc(str_args, setup) != 0) {
      Err(Location(), "Failed to run desc").PrintToStdout();
      return;
    }
  } else {
    Err(Location(), "Unsupported query command: " + str_args[0])
        .PrintToStdout();
    return;
  }
}

int StartServerSocket() {
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd == -1) {
    Err(Location(),
        std::string("Failed to create server socket: ") + strerror(errno))
        .PrintToStdout();
    return -1;
  }

  struct sockaddr_un saddr = {};
  saddr.sun_family = AF_UNIX;
  memcpy(saddr.sun_path, kServerSockPath, sizeof(kServerSockPath));
  if (bind(server_fd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
    Err(Location(), std::string("Failed to bind server socket to path ") +
                        kServerSockPath + ": " + strerror(errno))
        .PrintToStdout();
    return -1;
  }

  if (listen(server_fd, 10) == -1) {
    Err(Location(),
        std::string("Failed to listen with server socket at path ") +
            kServerSockPath + ": " + strerror(errno))
        .PrintToStdout();
    return -1;
  }

  return server_fd;
}

int StartServerLoop(int server_fd, Setup* setup) {
  OutputString("Server listening...\n");

  int client_fd;
  while (true) {
    client_fd = accept(server_fd, NULL, NULL);
    HandleClientRequest(client_fd, setup);
    if (close(client_fd) == -1) {
      // Report close failures but don't exit server.
      Err(Location(),
          std::string("Failed to close client connection: ") + strerror(errno))
          .PrintToStdout();
    }
  }

  return 0;
}
}  // namespace

const char kStartServer[] = "start_server";
const char kStartServer_HelpShort[] = "start_server short help TBA";
const char kStartServer_Help[] = "start_server help TBA";

int RunStartServer(const std::vector<std::string>& args) {
  printf("Loading GN files...\n");
  Setup* setup = new Setup;
  if (!setup->DoSetup(args[0], false)) {
    return 1;
  }
  if (!setup->Run()) {
    return 1;
  }

  int server_fd = StartServerSocket();
  if (server_fd == -1) {
    return 1;
  }
  return StartServerLoop(server_fd, setup);
}
}  // namespace commands
