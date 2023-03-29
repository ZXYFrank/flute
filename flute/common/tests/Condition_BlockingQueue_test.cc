#include <flute/common/Condition.h>
#include <flute/common/Thread.h>
#include <flute/common/LogLine.h>
#include <flute/common/CurrentThread.h>
#include <flute/common/common.h>

#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <queue>
#include <csignal>
#include <memory>

void sleep_sec(double seconds) {
  usleep(static_cast<int>(seconds * flute::kMicroSecondsPerSecond));
}

const int SEC = 10;

inline double my_rand() {
  return (static_cast<double>(rand()) / (RAND_MAX)) * SEC;
}

const int CAP = 50;
const int N_CONSUMERS = 10;
const int PRODUCER_SPEED = 5;
flute::MutexLock queue_mutex("queue_mutex");
flute::Condition not_full(queue_mutex);
flute::Condition not_empty(queue_mutex);
std::queue<int> my_queue;

void f_producer() {
  while (1) {
    double sec = my_rand() / (N_CONSUMERS * PRODUCER_SPEED);
    sleep_sec(sec);
    int x = static_cast<int>(my_rand() * 20);
    {
      flute::MutexLockGuard g(queue_mutex);
      while (!my_queue.empty() && my_queue.size() >= CAP) {
        not_full.wait();
      }
      // produce
      my_queue.push(x);
      LOG_INFO << flute::CurrentThread::name() << " Push " << x << " using "
               << flute::FormattedString("%.2f", sec).data() << " s."
               << " queue_size: " << my_queue.size();
      not_empty.notify();
      // LOG_TRACE << flute::CurrentThread::name() << " notifies not_empty";
    }
  }
}

void f_consumer() {
  while (1) {
    int x = 0;
    {
      flute::MutexLockGuard g(queue_mutex);
      while (my_queue.empty()) {
        not_empty.wait();
        // LOG_TRACE << flute::CurrentThread::name() << " got notified
        // not_empty";
      }
      x = my_queue.front();
      my_queue.pop();
      not_full.notify();
    }
    double sec = my_rand();
    sleep_sec(sec);
    LOG_INFO << flute::CurrentThread::name() << " Consume " << x << " using "
             << flute::FormattedString("%.2f", sec).data() << " s."
             << " queue_size " << my_queue.size();
    // LOG_TRACE << flute::CurrentThread::name() << " notifies not_full";
  }
}

void my_sigint(int sig) {
  printf("Exit");
  exit(0);
}

int main() {
  flute::LogLine::set_log_level(flute::LogLine::INFO);
  signal(SIGINT, my_sigint);
  srand(100);

  std::vector<std::unique_ptr<flute::Thread>> consumers;
  for (int i = 0; i < N_CONSUMERS; ++i) {
    ;
    consumers.emplace_back(new flute::Thread(f_consumer, "consumer"));
    consumers[i]->start();
  }
  flute::Thread producer(f_producer, "producer");
  producer.start();

  for (int i = 0; i < N_CONSUMERS; ++i) {
    consumers[i]->join();
  }
  producer.join();
  return 0;
}
