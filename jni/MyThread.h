#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <pthread.h>
#include <unistd.h>

class Runnable {
public:
  //运行实体
  virtual void run() = 0;
};

//线程类
class MyThread : public Runnable {
public:
  //构造函数
  MyThread();
  //构造函数
  MyThread(Runnable *target);
  //析构
  ~MyThread();

private:
  //线程初始化号
  static int thread_init_number;
  //当前线程初始化序号
  int current_thread_init_number;
  //线程体
  Runnable *target;
  //当前线程的线程ID
  pthread_t tid;
  //线程的状态
  int thread_status;
  //线程属性
  pthread_attr_t attr;
  //线程优先级
  sched_param param;
  //获取执行方法的指针
  static void *run0(void *pVoid);
  //内部执行方法
  void *run1();
  //获取线程序号
  static int get_next_thread_num();

public:
  //线程的状态－新建
  static const int THREAD_STATUS_NEW = 0;
  //线程的状态－正在运行
  static const int THREAD_STATUS_RUNNING = 1;
  //线程的状态－运行结束
  static const int THREAD_STATUS_EXIT = -1;

  //线程的运行体
  void run();
  //开始执行线程
  bool start();
  //获取线程状态
  int get_state();
  //等待线程直至退出
  void join();
  //等待线程退出或者超时
  void join(unsigned long millis_time);
  //比较两个线程时候相同，通过current_thread_init_number判断
  bool operator==(const MyThread *other_pthread);
  //获取this线程ID
  pthread_t get_thread_id();
  //获取当前线程ID
  static pthread_t get_current_thread_id();
  //当前线程是否和某个线程相等，通过tid判断
  static bool is_equals(MyThread *iTarget);
  //设置线程的类型:绑定/非绑定
  void set_thread_scope(bool isSystem);
  //获取线程的类型:绑定/非绑定
  bool get_thread_scope();
  //设置线程的优先级，1－99，其中99为实时，意外的为普通
  void set_thread_priority(int priority);
  //获取线程的优先级
  int get_thread_priority();
};

#endif // MYTHREAD_H
