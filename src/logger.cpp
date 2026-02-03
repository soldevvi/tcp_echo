#include <condition_variable>
#include <fstream>
#include <string>
#include <vector>

struct LogEntry {
	uint64_t ts;
	int fd;
	uint64_t latency_ns;
	int active_clients;
	const char *tag; // "MSG", "CONN", "DISC"
};

class AsyncLogger {
  private:
	std::ofstream file;
	std::vector<LogEntry> front_buffer; // Буфер для записи из основного потока
	std::vector<LogEntry> back_buffer;	// Буфер для записи на диск
	std::mutex mtx;
	std::condition_variable cv;
	std::thread worker;
	std::atomic<bool> running{true};

	void work() {
		while (running || !front_buffer.empty()) {
			{
				std::unique_lock<std::mutex> lock(mtx);
				// Ждем, пока буфер наполнится или сервер завершит работу
				cv.wait_for(lock, std::chrono::milliseconds(500), [this] {
					return front_buffer.size() >= 1000 || !running;
				});

				// меняем векторы местами (константное время)
				front_buffer.swap(back_buffer);
			}

			if (!back_buffer.empty()) {
				for (const auto &e : back_buffer) {
					file << e.ts << "," << e.tag << "," << e.fd << ","
						 << e.latency_ns << "," << e.active_clients << "\n";
				}
				file.flush();
				back_buffer.clear();
			}
		}
	}

  public:
	AsyncLogger(const std::string &filename) {
		file.open(filename);
		file << "ts,tag,fd,latency_ns,active_clients\n";
		front_buffer.reserve(2000);
		back_buffer.reserve(2000);
		worker = std::thread(&AsyncLogger::work, this);
	}

	~AsyncLogger() { stop(); }

	void stop() {
		if (running) {
			running = false;
			cv.notify_one();
			if (worker.joinable())
				worker.join();
			file.close();
		}
	}

	void log(const char *tag, int fd, uint64_t latency, int active) {
		// push_back в память
		auto now = std::chrono::steady_clock::now().time_since_epoch().count();
		std::lock_guard<std::mutex> lock(mtx);
		front_buffer.push_back({(uint64_t)now, fd, latency, active, tag});

		// Будим поток записи, если накопили много данных
		if (front_buffer.size() >= 1000) {
			cv.notify_one();
		}
	}
};