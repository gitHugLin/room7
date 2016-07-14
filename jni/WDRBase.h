//
// Created by linqi on 16-6-8.
//

#ifndef WDR_WDRBASE_H
#define WDR_WDRBASE_H

#include "MyThread.h"
#include "iostream"
#include "log.h"
#include "opencv2/opencv.hpp"
#include "string"
#include "vector"
#include <arm_neon.h>
#include <semaphore.h>

using namespace cv;
using namespace std;

namespace wdr {

#define wdrMin(a, b) ((a) < (b) ? (a) : (b))
#define wdrMax(a, b) ((a) < (b) ? (b) : (a))

typedef int INT32;
typedef unsigned int UINT32;
typedef short INT16;
typedef unsigned short UINT16;
typedef char INT8;
typedef unsigned char UINT8;
typedef void VOID;
typedef unsigned long long LONG;
typedef unsigned char UCHAR;

class wdrBase : public MyThread {
public:
  wdrBase();
  ~wdrBase();

public:
  bool initialize(int width, int height);
  void process(int dataAddr, int mode);
  bool loadData(int dataAddr, int mode, string imagePath = "");

private:
  MyThread *thFirst;
  MyThread *thSecond;
  MyThread *thThird;
  UCHAR *mSrcImage;
  UINT32 *mIntegralImage;
  UINT32 *mColumnSum;
  UCHAR *mSrcData;
  Mat mSrcMat;

private:
  float mToneMapLut[256][256];
  float mToneMapLut2[256][256];
  float mGainLut[256][256];

private:
  INT32 mSignal;
  UINT32 mWidth;
  UINT32 mHeight;
  INT32 mBlkSize;
  float mGainOffset;
  bool mInitialized;

private:
  void run();
  void toneMappingThread1();
  void toneMappingThread2();
  void toneMappingThread3();
  void toneMappingThread4();
  void toneMappingUneven();
  void toneMappingEven();
  void MutilToneMapping();
  void fastIntegral();
  void toneMapping();

private:
  pthread_mutex_t g_mutex;
  sem_t sem_id;
};
}

#endif // WDR_WDRBASE_H
