package cn.xag.androidforlive555;

import android.Manifest;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.provider.Settings;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.os.Environment;
import android.widget.Toast;

import com.czt.mp3recorder.MP3Recorder;
import com.live555.push.InitCallback;
import com.live555.push.LivePush;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity{

//    Mp3Recorder mp3Recorder;
    MP3Recorder mp3Recorder;

    private String path = Environment.getExternalStorageDirectory()
            + "/AudioRecorderMp3/recorder/";
    String[] permiss = new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.RECORD_AUDIO};
    int REQUEST_CODE = 1002;

    String filePath;

    // Used to load the 'native-lib' library on application startup.
    static {
//        System.loadLibrary("native-lib");
//        System.loadLibrary("live555-rtsp");
    }

    LivePush livePush;

    private Context context;

    AudioRecorder ar = new AudioRecorder();
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        if(!checkMyPermission(permiss)){
            ActivityCompat.requestPermissions(this,permiss,REQUEST_CODE);
        }
        final TextView textView = findViewById(R.id.sample_text);
        final Button recbtn = findViewById(R.id.recbtn);
        final Button playbtn = findViewById(R.id.playbtn);

        final String mp3 = copyMp3ToSdcard("I still believe(cover).mp3");
        livePush = new LivePush();
        textView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                livePush.startPush(null,mp3);
//                if(livePush.isConn && livePush.pushing){
//                    livePush.stopPlay();
//                }else if(livePush.isConn && !livePush.pushing){
//                    livePush.startPlay();
//                }
            }
        });
        recbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(mp3Recorder != null && mp3Recorder.isRecording()){
                    //如果已启动就停止
                    mp3Recorder.stop();
                    mp3Recorder = null;
                    if(livePush != null){
                        if(livePush.isConn && livePush.pushing){
                            livePush.stopPlay();
                            livePush = null;
                        }
                    }
                    return;
                }
                String filePath =  Environment.getExternalStorageDirectory()
                        + "/AudioRecorderMp3/recorder/" + "ann.mp3";
                filePath = copyMp3ToSdcard("ann.mp3");//推录音开启
//                filePath = copyMp3ToSdcard("I still believe(cover).mp3");//推mp3开启
                if (livePush != null && livePush.isConn && livePush.pushing) {
                    livePush.stopPlay();
                    livePush = null;
                }
                livePush = new LivePush();
                File mp3File = new File(filePath);//这里通过app内复制一份空的mp3文件到指定目录进行推，如果不复制一份空的mp3文件，直接用创建的方式会出现首次推流量声音不对
                mp3Recorder = new MP3Recorder(mp3File);
                if(!mp3File.getParentFile().exists()){
                    mp3File.getParentFile().mkdirs();
                }
                //初始化推流 本地安装easyDarwin服务端
                livePush.initPush(v.getContext(), "192.168.3.137", "554", "test.sdp", filePath, new InitCallback() {
                    @Override
                    public void onCallback(String code) {
                        System.out.println(code);
                        if (LivePush.OnInitPusherCallback.CODE.LIVE_PUSH_STATE_CONNECTED == Integer.valueOf(code)) {
                            try{
                                //推录音
                                mp3Recorder.start(new InitCallback(){
                                    @Override
                                    public void onCallback(String code) {
                                        //录音启动后，开始推流量
                                        livePush.startPlay();
                                    }
                                    @Override
                                    public void onSuccess(String code) {
                                    }
                                },livePush);

                                //直接推mp3
//                                livePush.startPlay();
                            }catch (Exception e){}
                            if (LivePush.OnInitPusherCallback.CODE.LIVE_PUSH_STATE_PLAYOVER == Integer.valueOf(code)) {
//                                livePush.startPlay();//循环播放---如果是播放录音则注释
                            }
                        }
                    }

                    @Override
                    public void onSuccess(String code) {
                        System.out.println(code);
                    }
                });
            }


        });


        playbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try{//播放未实现，可能用VLC media player客户端播放
                    String rtspUrl = "rtsp://127.0.0.1:554/test.sdp";
                    String res = livePush.playRtsp(rtspUrl,"live555  MPEG-1 or 2 Audio, streamed by the HUAYIN Media Server");
                    System.out.println("res----------"+res);
                    return;
                }catch (Exception e){
                    e.printStackTrace();
                }

            }
        });
    }

    private boolean checkMyPermission(String[] permiss){
        if(permiss !=null && permiss.length > 0 ){
            for(String per : permiss) {
                if (ContextCompat.checkSelfPermission(this,per) != PackageManager.PERMISSION_GRANTED){
                    return false;
                }
            }
        }
        return true;
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults){
        if (requestCode == REQUEST_CODE) {
            boolean isAllGranted = true;
            // 判断是否所有的权限都已经授予了
            for (int grant : grantResults) {
                if (grant != PackageManager.PERMISSION_GRANTED) {
                    isAllGranted = false;
                    break;
                }
            }
            if (isAllGranted) {
            } else {
                // 弹出对话框告诉用户需要权限的原因, 并引导用户去应用权限管理中手动打开权限按钮
                openAppDetails();
            }
        }
    }
    /**
     * 打开 APP 的详情设置
     */
    private void openAppDetails() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage("录音需要访问 “外部存储器”，请到 “应用信息 -> 权限” 中授予！");
        builder.setPositiveButton("去手动授权", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                Intent intent = new Intent();
                intent.setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                intent.addCategory(Intent.CATEGORY_DEFAULT);
                intent.setData(Uri.parse("package:" + getPackageName()));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
                intent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
                startActivity(intent);
            }
        });
        builder.setNegativeButton("取消", null);
        builder.show();
    }


    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if(keyCode == KeyEvent.KEYCODE_BACK) {
            if (mp3Recorder != null && mp3Recorder.isRecording()) {
                new AlertDialog.Builder(this).setTitle("提示").setMessage("正在录音中，是否保存正在录制的音频?").setPositiveButton("是", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        try {
                            mp3Recorder.stop();
                            Toast.makeText(MainActivity.this, "结束录音!", Toast.LENGTH_SHORT).show();
                            mp3Recorder = null;
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        MainActivity.this.finish();
                        System.exit(0);
                    }
                }).setNegativeButton("否", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        try {
                            mp3Recorder.stop();
                            Toast.makeText(MainActivity.this, "结束录音!", Toast.LENGTH_SHORT).show();
                            mp3Recorder = null;
                            FileUtils.deleteFile(filePath);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        MainActivity.this.finish();
                        System.exit(0);
                    }
                }).create().show();
            }
            return false;
        }
        return super.onKeyDown(keyCode, event);
    }

    private String copyMp3ToSdcard(String filepath) {
        File file = new File(Environment.getExternalStorageDirectory(), filepath);
        if (file.exists()) {
            file.delete();
        }
        InputStream inputStream = null;
        FileOutputStream fileOutputStream = null;
        try {
            inputStream = getAssets().open(filepath);
            fileOutputStream = new FileOutputStream(file);
            byte[] buf = new byte[1024];
            int count;
            while ((count = inputStream.read(buf)) > 0) {
                fileOutputStream.write(buf, 0, count);
            }
            fileOutputStream.flush();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (inputStream != null) {
                    inputStream.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
            try {
                if (fileOutputStream != null) {
                    fileOutputStream.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        return file.getAbsolutePath();
    }

    private String copyTestFileToSdcard() {
        File file = new File(Environment.getExternalStorageDirectory(), "rtsp_test_butterfly.h264");
        if (file.exists()) {
            file.delete();
        }
        InputStream inputStream = null;
        FileOutputStream fileOutputStream = null;
        try {
            inputStream = getAssets().open("butterfly.h264");
            fileOutputStream = new FileOutputStream(file);
            byte[] buf = new byte[1024];
            int count;
            while ((count = inputStream.read(buf)) > 0) {
                fileOutputStream.write(buf, 0, count);
            }
            fileOutputStream.flush();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (inputStream != null) {
                    inputStream.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
            try {
                if (fileOutputStream != null) {
                    fileOutputStream.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        return file.getAbsolutePath();
    }
}
