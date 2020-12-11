#include <opencv2/opencv.hpp>
#include <jni.h>
#include <algorithm>
#include <numeric>
#include <android/log.h>
#include <android/bitmap.h>


#define LOG_TAG "DEBUG"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_detect_1Edge(JNIEnv *env, jobject thiz,
                                                             jobject bitmap) {
    // TODO: implement detect_Edge()

    LOGD("reading bitmap info...");
    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed! error = %d", ret);
        return NULL;
    }

    LOGD("width : %d height : %d stride %d", info.width, info.height, info.stride);
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888!");
        return NULL;
    }

    LOGD("reading bitmap pixels...");
    void *bitmapPixels;

    ret = AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels);
    if (ret < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
        return NULL;
    }

    uint32_t *src = (uint32_t *) bitmapPixels;
    uint32_t *tempPixels = (uint32_t *) malloc(info.height * info.width * 4);

    int pixelsCount = info.height * info.width;
    memcpy(tempPixels, src, sizeof(uint32_t) * pixelsCount);

    cv::Mat tempMat(info.height, info.width, CV_8UC4, tempPixels);

    cv::Mat grayMat(info.height, info.width, CV_8UC4);
    cv::Mat blurMat(info.height, info.width, CV_8UC4);
    cv::Mat cannyMat(info.height, info.width, CV_8UC4);

    cv::cvtColor(tempMat, grayMat, CV_RGBA2GRAY);
    cv::blur(grayMat, cannyMat, cv::Size(3, 3));
    cv::Canny(cannyMat, cannyMat, 50, 100, 3);
    cv::cvtColor(cannyMat, cannyMat, CV_GRAY2RGBA);

    cv::Mat bitmapPixelsMat(info.height, info.width, CV_8UC4, bitmapPixels);
    cannyMat.copyTo(bitmapPixelsMat);

    AndroidBitmap_unlockPixels(env, bitmap);
    free(tempPixels);
    return bitmap;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_detect_1Traffic(JNIEnv *env, jobject thiz,
                                                                jobject bitmap) {
    // TODO: implement detect_Traffic()
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_detect_1Direction(JNIEnv *env, jobject thiz,
                                                                  jobject bitmap) {
    // TODO: implement detect_Direction()
}