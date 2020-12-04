#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#include <jni.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <math.h>
#include <time.h>
#include "CL/opencl.h"

#define LOG_TAG "DEBUG"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define checkCL(expression) {  \
    cl_int err = (expression); \
    if (err < 0 && err > -64) { \
        printf("Error on line &d. error code: %d\n", __LINE__, err); \
        exit(0); \
    } \
} \


int fd1 = 0;

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_mpclass_projectmp_MainActivity_open_1LED_1Driver(JNIEnv *env, jclass clazz,
                                                                  jstring path) {
    LOGD("LED:: 드라이버 여는중");
    jboolean iscopy;
    const char *path_utf = env->GetStringUTFChars(path, &iscopy);
    fd1 = open(path_utf, O_WRONLY);
    (env)->ReleaseStringUTFChars(path, path_utf);

    if (fd1 < 0) {
        LOGE("LED:: 드라이버 열기 오류!");
        return -1;
    }
    else {
        LOGD("LED:: 드라이버 잘 열림!");
        return 1;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_mpclass_projectmp_MainActivity_close_1LED_1Driver(JNIEnv *env, jclass clazz) {
    if (fd1 > 0) {
        LOGD("LED:: 드라이버 닫는다");
        close(fd1);
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_mpclass_projectmp_MainActivity_write_1LED_1Driver(JNIEnv *env, jclass clazz,
                                                                   jbyteArray data, jint length) {
    LOGD("LED:: 쓰기 시작! ");
    jbyte *chars = (env)->GetByteArrayElements( data, 0);
    if (fd1 > 0) write(fd1, (unsigned char *) chars, length);
    (env)->ReleaseByteArrayElements( data, chars, 0);
    LOGD("LED:: 쓰기 완료!");
}

int opencl_infra_creation (cl_context &context,
                           cl_platform_id &cpPlatform,
                           cl_device_id &device_id,
                           cl_command_queue &queue,
                           cl_program &program,
                           cl_kernel &kernel,
                           char *kernel_file_buffer,
                           size_t kernel_file_size,
                           AndroidBitmapInfo info,
                           unsigned char *kernel_name)
{
    cl_int err;

    //Bind to Platform
    checkCL(clGetPlatformIDs(1, &cpPlatform, NULL));

    //Get ID for the device
    checkCL(clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL));

    //Create a context
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);

    //Create a command queue
    queue = clCreateCommandQueue(context, device_id, 0, &err);
    checkCL(err);

    //create the compute program from the source buffer
    program = clCreateProgramWithSource(context, 1, (const char **) & kernel_file_buffer, &kernel_file_size, &err);
    checkCL(err);

    //Build the program executable
    checkCL(clBuildProgram(program, 0, NULL, NULL, NULL, NULL));

    //create the compute kernel int the program we wish to run
    kernel = clCreateKernel(program, (const char*)kernel_name, &err);
    checkCL(err);

    return 0;
}

int launch_the_kernel(cl_context &context,
                      cl_command_queue &queue,
                      cl_kernel &kernel,
                      size_t globalSize,
                      size_t localSize,
                      AndroidBitmapInfo &info,
                      cl_mem &d_src,
                      cl_mem &d_dst,
                      unsigned char *image,
                      unsigned char *blured_img)
{
    cl_int err;
    //create the input and output arrays in device memory for our calculation
    d_src = clCreateBuffer(context, CL_MEM_READ_ONLY, info.width*info.height*4, NULL, &err);
    checkCL(err);
    d_dst = clCreateBuffer(context, CL_MEM_WRITE_ONLY, info.width*info.height*4, NULL, &err);
    checkCL(err);

    //wirte our data set into the input array in device memory
    checkCL(clEnqueueWriteBuffer(queue, d_src, CL_TRUE, 0, info.width*info.height*4, image, 0, NULL, NULL));

    //set the arg to our compute kernel
    checkCL(clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_src));
    checkCL(clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_dst));
    checkCL(clSetKernelArg(kernel, 2, sizeof(int), &info.width));
    checkCL(clSetKernelArg(kernel, 3, sizeof(int), &info.height));

    //execute the kernel over the entire range of the data set
    checkCL(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalSize, &localSize, 0, NULL, NULL));

    //wait for the command queue to get serviced before reading back results
    checkCL(clFinish(queue));

    //read the results form the device
    checkCL(clEnqueueReadBuffer(queue, d_dst, CL_TRUE, 0, info.width*info.height*4, blured_img, 0, NULL, NULL));

    return 0;
}

void gaussian_blur(unsigned char *src_img, unsigned char *dst_img, int width, int height) {
    float red = 0, green = 0, blue = 0, alpha=0;
    int row = 0, col = 0;
    int m, n, k;
    int pix;
    float mask[9][9]=
            {	{0.011237, 0.011637, 0.011931, 0.012111, 0.012172, 0.012111, 0.011931, 0.011637, 0.011237},
                 {0.011637, 0.012051, 0.012356, 0.012542, 0.012605, 0.012542, 0.012356, 0.012051, 0.011637},
                 {0.011931, 0.012356, 0.012668, 0.012860, 0.012924, 0.012860, 0.012668, 0.012356, 0.011931},
                 {0.012111, 0.012542, 0.012860, 0.013054, 0.013119, 0.013054, 0.012860, 0.012542, 0.012111},
                 {0.012172, 0.012605, 0.012924, 0.013119, 0.013185, 0.013119, 0.012924, 0.012605, 0.012172},
                 {0.012111, 0.012542, 0.012860, 0.013054, 0.013119, 0.013054, 0.012860, 0.012542, 0.012111},
                 {0.011931, 0.012356, 0.012668, 0.012860, 0.012924, 0.012860, 0.012668, 0.012356, 0.011931},
                 {0.011637, 0.012051, 0.012356, 0.012542, 0.012605, 0.012542, 0.012356, 0.012051, 0.011637},
                 {0.011237, 0.011637, 0.011931, 0.012111, 0.012172, 0.012111, 0.011931, 0.011637, 0.011237}
            };

    for(row=0; row<height; row++) {
        for(col=0; col<width; col++) {
            blue = 0;
            green = 0;
            red = 0;
            alpha = 255;

            for(m=0; m<9; m++) {
                for(n=0; n<9; n++) {
                    pix = (((row+m-4)%height)*width)*4 + ((col+n-4)%width)*4;
                    red += src_img[pix+0]*mask[m][n];
                    green += src_img[pix+1]*mask[m][n];
                    blue += src_img[pix+2]*mask[m][n];
                    alpha = src_img[pix+3];
                }
            }
            dst_img[(row*width+col)*4 + 0] = red;
            dst_img[(row*width+col)*4 + 1] = green;
            dst_img[(row*width+col)*4 + 2] = blue;
            dst_img[(row*width+col)*4 + 3] = alpha;
        }
    }
}


extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_GaussianBlurBitmap(JNIEnv *env, jobject thiz,
                                                                      jobject bitmap) {
    LOGD("reading bitmap info...");
    AndroidBitmapInfo info;
    int ret;
    if((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed! error = %d", ret);
        return NULL;
    }

    LOGD("width : %d height : %d stride %d", info.width, info.height, info.stride);
    if(info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888!");
        return NULL;
    }

    LOGD("reading bitmap pixels...");
    void* bitmapPixels;

    ret = AndroidBitmap_lockPixels(env,bitmap,&bitmapPixels);
    if(ret < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
        return NULL;
    }

    uint32_t* src = (uint32_t*)bitmapPixels;
    uint32_t* tempPixels = (uint32_t*) malloc(info.height * info.width * 4);

    int pixelsCount = info.height * info.width;
    memcpy(tempPixels, src, sizeof(uint32_t) * pixelsCount);

    gaussian_blur((unsigned char*)tempPixels, (unsigned char*)bitmapPixels, info.width, info.height);

    AndroidBitmap_unlockPixels(env, bitmap);
    free(tempPixels);
    return bitmap;

}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_mpclass_projectmp_MainActivity_GaussianBlurGPU(JNIEnv *env, jobject thiz,
                                                                   jobject bitmap) {
    LOGD("reading bitmap info...");
    AndroidBitmapInfo info;
    int ret;
    if((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed! error = %d", ret);
        return NULL;
    }

    LOGD("width : %d height : %d stride %d", info.width, info.height, info.stride);
    if(info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888!");
        return NULL;
    }

    LOGD("reading bitmap pixels...");

    void* bitmapPixels;
    if((ret=AndroidBitmap_lockPixels(env,bitmap,&bitmapPixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
        return NULL;
    }
    uint32_t* src = (uint32_t*)bitmapPixels;

    uint32_t* tempPixels = (uint32_t*) malloc(info.height * info.width * 4);
    int pixelsCount = info.height * info.width;

    memcpy(tempPixels, src, sizeof(uint32_t) * pixelsCount);

    FILE *file_handle;
    char *kernel_file_buffer, *file_log;
    size_t kernel_file_size, log_size;

    const char* cl_file_name = "/data/local/tmp/Blur.cl";
    const char* kernel_name = "kernel_blur";

    //Device input buffers
    cl_mem d_src;
    //Device output buffer
    cl_mem d_dst;

    cl_platform_id cpPlatform; //opencl platform
    cl_device_id device_id;    //device id
    cl_context context;        //context
    cl_command_queue queue;    //command queue
    cl_program program;        //program
    cl_kernel kernel;          //kernel

    file_handle = fopen(cl_file_name, "r");
    if (file_handle == NULL)
    {
        printf("Couldn't find the file");
        exit(1);
    }

    //read kernel file
    fseek(file_handle, 0, SEEK_END);
    kernel_file_size = ftell(file_handle);
    rewind(file_handle);
    kernel_file_buffer = (char *)malloc(kernel_file_size + 1);
    kernel_file_buffer[kernel_file_size] = '\0';
    fread(kernel_file_buffer, sizeof(char), kernel_file_size, file_handle);
    fclose(file_handle);
    fclose(file_handle);

    //initialize vectors on host
    int i;
    size_t globalSize, localSize, grid;

    //Number of work items in each local work group
    localSize = 64;
    int n_pix = info.width*info.height;

    //Number of total work items - localsize must be devisor
    grid = ((n_pix) % localSize) ? (n_pix / localSize) + 1 : n_pix / localSize;
    globalSize = grid * localSize;

    opencl_infra_creation(context, cpPlatform, device_id, queue, program, kernel, kernel_file_buffer,
                          kernel_file_size, info, (unsigned char*)kernel_name);


    launch_the_kernel(context, queue, kernel, globalSize, localSize, info, d_src, d_dst, (unsigned char*)tempPixels, (unsigned char*)bitmapPixels);

    //release opencl resources
    checkCL(clReleaseMemObject(d_src));
    checkCL(clReleaseMemObject(d_dst));
    checkCL(clReleaseProgram(program));
    checkCL(clReleaseKernel(kernel));
    checkCL(clReleaseCommandQueue(queue));
    checkCL(clReleaseContext(context));

    AndroidBitmap_unlockPixels(env, bitmap);

    //release host memory
    free(tempPixels);
    free(bitmapPixels);

    return bitmap;
}

