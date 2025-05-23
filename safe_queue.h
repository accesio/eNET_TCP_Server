#pragma once
//https://stackoverflow.com/questions/15278343/c11-thread-safe-queue //2020 05 28

#include <signal.h>
#include <queue>
#include <mutex>
#include <condition_variable>

extern volatile sig_atomic_t done;
// A threadsafe-queue.
template <class T>
class SafeQueue
{
public:
  SafeQueue(void)
    : q()
    , m()
    , c()
  { stop = false;}

  ~SafeQueue(void)
  {}

  // Add an element to the queue.
  void enqueue(T t)
  {
    std::lock_guard<std::mutex> lock(m);
    q.push(t);
    c.notify_one();
  }

  // Get the "front"-element.
  // If the queue is empty, wait till a element is available.
  T dequeue(void)
  {
    std::unique_lock<std::mutex> lock(m);
    while(q.empty() && !done)
    {
      // release lock as long as the wait and reacquire it afterwards.
      c.wait(lock);
      if (stop) return nullptr;
    }
    if (done)
      return nullptr;
    T val = q.front();
    q.pop();
    return val;
  }

T tryDequeue(void)
{
  std::unique_lock<std::mutex> lock(m);
  if (q.empty()) return NULL;

  T val = q.front();
  q.pop();
  return val;
}

void Stop(void)
{
  stop=true;
  c.notify_one();
}

private:
  std::queue<T> q;
  mutable std::mutex m;
  std::condition_variable c;
  bool stop;
};
