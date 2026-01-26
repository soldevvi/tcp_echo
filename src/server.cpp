#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>

#include <format>

struct MessageHeader {
  uint32_t type;
  uint32_t size;
};

struct ClientInfo {
  int fd;
  time_t last_activ;
  char buff[1024];
  size_t recived;
  bool header_recived;
  MessageHeader header;
};

struct Stats {
  int active_clients = 0;
  int total_received = 0;
  int total_sent = 0;
  int errors = 0;
} stats;

int make_socket_non_block(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int close_client_connection(int epoll_fd, int client_fd,
                             std::unordered_map<int, ClientInfo> &clients) {
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
  close(client_fd);
  std::cout << std::format("Client disconnected: fd={}",
                           std::to_string(client_fd))
            << std::endl;
  auto it = clients.erase(client_fd);
  stats.active_clients--;
  return it;
}

int main(int argc, char **argv) {

  if (argc < 2) {
    std::cout << "Usage: server <port>" << std::endl;
    return 0;
  }

  int port = atoi(argv[1]);
  const uint16_t MAX_CLIENTS = 10000;

  // 1 socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Failed to create socket");
    return 1;
  }

  int opt = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // 2 bind

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Failed to bind socket");
    close(sock);
    return 1;
  }

  // 3- Listen

  int listen_backlog = 128;
  if (listen(sock, listen_backlog) != 0) {
    perror("Failed to listen on socket");
    close(sock);
    return 1;
  }

  // make socket non-blocking
  make_socket_non_block(sock);

  // 4 - epoll to monitor
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("Failed to create epoll instance");
    close(sock);
    return 1;
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = sock;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);

  std::unordered_map<int, ClientInfo> clients;
  struct epoll_event events[100];
  time_t last_stats = time(nullptr);

  std::cout << "Server listening on port " << port << std::endl;

  // Event loop

  while (true) {
    int n = epoll_wait(epoll_fd, events, 100, 1000);

    time_t now = time(nullptr);

    if (now - last_stats >= 1) {
      std::cout << "Active clients: " << stats.active_clients
                << ", Total received: " << stats.total_received
                << ", Total sent: " << stats.total_sent
                << ", Errors: " << stats.errors << std::endl;
      last_stats = now;
    }

    for (auto it = clients.begin(); it != clients.end();) {
      if (now - it->second.last_activ > 30) {
        std::cout << std::format("Timeout client fd: {}",
                                 std::to_string(it->first))
                  << std::endl;
        stats.errors++;
        int fd_to_close = it->first;

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it->first, nullptr);
        close(it->first);
        it = clients.erase(it);
        stats.active_clients--;
        stats.errors++;
        

      } else {
        ++it;
      }
    }

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if (fd == sock) {

        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int client_fd = accept(sock, (struct sockaddr *)&client_addr, &len);

        if (client_fd < 0) {
          break;
        }
        if (stats.active_clients >= MAX_CLIENTS) {
          std::cerr << "Max clients reached, rejecting new connection"
                    << std::endl;
          close(client_fd);
          stats.errors++;
          continue;
        }

        make_socket_non_block(client_fd);

        // add to epoll
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

        ClientInfo cl_info;
        cl_info.fd = client_fd;
        cl_info.last_activ = now;
        cl_info.recived = 0;
        cl_info.header_recived = false;

        clients[client_fd] = cl_info;
        stats.active_clients++;

        std::cout << std::format("New client connected: fd={}",
                                 std::to_string(client_fd))
                  << std::endl;
      } else {
        ClientInfo &cl_info = clients[fd];
        cl_info.last_activ = now;

        while (true) {
          ssize_t count = recv(fd, cl_info.buff + cl_info.recived,
                               sizeof(cl_info.buff) - cl_info.recived, 0);

          if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            perror("recv");
            stats.errors++;
            close_client_connection(epoll_fd, fd, clients);
            break;
          }
          if (count == 0) {
            close_client_connection(epoll_fd, fd, clients);
            break;
          }

          cl_info.recived += count;

          while (true) {
            if (!cl_info.header_recived) {
              if (cl_info.recived < sizeof(MessageHeader)) {
                break;
              }
              memcpy(&cl_info.header, cl_info.buff, sizeof(MessageHeader));
              cl_info.header_recived = true;
            }

            if (cl_info.header.size > 1016) {
              std::cerr << std::format("Message too large from client fd:{}",
                                       fd)
                        << std::endl;
              stats.errors++;
              close_client_connection(epoll_fd, fd, clients);
              goto next_event;
            }

            // wait for full message
            size_t msg_size = sizeof(MessageHeader) + cl_info.header.size;
            if (cl_info.recived < msg_size) {
              break;
            }

            // read full message
            ssize_t sent = send(fd, cl_info.buff, msg_size, 0);
            if (sent != (ssize_t)msg_size) {
              stats.errors++;
              close_client_connection(epoll_fd, fd, clients);
              goto next_event;
            }
            stats.total_received++;
            stats.total_sent++;

            // remove message from buffer
            size_t remaining = cl_info.recived - msg_size;
            cl_info.recived = remaining;
            memmove(cl_info.buff, cl_info.buff + msg_size, remaining);
            cl_info.header_recived = false;
          }
        }
      }
    next_event:;
    }
  };

  close(sock);
  close(epoll_fd);
  return 0;
}
