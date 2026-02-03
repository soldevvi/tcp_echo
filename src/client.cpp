// client.cpp
#include "perf_mon.cpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

PerformanceMonitor client_pm;

struct MessageHeader {
  uint32_t type;
  uint32_t size;
};

void close_connection(int client_fd) {
  close(client_fd);
  client_pm.active_clients--;
}



// single client processing
void client_thread(const char *host, int port, int id) {

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    std::cerr << "[Client " << id << "] Failed to create socket\n";
    return;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, host, &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "[Client " << id << "] Failed to connect\n";
    close(sock);
    return;
  }
  auto conn_start = Clock::now();

  client_pm.active_clients++;
  
    // random gen
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> type_dist(0, 1000);
    std::uniform_int_distribution<> size_dist(0, 1016);
    std::uniform_int_distribution<> byte_dist(0, 255);

    MessageHeader header;
    header.type = type_dist(gen);
    header.size = size_dist(gen);

    // payload
    char buffer[1024];
    memcpy(buffer, &header, sizeof(header));
    for (uint32_t i = 0; i < header.size; i++) {
      buffer[sizeof(header) + i] = byte_dist(gen);
    }

    size_t total_size = sizeof(header) + header.size;

    auto start_rtt = Clock::now();

    // send message
    ssize_t sent = send(sock, buffer, total_size, 0);
    if (sent != (ssize_t)total_size) {
      std::cerr << "[Client " << id << "] Send failed\n";
      close(sock);
      return;
    }
    client_pm.total_sent++;

    // recv echo
    char recv_buffer[1024];
    size_t received = 0;
    while (received < total_size) {
      ssize_t n = recv(sock, recv_buffer + received, total_size - received, 0);
      if (n <= 0) {
        std::cerr << "[Client " << id << "] Recv failed\n";
        client_pm.total_errors++;
        close(sock);
        return;
      }
      received += n;
    }
    auto end_rtt = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end_rtt - start_rtt)
                        .count();
    client_pm.add_latency_sample(duration);
    client_pm.total_received++;

    // compare
    if (memcmp(buffer, recv_buffer, total_size) == 0) {
      // std::cout << "[Client " << id << "] SUCCESS (type=" << header.type
      //           << ", size=" << header.size << ")\n";
    } else {
      std::cerr << "[Client " << id << "] MISMATCH!\n";
      client_pm.total_errors++;
    }
  
  close(sock);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "Usage: client <host> <port> <num_connections>\n";
    return 0;
  }

  const char *host = argv[1];
  int port = atoi(argv[2]);
  int num = atoi(argv[3]);

  std::cout << "Starting " << num << " connections to " << host << ":" << port
            << std::endl;

  std::vector<std::thread> threads;
  for (int i = 0; i < num; i++) {
    threads.emplace_back(client_thread, host, port, i);

    if (i % 100 == 0 && i > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  for (auto &t : threads) {
    t.join();
  }
  client_pm.print_report("CLIENT_TEST");

  std::cout << "All clients finished\n";
  return 0;
}