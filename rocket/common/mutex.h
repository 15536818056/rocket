#ifndef ROCKET_COMMON_MUTEX_H
#define ROCKET_COMMON_MUTEX_H

#include <pthread.h>

namespace rocket {

/*
由于互斥锁会经常忘了释放，会造成死锁或者性能下降
ScopeMutex是一种特殊类型的互斥锁，它具有作用域限定的特性。通常来说，互斥锁的加锁和解锁操作需要手动进行，而ScopeMutex可以利用其生命周期来自动管理锁的申请和释放。
ScopeMutex的基本原理是，在创建ScopeMutex对象时自动加锁，当对象的生命周期结束时自动解锁。这样可以避免程序员忘记释放锁导致的问题，简化代码的编写和维护。
*/
template <class T>
class ScopeMutex {

 public:
  ScopeMutex(T& mutex) : m_mutex(mutex) {
    m_mutex.lock();
    m_is_lock = true;
  }

  ~ScopeMutex() {
    m_mutex.unlock();
    m_is_lock = false;
  }

  void lock() {
    if (!m_is_lock) {
      m_mutex.lock();
    }
  }

  void unlock() {
    if (m_is_lock) {
      m_mutex.unlock();
    }
  }
  pthread_mutex_t * getMutex()
  {
    return &m_mutex;
  }

 private:

  T& m_mutex;

  bool m_is_lock {false};

};


class Mutex {
 public:
  Mutex() {
    pthread_mutex_init(&m_mutex, NULL);
  }

  ~Mutex() {
    pthread_mutex_destroy(&m_mutex);
  }

  void lock() {
    pthread_mutex_lock(&m_mutex);
  }

  void unlock() {
    pthread_mutex_unlock(&m_mutex);
  }

  pthread_mutex_t* getMutex() {
    return &m_mutex;
  }

 private:
  pthread_mutex_t m_mutex;

};



}


#endif