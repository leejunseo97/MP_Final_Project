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

struct TrafficLights {
    bool red, yellow, green, left, right;
};

enum Shape : int {
    Circle = 0b1,
    Rectangle = 0b10,
    Left = 0b100,
    Right = 0b1000,
    Undefined = 0b10000
};

typedef std::vector<cv::Point> Contour;

cv::Mat maskImage(cv::Mat &frame, int h, int error, int sMin, int vMin) {
    cv::Mat hsvImage;
    cv::cvtColor(frame, hsvImage, cv::COLOR_BGR2HSV);
    int lowH = (h - error >= 0) ? h - error : h - error + 180;
    int highH = (h + error <= 180) ? h + error : h + error - 180;

    std::vector<cv::Mat> channels;
    cv::split(hsvImage, channels);
    if (lowH < highH) cv::bitwise_and(lowH <= channels[0], channels[0] <= highH, channels[0]);
    else cv::bitwise_or(lowH <= channels[0], channels[0] <= highH, channels[0]);

    channels[1] = channels[1] > sMin;
    channels[2] = channels[2] > vMin;

    cv::Mat mask = channels[0];
    for (int i = 1; i < 3; ++i) cv::bitwise_and(channels[i], mask, mask);

    cv::Mat grey = cv::Mat::zeros(mask.rows, mask.cols, CV_8U);
    for (int row = 0; row < mask.rows; ++row) {
        for (int col = 0; col < mask.cols; ++col) {
            uchar v1 = channels[0].data[row * mask.cols + col];
            uchar v2 = channels[1].data[row * mask.cols + col];
            uchar v3 = channels[2].data[row * mask.cols + col];
            grey.data[row * mask.cols + col] = (v1 && v2 && v3 ? 255 : 0);
        }
    }
    LOGD("OPenCV:: maskImage 함수 종료");
    return grey;
}

Shape labelPolygon(Contour &c) {
    double peri = cv::arcLength(c, true);
    Contour approx;
    cv::approxPolyDP(c, approx, 0.02 * peri, true);
    bool isConvex = cv::isContourConvex(approx);

    if ((int) approx.size() == 4 && isConvex) return Rectangle;

    if ((int) approx.size() == 7 && !isConvex) {
        int center = std::accumulate(approx.begin(), approx.end(), cv::Point(0, 0)).x /7;
        int leftCount, rightCount;
        leftCount = rightCount = 0;

        for (int i = 0; i < 7; ++i) {
            if (approx[i].x - center >= 0) ++rightCount;
            else ++leftCount;
        }

        if (leftCount > rightCount) return Left;
        else return Right;
    }

    if (approx.size() > 7 && isConvex) return Circle;
    return Undefined;
}

std::vector<Contour> findShapes(Shape shapeToFind, cv::Mat &grey, int minArea, int maxArea) {
    std::vector<Contour> contours;
    cv::findContours(grey, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    std::vector<Contour> found;

    for (int i = 0; i < (int) contours.size(); ++i) {
        Contour &c = contours[i];
        double area = cv::contourArea(c);

        if (area != 0 && minArea <= area && area <= maxArea) {
            Shape shape = labelPolygon(c);
            if (shape & shapeToFind) found.push_back(c);
        }
    }
    LOGD("OPenCV:: findShapes 함수 종료");
    return found;
}

void putTextAtCenter(cv::Mat &frame, std::string text, cv::Scalar color) {
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1, 1, &baseline);

    int x = (frame.cols - textSize.width) / 2;
    int y = (frame.rows - textSize.height) / 2;

    cv::putText(frame, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, 1,
                color, 1);
    LOGD("OpenCV:: putTextAtCenter 함수 종료");
}

TrafficLights detectLights(cv::Mat &frame, cv::Mat *drawBoard, int minArea, int maxArea) {
    LOGD("OpenCV:: detecLights 함수 진입");
    cv::Mat redMasked = maskImage(frame, 0, 15, 90, 60);
    cv::Mat yellowMasked = maskImage(frame, 30, 20, 120, 60);
    cv::Mat greenMasked = maskImage(frame, 60, 20, 90, 40);
     cv::Mat greenInverse = 255 - greenMasked;

    const static std::string captions[] = {"Red Light!", "Yellow Light!",
                                           "Green Light!", "Left Direction!",
                                           "Right Direction!"};
    const static cv::Scalar colors[] = {
            cv::Scalar(0, 0, 255), cv::Scalar(131, 232, 252), cv::Scalar(0, 255, 0),
            cv::Scalar(0, 255, 0), cv::Scalar(0, 255, 0)};

    std::vector<Contour> found[] = {
            findShapes(Circle, redMasked, minArea, maxArea),
            findShapes(Circle, yellowMasked, minArea, maxArea),
            findShapes(Circle, greenMasked, minArea, maxArea),
             findShapes(Left, greenInverse, minArea, maxArea),
             findShapes(Right, greenInverse, minArea, maxArea)
    };

    for (int i = 0; i < 5; ++i) {
        if (!found[i].empty()) {
            cv::drawContours(*drawBoard, found[i], -1, colors[i], 2);
            putTextAtCenter(*drawBoard, captions[i], colors[i]);
        }
    }


    TrafficLights result;
    result.red    = !found[0].empty();
    result.yellow = !found[1].empty();
    result.green  = !found[2].empty();
    result.left   = !found[3].empty();
    result.right  = !found[4].empty();
    LOGD("OpenCV:: detecLights 함수 종료");
    return result;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_detect_1Traffic(JNIEnv *env, jobject thiz,
                                                                jobject bitmap) {
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

    //최초의 이미지를 '소스 이미지' 와 '바탕 이미지' 로 나눈다.
    uint32_t *src = (uint32_t *) bitmapPixels;
    uint32_t *input_Pixels = (uint32_t *) malloc(info.height * info.width * 4);
    uint32_t *output_Pixels = (uint32_t *) malloc(info.height * info.width * 4);

    int pixelsCount = info.height * info.width;
    memcpy(input_Pixels, src, sizeof(uint32_t) * pixelsCount);
    memcpy(output_Pixels, src, sizeof(uint32_t) * pixelsCount);

    //소스 이미지를 3채널로 바꾼다.
    cv::Mat srcMat(info.height, info.width, CV_8UC4, input_Pixels);
    cv::Mat src_bgrMat(info.height, info.width, CV_8UC3);  //3채널로 바꿀 소스 이미지.
    cv::cvtColor(srcMat, src_bgrMat, CV_RGBA2BGR);

    //바탕 이미지를 3채널로 바꾼다.
    cv::Mat outMat(info.height, info.width, CV_8UC4, output_Pixels);
    cv::Mat out_bgrMat(info.height, info.width, CV_8UC3);  //3채널로 바꿀 바탕 이미지(여기에 텍스트랑 원 그림)
    cv::cvtColor(srcMat, out_bgrMat, CV_RGBA2BGR);

    //소스 이미지와 바탕 이미지를 통해 detectLights 함수 실행한다.
    detectLights(src_bgrMat, &out_bgrMat, 50, 100000);

    //바탕 이미지를 4채널로 바꾼다.
    cv::Mat finalMat(info.height, info.width, CV_8UC4); //4채널로 바꿀 최종 이미지
    cv::cvtColor(out_bgrMat, finalMat, CV_BGR2RGBA);

    //bitmap 과 연결된 mat 인스턴스에 copyTo 함수로 대입한다.
    cv::Mat bitmapPixelsMat(info.height, info.width, CV_8UC4, bitmapPixels);
    finalMat.copyTo(bitmapPixelsMat);


    AndroidBitmap_unlockPixels(env, bitmap);
    free(input_Pixels);
    free(output_Pixels);
    return bitmap;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_detect_1Direction(JNIEnv *env, jobject thiz,
                                                                  jobject bitmap) {
    // TODO: implement detect_Direction()
}