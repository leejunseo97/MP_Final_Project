package com.example.mpclass.projectmp;

//기본으로 있는 헤더들..

import androidx.appcompat.app.AppCompatActivity;

import android.hardware.Camera;
import android.os.Bundle;
import android.widget.ImageView;
import android.widget.TextView;

//LED
import java.util.Arrays;
//로그
import android.util.Log;

//인터럽트 관련
import android.os.Handler;
import android.os.Message;

//카메라 관련
import android.graphics.Color;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.graphics.Bitmap;
import android.hardware.Camera.PictureCallback;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.Toast;

//OpenCV 관련
import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.Mat;
import org.opencv.imgproc.Imgproc;

public class MainActivity extends AppCompatActivity implements JNIListener {

    //OpenCL 드라이버 라이브러리 로드
    static {
        System.loadLibrary("OpenCLDriver");
        System.loadLibrary("OpenCV");
    }

    //OpenCL
    public native Bitmap GaussianBlurBitmap(Bitmap bitmap);

    public native Bitmap GaussianBlurGPU(Bitmap bitmap);

    //이미지 버퍼
    static Bitmap org_img;
    static Bitmap buf_img;

    //카메라 관련
    private static Camera mCamera;
    private static CameraPreview mPreview;
    private static ImageView capturedImageHolder;

    //GPIO 버튼
    static JNIDriver mDriver;
    boolean mThreadRun = true;

    //LED
    private native static int open_LED_Driver(String path);

    private native static int close_LED_Driver();

    public native static int write_LED_Driver(byte[] data, int length);

    static boolean led_run;
    static boolean led_start;
    LedThread mLedThread;
    static byte[] led_array = {0, 0, 0, 0, 0, 0, 0, 0};

    //7SEG
    private native static int open_SEG_Driver(String path);

    private native static int close_SEG_Driver();

    private native static int write_SEG_Driver(byte[] data, int length);

    static boolean seg_run;
    static SegmentThread mSegThread;
    static float start_t, end_t;
    static int sub_t;
    static byte[] seg_array = {0, 0, 0, 0, 0, 0};

    //OpenCV
    private native Bitmap detect_Edge(Bitmap bitmap);

    private native Bitmap detect_Traffic(Bitmap bitmap);

    private native Bitmap detect_Direction(Bitmap bitmap);


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

    }

    @Override
    protected void onPause() {
        super.onPause();
        releaseMediaRecorder();
        releaseCamera();
        mDriver.close();

        //LED
        led_run = false;
        mLedThread = null;
        close_LED_Driver();

        //7SEG
        seg_run = false;
        mSegThread = null;
        close_SEG_Driver();
    }

    @Override
    protected void onResume() {
        super.onResume();
        //LED - 드라이버 불러오고 쓰레드 객체 생성 후 실행
        if (open_LED_Driver("/dev/sm9s5422_led") < 0) {
            Toast.makeText(this, "LED Driver Open Failed", Toast.LENGTH_SHORT).show();
            Log.e("LED::", "LED 드라이버 불러오기 실패!");
        } else Log.i("LED::", "LED 드라이버 불러오기 성공!");
        led_run = true;
        led_start = false;
        mLedThread = new LedThread();
        mLedThread.start();

        //SEG - 드라이버 불러오고 쓰레드 객체 생성 후 실행
        if (open_SEG_Driver("/dev/sm9s5422_segment") < 0) {
            Toast.makeText(MainActivity.this, "Driver Open Failed", Toast.LENGTH_SHORT).show();
            Log.e("SEG::", "SEG 드라이버 불러오기 실패!");
        } else Log.i("SEG::", "SEG 드라이버 불러오기 성공!");
        seg_run = true;
        mSegThread = new SegmentThread();
        mSegThread.start();

        //카메라 관련
        mCamera = getCameraInstance(); //카메라 객체 생성
        mCamera.setDisplayOrientation(180); //카메라 이미지를 180도 뒤집어 준다.

        //프리뷰 관련
        mPreview = new CameraPreview(this, mCamera); //카메라 프리뷰 객체 생성 및 할당
        FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview); //프레임레이아웃 객체 선언,생성,할당
        preview.addView(mPreview); //프리뷰 객체와 프레임레이아웃 연결

        capturedImageHolder = (ImageView) findViewById(R.id.processed_image); //이미지뷰 객체 생성 및 할당

        //GPIO 버튼 이용 인터럽트 관련
        mDriver = new JNIDriver();
        mDriver.setListener(this);
        if (mDriver.open("/dev/sm9s5422_interrupt") < 0) {
            Toast.makeText(MainActivity.this, "Driver Open Failed", Toast.LENGTH_SHORT).show();
            Log.e("GPIO::", "인터럽트 드라이버 읽어오기 실패! ");
        } else Log.i("GPIO::", "인터럽트 드라이버 읽어오기 성공!");

        //opencv 관련
        Button btn_edge = (Button) findViewById(R.id.button1);
        btn_edge.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (org_img == null) {
                    Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                    Log.i("OpenCV::", "엣지 검출할 이미지 없음!");
                } else {
                    Toast.makeText(MainActivity.this, "Edge Detection", Toast.LENGTH_SHORT).show();
                    Log.i("OpenCV::", "엣지 검출 시작");
                    buf_img = Bitmap.createBitmap(org_img);
                    detect_Edge(buf_img);
                    capturedImageHolder.setImageBitmap(buf_img);
                    Log.i("OpenCV::", "엣지 검출 완료");
                }
            }
        });
        Button btn_traffic = (Button) findViewById(R.id.button2);
        btn_traffic.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                if (org_img == null) {
//                    Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
//                    Log.i("OpenCV::", "신호등 검출할 이미지 없음!");
//                } else {
//                    Toast.makeText(MainActivity.this, "Traffic Detection", Toast.LENGTH_SHORT).show();
//                    Log.i("OpenCV::", "신호등 검출 버튼 클릭");
//                    buf_img = Bitmap.createBitmap(org_img);
//                    Utils.bitmapToMat(buf_img, buf_mat);
//                    buf_mat = detect_Traffic(buf_mat);
//                    Utils.matToBitmap(buf_mat, buf_img);
//
//                }
            }
        });
        Button btn_direction = (Button) findViewById(R.id.button3);
        btn_direction.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Toast.makeText(MainActivity.this, "Direction Detection", Toast.LENGTH_SHORT).show();
                Log.i("OpenCV::", "방향 표지판 버튼 클릭");
//                buf_img = Bitmap.createBitmap(org_img);

            }
        });
    }

    @Override
    public void onReceive(int val) {
        Message text = Message.obtain();
        text.arg1 = val;
        handler.sendMessage(text);
    }

    //인터럽트 핸들러
    public Handler handler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.arg1) {
                case 1: //그레이 스케일
                    if (org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Gray Scale::", "회색화 할 이미지 없음!");
                    } else {
                        Toast.makeText(MainActivity.this, "Gray Scale", Toast.LENGTH_SHORT).show();
                        buf_img = Bitmap.createBitmap(org_img);
                        start_t = (float) System.nanoTime() / 1000000;
                        capturedImageHolder.setImageBitmap(toGray(buf_img));
                        end_t = (float) System.nanoTime() / 1000000;
                        sub_t = (int) (end_t - start_t);
                        Log.i("Gray Scale::", "이미지 회색화 완료");
                    }
                    break;
                case 2: //원본 이미지
                    if (org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Original Image::", "원본 이미지 없음!");
                    } else {
                        Toast.makeText(MainActivity.this, "Origianl Image", Toast.LENGTH_SHORT).show();
                        capturedImageHolder.setImageBitmap(org_img);
                        sub_t = 0;
                    }
                    break;
                case 3: //씨피유 블러
                    if (org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Blur CPU::", "CPU blur 할 이미지 없음!");
                    } else {
                        buf_img = Bitmap.createBitmap(org_img);
                        start_t = (float) System.nanoTime() / 1000000;
                        buf_img = GaussianBlurBitmap(buf_img);
                        end_t = (float) System.nanoTime() / 1000000;
                        sub_t = (int) (end_t - start_t);
                        capturedImageHolder.setImageBitmap(buf_img);
                        Log.i("Blur CPU::", "CPU blur 완료");
                        Toast.makeText(MainActivity.this, "Blur using CPU", Toast.LENGTH_SHORT).show();
                    }
                    break;
                case 4: //지피유 블러
                    if (org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Blur GPU::", "GPU blur 할 이미지 없음!");
                    } else {
                        Toast.makeText(MainActivity.this, "Blur using GPU", Toast.LENGTH_SHORT).show();
                        buf_img = Bitmap.createBitmap(org_img);
                        start_t = (float) System.nanoTime() / 1000000;
                        buf_img = GaussianBlurGPU(buf_img);
                        end_t = (float) System.nanoTime() / 1000000;
                        sub_t = (int) (end_t - start_t);
                        capturedImageHolder.setImageBitmap(buf_img);
                        Log.i("Blur GPU::", "GPU blur 완료!");
                    }
                    break;
                case 5:
                    Toast.makeText(MainActivity.this, "Taking Picture", Toast.LENGTH_SHORT).show();
                    start_t = (float) System.nanoTime() / 1000000;
                    mCamera.takePicture(null, null, pictureCallback);
                    end_t = (float) System.nanoTime() / 1000000;
                    sub_t = (int) (end_t - start_t);
                    led_start = true;
                    break;
            }
        }
    };

    //실제 카메라를 할당해주는 함수
    public static Camera getCameraInstance() {
        Camera c = null;
        try {
            c = Camera.open();
            Log.i("Camera::", "Camera.open() 성공 ");
        } catch (Exception e) {
            Log.e("Camera::", "getCameraInstance()에러! ");
        }
        return c;
    }

    //콜백함수 정의 - 찍은 사진의 해상도를 줄여서 이미지뷰에 출력
    PictureCallback pictureCallback = new PictureCallback() {
        @Override
        public void onPictureTaken(byte[] data, Camera camera) {
            //카메라 이미지 받아오기
            Bitmap src_img = BitmapFactory.decodeByteArray(data, 0, data.length);
            Log.i("Tacking Picture::", "원본 이미지 가로 = " + src_img.getWidth());
            Log.i("Tacking Picture::", "원본 이미지 세로 = " + src_img.getHeight());
            if (src_img == null) {
                Toast.makeText(MainActivity.this, "Captured image is empty", Toast.LENGTH_SHORT).show();
                Log.e("MainActivity::", "캡처 이미지 없음!");
                return;
            }
            //이미지 해상도 줄이기
            Bitmap resized_img = Bitmap.createScaledBitmap(src_img, (int) src_img.getWidth() / 8, (int) src_img.getHeight() / 8, true);
            Log.i("Tacking Picture::", "해상도 줄이기 완료 ");
            //썸네일 이미지를 180도 돌려주기
            Matrix mtx = new Matrix();
            mtx.postRotate(180);
            Bitmap rotated_img = Bitmap.createBitmap(resized_img, 0, 0, resized_img.getWidth(), resized_img.getHeight(), mtx, true);
            Log.i("Tacking Picture::", "이미지 회전 완료 ");
            //다른 이미지 처리에서 쓰기 위해서 버퍼에 이미지 복사
            org_img = rotated_img;
            Log.i("Tacking Picture::", "이미지 버퍼에 저장 완료 ");
            //이미지뷰에 처리한 이미지 담기
            capturedImageHolder.setImageBitmap(rotated_img);
            Log.i("Tacking Picture::", "이미지 뷰에 처리한 이미지 담기 완료 ");
            Log.i("Tacking Picture::", "처리된 이미지 가로 = " + rotated_img.getWidth());
            Log.i("Tacking Picture::", "처리된 이미지 세로 = " + rotated_img.getHeight());
        }
    };

    //LED 쓰레드
    private class LedThread extends Thread {
        public void run() {
            super.run();
            Log.i("LED::", "LED 쓰레드 실행! ");
            while (led_run) {
                if (led_start) {
                    for (int j = 0; j < 4; j++) {
                        for (int i = 0; i < 4; i++) {
                            led_array[2 * i] = 0;
                            led_array[2 * i + 1] = 1;
                        }
                        write_LED_Driver(led_array, led_array.length);
                        try {
                            Thread.sleep(200);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        for (int i = 0; i < 4; i++) {
                            led_array[2 * i] = 1;
                            led_array[2 * i + 1] = 0;
                        }
                        write_LED_Driver(led_array, led_array.length);
                        try {
                            Thread.sleep(200);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                    for (int i = 0; i < 8; i++) {
                        led_array[i] = 0;
                    }
                    write_LED_Driver(led_array, led_array.length);
                    led_start = false;
                }
            }
        }
    }

    //SEG 쓰레드
    private class SegmentThread extends Thread {
        @Override
        public void run() {
            super.run();
            Log.i("SEG::", "SEG 쓰레드 실행! ");
            while (seg_run) {
                seg_array[0] = (byte) (sub_t % 1000000 / 100000);
                seg_array[1] = (byte) (sub_t % 100000 / 10000);
                seg_array[2] = (byte) (sub_t % 10000 / 1000);
                seg_array[3] = (byte) (sub_t % 1000 / 100);
                seg_array[4] = (byte) (sub_t % 100 / 10);
                seg_array[5] = (byte) (sub_t % 10);
                write_SEG_Driver(seg_array, seg_array.length); //함수 실행하는데 10ms 정도 걸린다. 위의 계산도 시간이 걸리긴 함

            }
        }
    }

    //비트맵 이미지를 회색조로 바꾸는 함수
    private Bitmap toGray(Bitmap rgbImage) {
        double redVal = 0.299;
        double greenVal = 0.587;
        double blueVal = 0.114;
        int A, R, G, B, GRAY;
        int pixel;
        int w = rgbImage.getWidth();
        int h = rgbImage.getHeight();
        Bitmap grayImage = Bitmap.createBitmap(w, h, rgbImage.getConfig());

        for (int i = 0; i < w; ++i) { //i++ 하면 안된다.
            for (int j = 0; j < h; ++j) { //j++ 하면 안된다.
                pixel = rgbImage.getPixel(i, j);
                A = Color.alpha(pixel);
                R = Color.red(pixel);
                G = Color.green(pixel);
                B = Color.blue(pixel);
                GRAY = (int) (redVal * R + greenVal * G + blueVal * B);
                grayImage.setPixel(i, j, Color.argb(A, GRAY, GRAY, GRAY));
            }
        }
        return grayImage;
    }


    //카메라 자원 반환 관련
    private void releaseMediaRecorder() {
        mCamera.lock();
    }

    private void releaseCamera() {
        if (mCamera != null) {
            mCamera.release();
            mCamera = null;
        }
    }

}
