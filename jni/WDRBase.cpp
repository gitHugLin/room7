//
// Created by linqi on 16-6-8.
//
#include "WDRBase.h"
#include "WDRInterface.h"
using namespace wdr;

static double work_begin = 0;
static double work_end = 0;
static double gTime = 0;
static void workEnd(char *tag = "TimeCounts");
static void workBegin();

static void workBegin() { work_begin = getTickCount(); }

static void workEnd(char *tag) {
  work_end = getTickCount() - work_begin;
  gTime = work_end / ((double)getTickFrequency()) * 1000.0;
  LOGD("[TAG: %s ]:TIME = %lf ms \n", tag, gTime);
}

bool YV12ToBGR24_OpenCV(unsigned char *pYUV, int width, int height,
                        char *dstPath) {
  if (1) {
    if (width < 1 || height < 1 || pYUV == NULL)
      return false;
    Mat dst;
    Mat src(height + height / 2, width, CV_8UC1, pYUV);
    cvtColor(src, dst, CV_YUV2RGB_YV12);
    imwrite(dstPath, dst);
  }
  return true;
}

wdrBase::wdrBase() {
  mSignal = 0;
  mWidth = 0;
  mHeight = 0;
  mBlkSize = 31;
  mGainOffset = __global_GainOffset;
  // mVuGainOffset = __global_VuGainOffset;
  mIntegralImage = NULL;
  mSrcData = NULL;
  mSrcImage = NULL;
  mColumnSum = NULL;
  mInitialized = false;

  for (size_t i = 0; i < 3; i++) {
    mThread[i] = NULL;
  }
  /*
  *  以下bl,pl,offset 的定义域均在 0~1 之间
  *  bl 当前像素的背景平均亮度值；pl 当前像素的亮度值；offset 增益值的微调值
  *  gain = (1+bl*pl)/(offset+bl*pl) 该公式对在bl,pl同时很小时，
  *  即在地灰度值时，动态范围大，图像对比度高，而在高灰度值时，gain值很小
  *  offset 增大gain值变小，offset 变小，gain值增大
  */
  for (int y = 0; y < 256; y++)
    for (int x = 0; x < 256; x++) {
      float lumiBlk = (float)x / 255, lumiPixel = (float)y / 255;
      float gain =
          (1 + lumiBlk * lumiPixel) / (lumiBlk + lumiPixel + mGainOffset);
      //避免查表是内存竞争冲突
      for (int i = 0; i < __global_thNums + 1; i++) {
        mToneMapLut[i][y][x] = y * gain;
      }
#ifdef _VU_
      float vuGain =
          (1 + lumiBlk * lumiPixel) / (lumiBlk + lumiPixel + mGainOffset);
      for (int i = 0; i < __global_thNums + 1; i++) {
        mGainLut[i][y][x] = vuGain;
      }
#endif
    }

  sem_init(&sem_id, 0, 0);
  pthread_mutex_init(&g_mutex, NULL);
}

wdrBase::~wdrBase() {
  if (NULL != mIntegralImage) {
    delete[] mIntegralImage;
    mIntegralImage = NULL;
  }
  if (NULL != mColumnSum) {
    delete[] mColumnSum;
    mColumnSum = NULL;
  }
  if (NULL != mSrcData) {
    delete[] mSrcData;
    mSrcData = NULL;
  }
  for (size_t i = 0; i < 3; i++) {
    mThread[i] = NULL;
  }
  pthread_mutex_destroy(&g_mutex);
  sem_destroy(&sem_id);
}

bool wdrBase::initialize(int width, int height) {
  if (mInitialized) {
    LOGD("WDR module initialized before");
    return false;
  }
  mWidth = width;
  mHeight = height;
  mInitialized = true;
  mIntegralImage = new UINT32[mHeight * mWidth];
  mColumnSum = new UINT32[mWidth]; // sum of each column
  mSrcData = new UCHAR[mHeight * mWidth * 3 / 2];
}

bool wdrBase::loadData(int data, int mode, string imagePath) {
  bool ret = true;
  switch (mode) {
  case WDR_INPUT_STREAM: {
    // LOGE("WDR mode is loading Data!");
    UCHAR *srcData = (UCHAR *)data;
#ifdef _MEMCPY_
    memcpy(mSrcData, srcData, mWidth * mHeight * 3 / 2 * sizeof(UCHAR));
    // mSrcImage = (UCHAR *)data;
    mSrcImage = mSrcData;
#else
    mSrcImage = srcData;
#endif
  } break;
  // case WDR_INPUT_YUV: {
  //   LOGD("input yuv.\n");
  //   Mat rgb = imread(imagePath);
  //   cvtColor(rgb, mSrcMat, COLOR_RGB2YUV_YV12);
  //
  //   mSrcImage = mSrcMat.data;
  //   mWidth = rgb.cols;
  //   mHeight = rgb.rows;
  //   initialize(mWidth, mHeight);
  // } break;
  default:
    ret = false;
    break;
  }

  return ret;
}

void wdrBase::fastIntegral() {
  // workBegin();
  // LOGE("WDR mode in fastIntegral!");
  UINT8 *pGray = NULL;
  pGray = mSrcImage;
  UINT32 *pIntegral = mIntegralImage;
  UINT32 *columnSum = mColumnSum; // sum of each column
  // calculate integral of the first line
  for (int i = 0; i < mWidth; i++) {
    columnSum[i] = *(pGray + i);
    *(pIntegral + i) = *(pGray + i);
    if (i > 0) {
      *(pIntegral + i) += *(pIntegral + i - 1);
    }
  }
  for (int i = 1; i < mHeight; i++) {
    int offset = i * mWidth;
    // first column of each line
    columnSum[0] += *(pGray + offset);
    *(pIntegral + offset) = columnSum[0];
    // other columns
    for (int j = 1; j < mWidth; j++) {
      columnSum[j] += *(pGray + offset + j);
      *(pIntegral + offset + j) = *(pIntegral + offset + j - 1) + columnSum[j];
    }
  }
  // int gGainIndex =
  //   *(pIntegral + (mWidth - 1) * (mHeight - 1)) / (mWidth * mHeight);
  // LOGE("Log:max value in intergralImage pixels = %d", gGainIndex);
  // workEnd("end of fastIntegral!");
}

void wdrBase::toneMapping() {
  // workBegin();
  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT8 *pVU = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage;
  pGray = mSrcImage;

  for (y = 0; y < nRows; y++) {
    for (x = 0; x < nCols; x) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = wdrMax(y - mBlkSize, 0);
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = wdrMin(y + mBlkSize, nRows - 1);

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);
      //   blockAvgLumi =
      //       *(pIntegral + xMax + yMaxOffset) - *(pIntegral + xMin +
      //       yMaxOffset) -
      //       *(pIntegral + xMax + yMinOffset) + *(pIntegral + xMin +
      //       yMinOffset);

      //   blockAvgLumi = 100;
      // blockAvgLumi = blockAvgLumi/((yMax - yMin + 1)*(xMax - xMin + 1));
      blockAvgLumi = blockAvgLumi >> 12;
      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = (int)blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      int finalPixel = mToneMapLut[0][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;

#ifdef _VU_
      if (0x1 & y && 0x1 & x) {
        float gain = mGainLut[0][indexY][indexX];
        INT32 V = gain * (*pVU - 128);
        *pVU = wdrMin(V + 128, 255);
        pVU++;
        INT32 U = gain * (*pVU - 128);
        *pVU = wdrMin(U + 128, 255);
        pVU++;
      }
#endif
    }
  }

  // LOGE("end of toneMapping! blockAvgLumi = %lf",blockAvgLumi);
  // workEnd("TONEMAPPING TIME:");
}

void wdrBase::toneMappingUneven() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT8 *pVU = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage;
  pGray = mSrcImage;
  UINT32 yLimit = nRows / 2;
  for (y = 0; y < nRows; y = y + 2) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = wdrMax(y - mBlkSize, 0);
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = wdrMin(y + mBlkSize, nRows - 1);
      //   UINT32 yMax = y + mBlkSize;

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);

      blockAvgLumi = blockAvgLumi >> 12;
      //   blockAvgLumi = blockAvgLumi / ((yMax - yMin + 1) * (xMax - xMin +1));

      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = (int)blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      int finalPixel = mToneMapLut[0][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
    }
  }
}

void wdrBase::toneMappingEven() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT8 *pVU = NULL;
  UINT32 blockAvgLumi = 0;
  // UINT32 *pIntegral = mIntegralImage + nCols * nRows / 2;
  UINT32 *pIntegral = mIntegralImage;
  // pGray = mSrcImage + nCols * nRows / 2;
  pGray = mSrcImage;
  UINT32 yLimit = nRows / 2;
  for (y = 1; y < nRows; y = y + 2) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      //   UINT32 yMin = y - mBlkSize;
      UINT32 yMin = wdrMax(y - mBlkSize, 0);
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = wdrMin(y + mBlkSize, nRows - 1);

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);

      blockAvgLumi = blockAvgLumi >> 12;
      //   blockAvgLumi = blockAvgLumi / ((yMax - yMin + 1) * (xMax - xMin +1));

      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = (int)blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      int finalPixel = mToneMapLut[1][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
    }
  }
}

void wdrBase::toneMappingThread1() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage;
  pGray = mSrcImage;
#ifdef _VU_
  UINT8 *pVU = NULL;
  pVU = mSrcImage + nCols * nRows;
#endif

  UINT32 yLimit = nRows / (__global_thNums + 1);
  for (y = 0; y < yLimit; y++) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = wdrMax(y - mBlkSize, 0);
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = y + mBlkSize;

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);

      blockAvgLumi = blockAvgLumi >> 12;
      //   blockAvgLumi = blockAvgLumi / ((yMax - yMin + 1) * (xMax - xMin +1));

      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      INT32 finalPixel = mToneMapLut[0][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;

#ifdef _VU_
      if (0x1 & y && 0x1 & x) {
        float gain = mGainLut[0][indexY][indexX];
        INT32 V = gain * (*pVU - 128);
        // *pVU = wdrMin(V + 128, 255);
        *pVU = wdrClip(V + 128, 0, 255);
        pVU++;
        INT32 U = gain * (*pVU - 128);
        // *pVU = wdrMin(U + 128, 255);
        *pVU = wdrClip(U + 128, 0, 255);
        pVU++;
      }
#endif
    }
  }
}

void wdrBase::toneMappingThread2() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral =
      mIntegralImage + UINT32(nCols * nRows / (__global_thNums + 1));
  pGray = mSrcImage + UINT32(nCols * nRows / (__global_thNums + 1));

#ifdef _VU_
  UINT8 *pVU = NULL;
  float rate = (1.0f / 2 * (1.0f / (__global_thNums + 1))) + 1;
  UINT32 offset = rate * nCols * nRows;
  pVU = mSrcImage + offset;
#endif

  UINT32 yLimit = nRows / (__global_thNums + 1);
  // UINT32 yLimit = nRows / 4;
  for (y = 0; y < yLimit; y++) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = y - mBlkSize;
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = y + mBlkSize;

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);

      blockAvgLumi = blockAvgLumi >> 12;
      //   blockAvgLumi = blockAvgLumi / ((yMax - yMin + 1) * (xMax - xMin +1));

      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      INT32 finalPixel = mToneMapLut[1][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
#ifdef _VU_
      if (0x1 & y && 0x1 & x) {
        float gain = mGainLut[1][indexY][indexX];
        INT32 V = gain * (*pVU - 128);
        // *pVU = wdrMin(V + 128, 255);
        *pVU = wdrClip(V + 128, 0, 255);
        pVU++;
        INT32 U = gain * (*pVU - 128);
        // *pVU = wdrMin(U + 128, 255);
        *pVU = wdrClip(U + 128, 0, 255);
        pVU++;
      }
#endif
    }
  }
}

void wdrBase::toneMappingThread3() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;

  UINT32 *pIntegral =
      mIntegralImage + UINT32(nCols * nRows * 2 / (__global_thNums + 1.0f));
  pGray = mSrcImage + UINT32(nCols * nRows * 2 / (__global_thNums + 1.0f));
#ifdef _VU_

  UINT8 *pVU = NULL;
  float rate = (1.0f / 2 * (2.0f / (__global_thNums + 1.0f))) + 1;
  UINT32 offset = rate * nCols * nRows;
  pVU = mSrcImage + offset;
#endif

  UINT32 yLimit = nRows / (__global_thNums + 1);
  // UINT32 yLimit = nRows / 4;
  for (y = 0; y < yLimit; y++) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = y - mBlkSize;
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = y + mBlkSize;

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);

      blockAvgLumi = blockAvgLumi >> 12;
      //   blockAvgLumi = blockAvgLumi / ((yMax - yMin + 1) * (xMax - xMin +1));

      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      INT32 finalPixel = mToneMapLut[2][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
#ifdef _VU_
      if (0x1 & y && 0x1 & x) {
        float gain = mGainLut[2][indexY][indexX];
        INT32 V = gain * (*pVU - 128);
        // *pVU = wdrMin(V + 128, 255);
        *pVU = wdrClip(V + 128, 0, 255);
        pVU++;
        INT32 U = gain * (*pVU - 128);
        // *pVU = wdrMin(U + 128, 255);
        *pVU = wdrClip(U + 128, 0, 255);
        pVU++;
      }
#endif
    }
  }
}

void wdrBase::toneMappingThread4() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage + UINT32(nCols * nRows * __global_thNums /
                                              (__global_thNums + 1.0f));
  pGray = mSrcImage +
          UINT32(nCols * nRows * __global_thNums / (__global_thNums + 1.0f));
#ifdef _VU_
  UINT8 *pVU = NULL;
  float rate =
      (1.0f / 2 * (1.0f * __global_thNums / (__global_thNums + 1.0f))) + 1;
  UINT32 offset = rate * nCols * nRows;
  pVU = mSrcImage + offset;
#endif

  UINT32 yLimit = nRows / (__global_thNums + 1);
  for (y = 0; y < yLimit; y++) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = y - mBlkSize;
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = wdrMin(y + mBlkSize, yLimit - 1);

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;

      INT32 pRB = xMax + yMaxOffset;
      INT32 pLB = xMin + yMaxOffset;
      INT32 pRT = xMax + yMinOffset;
      INT32 pLT = xMin + yMinOffset;

      blockAvgLumi = *(pIntegral + pRB) - *(pIntegral + pLB) -
                     *(pIntegral + pRT) + *(pIntegral + pLT);

      blockAvgLumi = blockAvgLumi >> 12;
      //   blockAvgLumi = blockAvgLumi / ((yMax - yMin + 1) * (xMax - xMin +1));

      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      INT32 finalPixel = mToneMapLut[__global_thNums][indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
#ifdef _VU_
      if (0x1 & y && 0x1 & x) {
        float gain = mGainLut[__global_thNums][indexY][indexX];
        INT32 V = gain * (*pVU - 128);
        // *pVU = wdrMin(V + 128, 255);
        *pVU = wdrClip(V + 128, 0, 255);
        pVU++;
        INT32 U = gain * (*pVU - 128);
        // *pVU = wdrMin(U + 128, 255);
        *pVU = wdrClip(U + 128, 0, 255);
        pVU++;
      }
#endif
    }
  }
}

void wdrBase::MutilToneMapping(int thNums) {
  this->set_thread_priority(90);
  start();
  for (size_t i = 0; i < thNums; i++) {
    mThread[i] = new MyThread(this);
    mThread[i]->set_thread_priority(90);
    mThread[i]->start();
    mThread[i]->join();
  }
}

void wdrBase::run() {
  if (is_equals(mThread[0])) {
#ifdef _UP_DOWN_
    toneMappingThread1();
#else
    toneMappingUneven();
#endif
    pthread_mutex_lock(&g_mutex);
    mSignal++;
    if (mSignal == __global_thNums + 1) {
      mSignal = 0;
      sem_post(&sem_id);
    }
    pthread_mutex_unlock(&g_mutex);

  } else if (is_equals(mThread[1])) {
#ifdef _UP_DOWN_
    toneMappingThread2();
#endif
    pthread_mutex_lock(&g_mutex);
    mSignal++;
    if (mSignal == __global_thNums + 1) {
      mSignal = 0;
      sem_post(&sem_id);
    }
    pthread_mutex_unlock(&g_mutex);
  } else if (is_equals(mThread[2])) {
#ifdef _UP_DOWN_
    toneMappingThread3();
#endif
    pthread_mutex_lock(&g_mutex);
    mSignal++;
    if (mSignal == __global_thNums + 1) {
      mSignal = 0;
      sem_post(&sem_id);
    }
    pthread_mutex_unlock(&g_mutex);
  } else if (is_equals(this)) {
#ifdef _UP_DOWN_
    toneMappingThread4();
#else
    toneMappingEven();
#endif
    pthread_mutex_lock(&g_mutex);
    mSignal++;
    if (mSignal == __global_thNums + 1) {
      mSignal = 0;
      sem_post(&sem_id);
    }
    pthread_mutex_unlock(&g_mutex);
  }
}

void wdrBase::process(int data, int mode) {
  // workBegin();
  bool ret = loadData(data, mode);
  if (ret) {
#ifdef _TIME_COUNT_
    workBegin();
#endif
    // LOGE("WDR width = %d,height = %d", mWidth, mHeight);
    fastIntegral();
    // toneMapping();
    MutilToneMapping(__global_thNums);

    UCHAR *srcData = (UCHAR *)data;
    sem_wait(&sem_id);
#ifdef _TIME_COUNT_
    workEnd("WDR NEON");
#endif
#ifdef _MEMCPY_
    memcpy(srcData, mSrcImage, mWidth * mHeight * 3 / 2 * sizeof(UCHAR));
#endif

    for (size_t i = 0; i < __global_thNums; i++) {
      if (mThread[i] != NULL)
        delete mThread[i];
      mThread[i] = NULL;
    }
    // workEnd("WDR NEON");
    // if (mode == WDR_INPUT_YUV) {
    //   YV12ToBGR24_OpenCV(mSrcImage, mWidth, mHeight,
    //   "/data/local/WDRBase.jpg");
    // }
  }
}
