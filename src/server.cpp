#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <tuple>
#include <unistd.h>
#include <unordered_map>

#include <format>
#include <utility>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

std::atomic<bool> server_running{true};

// Perfomance Parameters
const int MAX_ITERATION_SERVER_ACCEPT = 64;
const int MAX_ITERATION_CLIENT = 5;
const int MAX_EVENTS = 128;
const int BUF_SIZE = 1024;
const size_t MAX_MSG_SIZE = BUF_SIZE - sizeof(uint32_t) * 2;
const int MAX_CLIENTS = 10000;

struct MessageHeader {
  uint32_t type;
  uint32_t size;
};

struct ClientInfo {
  int fd;
  TimePoint last_activ;
  char buff[BUF_SIZE];
  size_t recived = 0;
  bool header_recived = false;
  MessageHeader header;

  ClientInfo(int f) : fd(f), last_activ(Clock::now()) {}
};

struct Stats {
  std::atomic<int> active_clients{0};
  std::atomic<int> total_received{0};
  std::atomic<int> total_sent{0};
  std::atomic<int> errors{0};
} stats;

// Globals
std::unordered_map<int, ClientInfo> clients;
std::mutex clients_mutex;

int make_socket_non_block(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void remove_client_locked(int epoll_fd, int client_fd) {
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
  close(client_fd);
  clients.erase(client_fd);
  stats.active_clients--;
}

void remove_client(int epoll_fd, int client_fd) {
  std::lock_guard<std::mutex> lock(clients_mutex);
  remove_client_locked(epoll_fd, client_fd);
}

void process_client_messages(int epoll_fd, ClientInfo &cl) {
  int count = 0;
  while (count <= MAX_ITERATION_CLIENT) {
    if (!cl.header_recived) {
      if (cl.recived < sizeof(MessageHeader))
        break;
      memcpy(&cl.header, cl.buff, sizeof(MessageHeader));
      cl.header_recived = true;
    }

    if (cl.header.size > MAX_MSG_SIZE) {
      std::cerr << "Message too large" << cl.fd << std::endl;
      remove_client(epoll_fd, cl.fd);
      return;
    }

    size_t full_msg_size = sizeof(MessageHeader) + cl.header.size;
    if (cl.recived < full_msg_size)
      break;

    // send back
    send(cl.fd, cl.buff, full_msg_size, 0);
    stats.total_received++;
    stats.total_sent++;

    size_t remaining = cl.recived - full_msg_size;
    if (remaining > 0) {
      memmove(cl.buff, cl.buff + full_msg_size, remaining);
    }
    cl.recived = remaining;
    cl.header_recived = false;

    count += 1;
  }
}

void handle_client_data(int epoll_fd, int fd, TimePoint now) {
  std::lock_guard<std::mutex> lock(clients_mutex);

  auto it = clients.find(fd);
  if (it == clients.end())
    return;

  ClientInfo &cl = it->second;
  cl.last_activ = now;

  // В режиме EPOLLET нужно читать до EAGAIN
  while (true) {
    ssize_t n = recv(fd, cl.buff + cl.recived, BUF_SIZE - cl.recived, 0);

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      stats.errors++;

      remove_client_locked(epoll_fd, fd);
      return;
    }

    if (n == 0) {

      remove_client_locked(epoll_fd, fd);
      return;
    }

    cl.recived += n;
    process_client_messages(epoll_fd, cl);

    if (cl.recived >= BUF_SIZE) {
      std::cerr << "Buffer overflow, closing " << fd << std::endl;

      remove_client_locked(epoll_fd, fd);
      return;
    }
  }
}
void handle_new_conn(int epoll_fd, int listen_sock) {
  while (true) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int client_fd = accept(listen_sock, (struct sockaddr *)&addr, &len);

    if (client_fd < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        perror("accept");
      break;
    }

    if (stats.active_clients >= MAX_CLIENTS) {
      close(client_fd);
      continue;
    }

    make_socket_non_block(client_fd);
    epoll_event ev;
    ev.events = EPOLLIN || EPOLLET;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

    {
      std::lock_guard<std::mutex> lock(clients_mutex);
      clients.emplace(std::piecewise_construct,
                      std::forward_as_tuple(client_fd),
                      std::forward_as_tuple(client_fd));
      stats.active_clients++;
    }
    std::cout << "New Client... " << client_fd << std::endl;
  }
}

void cleaner_thread(int epoll_fd) {
  while (server_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto now = Clock::now();
    std::vector<int> to_remove;

    {
      std::lock_guard<std::mutex> lock(clients_mutex);
      for (auto const &[fd, cl] : clients) {
        if (now - cl.last_activ > std::chrono::seconds(30)) {
          to_remove.push_back(fd);
        }
      }
    }

    for (int fd : to_remove) {
      std::cout << "Timeout: " << fd << std::endl;
      remove_client(epoll_fd, fd);
    }

    std::cout << std::format("Stats: Active={}, Sent={}, Recv={}, Errors={} \n",
                             stats.active_clients.load(),
                             stats.total_sent.load(),
                             stats.total_received.load(), stats.errors.load());
  }
}

int main(int argc, char **argv) {

  if (argc < 2) {
    std::cout << "Usage: server <port>" << std::endl;
    return 0;
  }

  int port = atoi(argv[1]);

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

  // bind listen sock to epoll
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = sock;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);

  std::thread cleaner(cleaner_thread, epoll_fd);

  std::cout << "Server listening on port " << port << std::endl;
  epoll_event events[MAX_EVENTS];

  // Event loop
  while (server_running) {
    int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
    auto now = Clock::now();

    // Iterate events (MAX_EVENTS)
    for (int i = 0; i < n; i++) {
      if (events[i].data.fd == sock) {
        handle_new_conn(epoll_fd, sock);
      } else {
        handle_client_data(epoll_fd, events[i].data.fd, now);
      }
    }
  }

  std::cout << "Shutting down server...";
  server_running = false;
  cleaner.join();

  for (auto &[fd, _] : clients) {
    close(fd);
  }

  close(sock);
  close(epoll_fd);
  return 0;
}
