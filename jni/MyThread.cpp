#include "MyThread.h"

//线程初始化号
int MyThread::thread_init_number = 1;

MyThread::MyThread() {
  //当前线程的线程ID
  tid = 0;
  //线程的状态－新建
  thread_status = THREAD_STATUS_NEW;
  //当前线程初始化序号
  current_thread_init_number = get_next_thread_num();
  //初始化线程属性
  pthread_attr_init(&attr);
}

MyThread::MyThread(Runnable *iTarget) {
  target = iTarget;
  tid = 0;
  thread_status = THREAD_STATUS_NEW;
  current_thread_init_number = get_next_thread_num();
  pthread_attr_init(&attr);
}

MyThread::~MyThread() { pthread_attr_destroy(&attr); }

inline int MyThread::get_next_thread_num() { return thread_init_number++; }

void *MyThread::run0(void *pVoid) {
  MyThread *p = (MyThread *)pVoid;
  p->run1();
  return p;
}

void *MyThread::run1() {
  thread_status = THREAD_STATUS_RUNNING;
  tid = pthread_self();
  run();
  thread_status = THREAD_STATUS_EXIT;
  tid = 0;
  pthread_exit(NULL);
}

void MyThread::run() {
  if (target != NULL) {
    (*target).run();
  }
}

bool MyThread::start() { return pthread_create(&tid, &attr, run0, this); }

inline pthread_t MyThread::get_current_thread_id() { return pthread_self(); }

inline pthread_t MyThread::get_thread_id() { return tid; }

inline int MyThread::get_state() { return thread_status; }

void MyThread::join() {
  if (tid > 0) {
    pthread_join(tid, NULL);
  }
}

void MyThread::join(unsigned long millis_time) {
  if (tid == 0) {
    return;
  }
  if (millis_time == 0) {
    join();
  } else {
    unsigned long k = 0;
    while (thread_status != THREAD_STATUS_EXIT && k <= millis_time) {
      usleep(100);
      k++;
    }
  }
}

bool MyThread::operator==(const MyThread *other_pthread) {
  if (other_pthread == NULL) {
    return false;
  }
  if (current_thread_init_number ==
      (*other_pthread).current_thread_init_number) {
    return true;
  }
  return false;
}

bool MyThread::is_equals(MyThread *iTarget) {
  if (iTarget == NULL) {
    return false;
  }
  return pthread_self() == iTarget->tid;
}

//设置线程的类型:绑定/非绑定
void MyThread::set_thread_scope(bool isSystem) {
  if (isSystem) {
    // Linux的线程设置绑定
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  } else {
    /*
    *Linux的线程永远都是绑定的，所以PTHREAD_SCOPE_PROCESS在Linux中不管用，而且会返回ENOTSUP错误,
    *既然Linux并不支持线程的非绑定，为什么还要提供这个接口呢？答案就是兼容！因为Linux的NTPL是号称POSIX标准兼容的，
    *而绑定属性正是POSIX标准所要求的，所以提供了这个接口。如果读者们只是在Linux下编写多线程程序，可以完全忽略这个属性。
     */
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
  }
}

//设置线程的优先级，1－99，其中99为实时，意外的为普通
void MyThread::set_thread_priority(int priority) {
  pthread_attr_getschedparam(&attr, &param);
  param.__sched_priority = priority;
  pthread_attr_setschedparam(&attr, &param);
}

int MyThread::get_thread_priority() {
  pthread_attr_getschedparam(&attr, &param);
  return param.__sched_priority;
}
