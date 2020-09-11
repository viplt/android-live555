package com.live555.push;


import android.content.Context;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.util.Log;

import java.util.List;

/**
 * 推流so调用类
 */
public class LivePush {
    private static String TAG = "LivePush live555";
    static {
        System.loadLibrary("live555-rtsp");
    }


    public native String init(String serverIP,String steamName,String filepath,String rangeLine,String auxSDPLine,int sampleRate,OnInitPusherCallback callback);

    public native String start(String filePath);
    public native String start(String serverIP, String serverPort, String streamName,String fileName);

//    private native void push(long pusherObj, byte[] data, int offset, int length, long timestamp, int type);

    private native String pause();
    private native String play();
    private native String stop();
    private native String push(byte[] data,OnInitPusherCallback callback);
    private native String pushList(List<byte[]> data,OnInitPusherCallback callback);

    public boolean pushing = false;
    public boolean isConn = false;

    public interface OnInitPusherCallback {
        public void onCallback(String code);
        public void onSuccess(String code);

        static class CODE {
            public static final int LIVE_ACTIVATE_INVALID_KEY = -1;       //无效Key
            public static final int LIVE_ACTIVATE_TIME_ERR = -2;       //时间错误
            public static final int LIVE_ACTIVATE_PROCESS_NAME_LEN_ERR = -3;       //进程名称长度不匹配
            public static final int LIVE_ACTIVATE_PROCESS_NAME_ERR = -4;       //进程名称不匹配
            public static final int LIVE_ACTIVATE_VALIDITY_PERIOD_ERR = -5;       //有效期校验不一致
            public static final int LIVE_ACTIVATE_PLATFORM_ERR = -6;          //平台不匹配
            public static final int LIVE_ACTIVATE_COMPANY_ID_LEN_ERR = -7;          //授权使用商不匹配
            public static final int LIVE_ACTIVATE_SUCCESS = 0;        //激活成功
            public static final int LIVE_PUSH_STATE_CONNECTING = 1;        //连接中
            public static final int LIVE_PUSH_STATE_CONNECTED = 2;        //连接成功
            public static final int LIVE_PUSH_STATE_CONNECT_FAILED = 3;        //连接失败
            public static final int LIVE_PUSH_STATE_CONNECT_ABORT = 4;        //连接异常中断
            public static final int LIVE_PUSH_STATE_PUSHING = 5;        //推流中
            public static final int LIVE_PUSH_STATE_DISCONNECTED = 6;        //断开连接
            public static final int LIVE_PUSH_STATE_ERROR = 7;//
            public static final int LIVE_PUSH_STATE_PLAYOVER = 7;//播放结束
            public static final int LIVE_PUSH_STATE_STOP = 8;//停止推
            public static final int LIVE_PUSH_STATE_PAUSE = 9;//暂停
        }

    }

    private long mPusherObj = 0;
    private String jniRes;
    public synchronized void initPush(Context context,String serverIp,String port,String steamName,String filepath, final InitCallback callback) {
        Log.i(TAG, "PusherStart");
        //获取音频文件时长
        double fileTime = 0d;
        /*       rangeLine  auxSDPLine
        ---------因为服务器接收端要用的range和audiofmt这两个参数做判断，本人live555不熟悉，只能硬生在java里拼好传入底层处理
        */
        String rangeLine = "a=range:npt=0--1.000\r\n";//应该是录音时传
        String auxSDPLine = "";//设备播放时需要的参数
        int sampleRate = 44100;
        try{
            if(!filepath.contains("ann.mp3") && !filepath.contains("inn.mp3") && !filepath.contains("pcm")){
                MediaExtractor mex = new MediaExtractor();
                mex.setDataSource(filepath);// the adresss location of the sound on sdcard.
                MediaFormat mf = mex.getTrackFormat(0);
                int bitRate = mf.getInteger(MediaFormat.KEY_BIT_RATE);
                sampleRate = mf.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                fileTime = mf.getLong(MediaFormat.KEY_DURATION) / 1000;//时长
                fileTime = fileTime/1000;
                int channelCount = mf.getInteger(MediaFormat.KEY_CHANNEL_COUNT);//通道
                auxSDPLine = "a=audiofmt:14/16/"+sampleRate+"/"+channelCount+"\r\n";
                rangeLine = "a=range:npt=0-"+fileTime+"\r\n";
            }else{
                MediaExtractor mex = new MediaExtractor();
                mex.setDataSource(filepath);// the adresss location of the sound on sdcard.
                MediaFormat mf = mex.getTrackFormat(0);
                int bitRate = mf.getInteger(MediaFormat.KEY_BIT_RATE);
                sampleRate = mf.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                int channelCount = mf.getInteger(MediaFormat.KEY_CHANNEL_COUNT);//通道
                auxSDPLine = "a=audiofmt:1/16/32000/"+channelCount+"\r\n";//ann
                rangeLine = "a=range:npt=0--1.000\r\n";
            }
        }catch (Exception e){
            e.printStackTrace();
        }
        jniRes = init(serverIp,steamName,filepath,rangeLine,auxSDPLine,sampleRate, new OnInitPusherCallback() {
            String code = "";
            @Override
            public void onCallback(String code) {
                Log.i(TAG, "code-----"+code);
                this.code = code;
                if(CODE.LIVE_PUSH_STATE_CONNECTED == Integer.valueOf(code)){
                    isConn = true;//连接成功
                }else if(CODE.LIVE_PUSH_STATE_CONNECT_FAILED == Integer.valueOf(code)){
                    isConn = false;//连接失败
                }else if(CODE.LIVE_PUSH_STATE_PUSHING == Integer.valueOf(code)){
                    pushing = true;//推流中
                }else if(CODE.LIVE_PUSH_STATE_PLAYOVER == Integer.valueOf(code)){
                    pushing = false;//播放完成
                }else if(CODE.LIVE_PUSH_STATE_STOP == Integer.valueOf(code)){
                    pushing = false;//停止推
                }
                if (callback != null) callback.onCallback(code);
            }

            @Override
            public void onSuccess(String code) {
                this.code = code;
                if (callback != null) callback.onCallback(code);
            }
        });
        Log.i(TAG, "initPush-----"+jniRes);
    }

    public synchronized void startPush(Context context,String filepath) {
        if(!isConn || pushing){
            return;
        }
        Log.i(TAG, "PusherStart");
        jniRes = start(filepath);
        Log.i(TAG, "initPush-----"+jniRes);
    }

    public synchronized void stopPlay(){
        if(isConn){
            if(pushing){
                pushing = false;
            }
            stop();
        }
    }

    public synchronized void startPlay(){
        if(isConn && !pushing){
            play();
        }
    }


    public String playRtsp(String url,String name){
        return "";
    }


    public void onSuccess(String code){
        Log.i(TAG, "onSucces-----"+code);
    }

    public void pushBuffer(byte[] mMp3Buffer,final InitCallback callback){
        try{
            pushing = true;
            push(mMp3Buffer,new OnInitPusherCallback() {
                @Override
                public void onCallback(String code) {
                    if (callback != null) callback.onCallback(code);
                }
                @Override
                public void onSuccess(String code) {
                    if (callback != null) callback.onCallback(code);
                }
            });
        }catch (Exception e){
        }
    }
    public void pushBuffer(List<byte[]> mMp3Buffer, final InitCallback callback){
        try{
            pushing = true;
            pushList(mMp3Buffer,new OnInitPusherCallback() {
                @Override
                public void onCallback(String code) {
                    if (callback != null) callback.onCallback(code);
                }
                @Override
                public void onSuccess(String code) {
                    if (callback != null) callback.onCallback(code);
                }
            });
        }catch (Exception e){
        }
    }
}
