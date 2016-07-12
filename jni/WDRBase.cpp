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
  mGainOffset = 0.3;
  mIntegralImage = NULL;
  mSrcData = NULL;
  mSrcImage = NULL;
  mColumnSum = NULL;
  mInitialized = false;
  thFirst = NULL;
  for (int y = 0; y < 256; y++)
    for (int x = 0; x < 256; x++) {
      float lumiBlk = (float)x / 255, lumiPixel = (float)y / 255;
      mToneMapLut[y][x] = y * (1 + mGainOffset + lumiBlk * lumiPixel) /
                          (lumiBlk + lumiPixel + mGainOffset);
      //避免查表是内存竞争冲突
      mToneMapLut2[y][x] = mToneMapLut[y][x];
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
  thFirst = NULL;
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
  mSrcData = new UCHAR[mHeight * mWidth];
}

bool wdrBase::loadData(int data, int mode, string imagePath) {
  bool ret = true;
  switch (mode) {
  case WDR_INPUT_STREAM: {
    // LOGE("WDR mode is loading Data!");
    UCHAR *srcData = (UCHAR *)data;
    memcpy(mSrcData, srcData, mWidth * mHeight * sizeof(UCHAR));
    // mSrcImage = (UCHAR *)data;
    mSrcImage = mSrcData;
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
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage;
  pGray = mSrcImage;

  for (y = 0; y < nRows; y++) {
    for (x = 0; x < nCols; x = x + 1) {

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
      int finalPixel = mToneMapLut[indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;

      //   blockAvgLumi = *(pIntegral + xMax + yMaxOffset + 1) -
      //                  *(pIntegral + xMin + yMaxOffset + 1) -
      //                  *(pIntegral + xMax + yMinOffset + 1) +
      //                  *(pIntegral + xMin + yMinOffset + 1);
      //
      //   blockAvgLumi = blockAvgLumi >> 10; // 1024 = 32*32
      //   indexX = (int)blockAvgLumi;
      //   indexY = *(pGray + offsetGray + 1);
      //   finalPixel = mToneMapLut[indexY][indexX];
      //   curPixel = wdrMin(finalPixel, 255);
      //   *(pGray + offsetGray + 1) = curPixel;
    }
  }

  // LOGE("end of toneMapping! blockAvgLumi = %lf",blockAvgLumi);
  // workEnd("TONEMAPPING TIME:");
}

void wdrBase::toneMappingThread1() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage;
  pGray = mSrcImage;
  UINT32 yLimit = nRows / 2;
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
      UINT32 indexX = (int)blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      int finalPixel = mToneMapLut[indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
    }
  }
}

void wdrBase::toneMappingThread2() {

  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;
  UINT32 *pIntegral = mIntegralImage + nCols * nRows / 2;
  pGray = mSrcImage + nCols * nRows / 2;
  UINT32 yLimit = nRows / 2;
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
      UINT32 indexX = (int)blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      int finalPixel = mToneMapLut2[indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
    }
  }
}

void wdrBase::MutilToneMapping() {
  thFirst = new MyThread(this);
  thFirst->set_thread_priority(90);
  start();
  thFirst->start();
  thFirst->join();
}

void wdrBase::run() {
  if (is_equals(thFirst)) {
    toneMappingThread1();
    pthread_mutex_lock(&g_mutex);
    mSignal++;
    if (mSignal == 2) {
      mSignal = 0;
      sem_post(&sem_id);
    }
    pthread_mutex_unlock(&g_mutex);

  } else if (is_equals(this)) {
    toneMappingThread2();
    // toneMapping();
    pthread_mutex_lock(&g_mutex);
    mSignal++;
    if (mSignal == 2) {
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
    workBegin();
    LOGE("WDR is running!");
    fastIntegral();
    // toneMapping();
    MutilToneMapping();

    UCHAR *srcData = (UCHAR *)data;
    sem_wait(&sem_id);
    workEnd("WDR NEON");
    memcpy(srcData, mSrcImage, mWidth * mHeight * sizeof(UCHAR));
    if (NULL != thFirst) {
      delete thFirst;
      thFirst = NULL;
    }
    // workEnd("WDR NEON");
    // if (mode == WDR_INPUT_YUV) {
    //   YV12ToBGR24_OpenCV(mSrcImage, mWidth, mHeight,
    //   "/data/local/WDRBase.jpg");
    // }
  }
}
