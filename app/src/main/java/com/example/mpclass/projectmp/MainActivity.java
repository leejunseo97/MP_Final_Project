package com.example.mpclass.projectmp;

//기본으로 있는 헤더들..
import androidx.appcompat.app.AppCompatActivity;
import android.hardware.Camera;
import android.os.Bundle;
import android.widget.ImageView;
import android.widget.TextView;

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

public class MainActivity extends AppCompatActivity implements JNIListener {

    //OpenCL 드라이버 라이브러리 로드
    static {
        System.loadLibrary("OpenCLDriver");
    }
    public native Bitmap GaussianBlurBitmap(Bitmap bitmap);
    public native Bitmap GaussianBlurGPU(Bitmap bitmap);

    //이미지 버퍼
    Bitmap org_img;
    Bitmap buf_img;

    //카메라 관련
    private Camera mCamera;
    private CameraPreview mPreview;
    private ImageView capturedImageHolder;

    //버튼 인터럽트 관련
    JNIDriver mDriver;
    boolean mThreadRun = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //카메라 관련
        mCamera = getCameraInstance(); //카메라 객체 생성
        mCamera.setDisplayOrientation(180); //카메라 이미지를 180도 뒤집어 준다.

        mPreview = new CameraPreview(this, mCamera); //카메라 프리뷰 객체 생성 및 할당
        FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview); //프레임레이아웃 객체 선언,생성,할당
        preview.addView(mPreview); //프리뷰 객체와 프레임레이아웃 연결

        capturedImageHolder = (ImageView) findViewById(R.id.processed_image); //이미지뷰 객체 생성 및 할당

        //GPIO 버튼 이용 인터럽트 관련
        mDriver = new JNIDriver();
        mDriver.setListener(this);
        if (mDriver.open("/dev/sm9s5422_interrupt") < 0) {
            Toast.makeText(MainActivity.this, "Driver Open Failed", Toast.LENGTH_SHORT).show();
            Log.e("MainActivity::", "인터럽트 드라이버 읽어오기 실패! ");
        }

        //당장은 안쓰지만 onClickListener 예시로 남겨놓는다.
//        Button btn = (Button) findViewById((R.id.button_capture)); //버튼과 btn연결
//        btn.setOnClickListener(new View.OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                mCamera.takePicture(null, null, pictureCallback); //클릭되면 콜백함수 실행
//            }
//        });
    }

    //실제 카메라를 할당해주는 함수
    public static Camera getCameraInstance() {
        Camera c = null;
        try {
            c = Camera.open();
        } catch (Exception e) {
            Log.e("MainActivity::", "getCameraInstance()에러! ");
        }
        return c;
    }


    //인터럽트 핸들
    public Handler handler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.arg1) {
                case 1: //그레이 스케일
                    if(org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Gray Scale::", "회색화 할 이미지 없음!");
                    }
                    else {
                        Toast.makeText(MainActivity.this, "Gray Scale", Toast.LENGTH_SHORT).show();
                        buf_img = Bitmap.createBitmap(org_img);
                        capturedImageHolder.setImageBitmap(toGray(buf_img));
                        Log.i("Gray Scale::", "이미지 회색화 완료");
                    }
                    break;
                case 2: //원본 이미지
                    if(org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Original Image::", "원본 이미지 없음!");
                    }
                    else {
                        Toast.makeText(MainActivity.this, "Origianl Image", Toast.LENGTH_SHORT).show();
                        capturedImageHolder.setImageBitmap(org_img);
                    }
                    break;
                case 3: //씨비유 블러
                    if(org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Blur CPU::", "CPU blur 할 이미지 없음!");
                    }
                    else {
                        buf_img = Bitmap.createBitmap(org_img);
                        capturedImageHolder.setImageBitmap(GaussianBlurBitmap(buf_img));
                        Log.i("Blur CPU::", "CPU blur 완료");
                        Toast.makeText(MainActivity.this, "Blur using CPU", Toast.LENGTH_SHORT).show();
                    }
                    break;
                case 4: //지피유 블러
                    if(org_img == null) {
                        Toast.makeText(MainActivity.this, "!! Take Picture first !!", Toast.LENGTH_SHORT).show();
                        Log.i("Blur GPU::", "GPU blur 할 이미지 없음!");
                    }
                    else {
                        Toast.makeText(MainActivity.this, "Blur using GPU", Toast.LENGTH_SHORT).show();
                        buf_img = Bitmap.createBitmap(org_img);
                        capturedImageHolder.setImageBitmap(GaussianBlurGPU(buf_img));
                        Log.i("Blur GPU::", "GPU blur 완료!");
                    }
                    break;
                case 5:
                    Toast.makeText(MainActivity.this, "Taking Picture", Toast.LENGTH_SHORT).show();
                    mCamera.takePicture(null, null, pictureCallback);
                    break;
            }
        }
    };

    //콜백함수 정의 - 찍은 사진의 해상도를 줄여서 썸네일로 이미지뷰에 출력
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
            Log.i("Tacking Picture::","해상도 줄이기 완료 ");
            //썸네일 이미지를 180도 돌려주기
            Matrix mtx = new Matrix();
            mtx.postRotate(180);
            Bitmap rotated_img = Bitmap.createBitmap(resized_img, 0, 0, resized_img.getWidth(), resized_img.getHeight(), mtx, true);
            Log.i("Tacking Picture::","이미지 회전 완료 ");
            //다른 이미지 처리에서 쓰기 위해서 버퍼에 이미지 복사
            org_img = rotated_img;
            Log.i("Tacking Picture::","이미지 버퍼에 저장 완료 ");
            //이미지뷰에 처리한 이미지 담기
            capturedImageHolder.setImageBitmap(rotated_img);
            Log.i("Tacking Picture::","이미지 뷰에 처리한 이미지 담기 완료 ");
            Log.i("Tacking Picture::", "처리된 이미지 가로 = " + rotated_img.getWidth());
            Log.i("Tacking Picture::", "처리된 이미지 세로 = " + rotated_img.getHeight());
        }
    };

    //비트맵 이미지를 회색조로 바꾸는 함수
    private Bitmap toGray(Bitmap rgbImage) {
        double redVal = 0.299;
        double greenVal = 0.587;
        double blueVal = 0.114;
        int A, R, G, B, GRAY;
        int pixel;
        int w = rgbImage.getWidth();
        int h = rgbImage.getHeight();
        Bitmap grayImage = Bitmap.createBitmap(w,h, rgbImage.getConfig());

        for(int i = 0; i<w; ++i) { //i++ 하면 안된다.
            for(int j=0; j<h; ++j) { //j++ 하면 안된다.
                pixel = rgbImage.getPixel(i,j);
                A = Color.alpha(pixel);
                R = Color.red(pixel);
                G = Color.green(pixel);
                B = Color.blue(pixel);
                GRAY = (int) (redVal*R + greenVal*G + blueVal*B);
                grayImage.setPixel(i, j, Color.argb(A,GRAY, GRAY, GRAY));
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

    //인터럽트 마무리 관련
    @Override
    protected void onPause() {
        super.onPause();
        releaseMediaRecorder();
        releaseCamera();
        mDriver.close();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    public void onReceive(int val) {
        Message text = Message.obtain();
        text.arg1 = val;
        handler.sendMessage(text);
    }
}

//    PictureCallback pictureCallback = new PictureCallback() {
//        @Override
//        public void onPictureTaken(byte[] data, Camera camera) {
//            Bitmap bitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
//            int w = bitmap.getWidth()/2;
//            int h = bitmap.getHeight()/2;
//
//            Matrix mtx = new Matrix();
//            mtx.postRotate(180); //캡처 화면을 180도 뒤집어 준다.
//            Bitmap rotatedBitmap = Bitmap.createBitmap(bitmap, 0, 0, w, h, mtx, true);
//
//            Bitmap grayBitmap = toGray(rotatedBitmap); //그레이스케일 하는 부분
//
//            if(bitmap == null) {
//                Toast.makeText(MainActivity.this, "Captured image is empty", Toast.LENGTH_SHORT).show();
//                return;
//            }
//            capturedImageHolder.setImageBitmap(scaleDownBitmapImage(rotatedBitmap, 450, 300));
//        }
//    };

//    //비트맵 크기 줄여주는 함수 (안하면 메모리 오류나서 캡처가 안된다.)
//    private Bitmap scaleDownBitmapImage(Bitmap bitmap, int newWidth, int newHeight) {
//        Bitmap resizedBitmap = Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, true);
//        return resizedBitmap;
//    }