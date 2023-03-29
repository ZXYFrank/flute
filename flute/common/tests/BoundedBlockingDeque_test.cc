#include <flute/common/BoundedBlockingDeque.h>
#include <flute/common/LogFile.h>
#include <flute/common/CurrentThread.h>
#include <flute/common/Thread.h>

#include <vector>
#include "unistd.h"

static const int kNumThreads = 3;
flute::BoundedBlockingDeque<flute::Task> task_deque;

void work(int x) {
  LOG_INFO << flute::CurrentThread::name() << " is processing " << x;
  // sleep(1);
}

flute::Task task(int x) { return std::bind(&work, x); }

void io_thread_func() {
  static int g_count = 0;
  while (1) {
    // LOG_INFO << flute::CurrentThread::name() << "Push " << g_count;
    task_deque.push_back(task(g_count));
    g_count++;
    // sleep(1);
  }
}

void worker_thread_func() {
  while (1) {
    int id;
    flute::Task t = task_deque.take_front(&id);
    t();
    // sleep(1);
  }
}

int main() {
  // We will see that inside the deque, elements are polled out in order.
  // However, after being taken by different workers, the execution order of
  // the tasks may vary unpredictably.
  flute::LogLine::set_log_level(flute::LogLine::TRACE);
  std::vector<std::shared_ptr<flute::Thread>> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    std::shared_ptr<flute::Thread> t(
        new flute::Thread(worker_thread_func, "worker"));
    threads.push_back(t);
    t->start();
  }
  std::shared_ptr<flute::Thread> io(
      new flute::Thread(io_thread_func, "worker"));
  io->start();

  return 0;
}