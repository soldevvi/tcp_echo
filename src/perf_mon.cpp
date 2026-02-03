#include <atomic>
#include <format>
#include <iostream>
#include <string_view>

class PerformanceMonitor {
  public:
	// Счетчики
	std::atomic<uint64_t> total_received{0};
	std::atomic<uint64_t> total_sent{0};
	std::atomic<uint64_t> total_errors{0};
	std::atomic<int> active_clients{0};

	// Метрики задержек (Latency)
	std::atomic<uint64_t> total_latency_ns{0};
	std::atomic<uint64_t> max_latency_ns{0};
	std::atomic<uint64_t> latency_samples{0};

	// Метрики соединений
	std::atomic<uint64_t> total_conn_duration_ns{0};
	std::atomic<uint64_t> max_conn_duration_ns{0};
	std::atomic<uint64_t> conn_samples{0};

	std::atomic<uint8_t> vector_resizes{0};

	void add_latency_sample(uint64_t ns) {
		total_latency_ns += ns;
		latency_samples++;
		update_max(max_latency_ns, ns);
	}

	void add_connection_sample(uint64_t ns) {
		total_conn_duration_ns += ns;
		conn_samples++;
		update_max(max_conn_duration_ns, ns);
	}

	void print_report(std::string_view label) {
		uint64_t l_count = latency_samples.load();
		uint64_t c_count = conn_samples.load();

		double avg_lat =
			l_count > 0 ? (double)total_latency_ns / l_count / 1000.0 : 0;
		double max_lat = (double)max_latency_ns / 1000.0;

		double avg_conn =
			c_count > 0 ? (double)total_conn_duration_ns / c_count / 1e9 : 0;
		double max_conn = (double)max_conn_duration_ns / 1e9;

		std::cout << std::format(
			"\n--- [{}] REPORT ---\n"
			"Clients: Active={}\n"
			"Clients: Sent={}, Recv={}, Errors={}\n"
			"Latency (Processing/RTT): Avg={:.2f} us, Max={:.2f} us\n"
			"Conn Duration: Avg={:.2f} s, Max={:.2f} s\n"
			"CLient Vector resize(invalidation) count: Count={}\n"
			"----------------------------\n",
			label, active_clients.load(), total_sent.load(),
			total_received.load(), total_errors.load(), avg_lat, max_lat,
			avg_conn, max_conn, vector_resizes.load());
	}

  private:
	void update_max(std::atomic<uint64_t> &max_val, uint64_t new_val) {
		uint64_t current = max_val.load();
		while (new_val > current &&
			   !max_val.compare_exchange_weak(current, new_val))
			;
	}
};