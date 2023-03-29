#include <flute/common/TaskDeque.h>
#include <flute/common/LogLine.h>

#include <unistd.h>
#include <string>

inline double my_rand() { return (static_cast<double>(rand()) / (RAND_MAX)); }

void sleep_sec(double seconds) {
  usleep(static_cast<int>(seconds * flute::kMicroSecondsPerSecond));
}

void work(std::string s) {
  LOG_INFO << s;
  sleep_sec(my_rand() * 5);
}

int main() {
  flute::LogLine::set_log_level(flute::LogLine::INFO);
  flute::TaskDeque task_queue;
  task_queue.start(3);
  task_queue.set_max_deque_size(10);
  while (1) {
    if (my_rand() < 0.5) {
      task_queue.push_task_back(std::bind(&work, "back"));
    } else {
      task_queue.push_task_front(std::bind(&work, "front"));
    }
    LOG_INFO << "TaskDeque size=" << task_queue.size();
  }
  return 0;
}