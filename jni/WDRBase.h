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

/* gain值计算公式
*  以下 blumi, plumi, offset 的定义域均在 0~1 之间
*  blumi 当前像素的背景平均亮度值,plumi 当前像素的亮度值,offset 增益值的微调值
*  gain = (1+blumi*plumi)/(offset+bl*pl)
*  该公式对在blumi,plumi同时很小时，gain 值动态范围大，能增强暗区域的对比度
*  即在地灰度值时，动态范围大，图像对比度高，而在高灰度值时，gain值很小
*  offset 增大gain值变小，offset 变小，gain值增大
*/

#define _UP_DOWN_
// #define _TIME_COUNT_
#define _VU_
#define _MEMCPY_
// 亮度通道的gain值偏移量 0~1
#define __global_GainOffset 0.3
// UV通道的gain值偏移量 0~1
// #define __global_VuGainOffset 0.3

//__global_thNums 为子线程数，不包括主线程
//__global_thNums 必须大于等于 1 小于等于 3
#define __global_thNums 3

#define wdrMin(a, b) ((a) < (b) ? (a) : (b))
#define wdrMax(a, b) ((a) < (b) ? (b) : (a))
#define wdrClip(a, min, max)                                                   \
  (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))

typedef int INT32;
typedef unsigned int UINT32;
typedef short INT16;
typedef unsigned short UINT16;
typedef char INT8;
typedef unsigned char UINT8;
typedef void VOID;
typedef unsigned long long LONG;
typedef unsigned char UCHAR;

typedef MyThread *pThread;

class wdrBase : public MyThread {
public:
  wdrBase();
  ~wdrBase();

public:
  bool initialize(int width, int height);
  void process(int dataAddr, int mode);
  bool loadData(int dataAddr, int mode, string imagePath = "");

private:
  pThread mThread[3];
  UCHAR *mSrcImage;
  UINT32 *mIntegralImage;
  UINT32 *mColumnSum;
  UCHAR *mSrcData;
  Mat mSrcMat;

private:
  float mToneMapLut[__global_thNums + 1][256][256];
  float mGainLut[__global_thNums + 1][256][256];

private:
  INT32 mSignal;
  UINT32 mWidth;
  UINT32 mHeight;
  INT32 mBlkSize;
  float mGainOffset;
  // float mVuGainOffset;
  bool mInitialized;

private:
  void run();
  void toneMappingThread1();
  void toneMappingThread2();
  void toneMappingThread3();
  void toneMappingThread4();

private:
  void MutilToneMapping(int thNums = __global_thNums);
  void fastIntegral();

private:
  // test function
  void toneMappingUneven();
  void toneMappingEven();
  void toneMapping();

private:
  pthread_mutex_t g_mutex;
  sem_t sem_id;
};
}

#endif // WDR_WDRBASE_H
