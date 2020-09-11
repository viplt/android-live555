package com.czt.mp3recorder;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Handler;
import android.util.Log;

import com.BaseRecorder;
import com.czt.mp3recorder.util.LameUtil;
import com.live555.push.InitCallback;
import com.live555.push.LivePush;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

public class MP3Recorder extends BaseRecorder {
    //=======================AudioRecord Default Settings=======================
    private static final int DEFAULT_AUDIO_SOURCE = MediaRecorder.AudioSource.MIC;
    /**
     * 以下三项为默认配置参数。Google Android文档明确表明只有以下3个参数是可以在所有设备上保证支持的。
     */
    private static final int DEFAULT_SAMPLING_RATE = 32000;//模拟器仅支持从麦克风输入8kHz采样率
    //    private static final int DEFAULT_SAMPLING_RATE = 44100;//模拟器仅支持从麦克风输入8kHz采样率
    private static final int DEFAULT_CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_MONO;
    /**
     * 下面是对此的封装
     * private static final int DEFAULT_AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT;
     */
    private static final PCMFormat DEFAULT_AUDIO_FORMAT = PCMFormat.PCM_16BIT;

    //======================Lame Default Settings=====================
//    private static final int DEFAULT_LAME_MP3_QUALITY = 7;
    private static final int DEFAULT_LAME_MP3_QUALITY = 9;
    /**
     * 与DEFAULT_CHANNEL_CONFIG相关，因为是mono单声，所以是1
     */
//    private static final int DEFAULT_LAME_IN_CHANNEL = 1;
    private static final int DEFAULT_LAME_IN_CHANNEL = 2;
    /**
     * Encoded bit rate. MP3 file will be encoded with bit rate 32kbps
     */
    private static final int DEFAULT_LAME_MP3_BIT_RATE = 320;//320目前是正常
//    private static final int DEFAULT_LAME_MP3_BIT_RATE = 16;

    //==================================================================

    /**
     * 自定义 每160帧作为一个周期，通知一下需要进行编码
     */
    private static final int FRAME_COUNT = 320;
    public static final int ERROR_TYPE = 22;


    private AudioRecord mAudioRecord = null;

    AudioTrack audioTrack;//边录边播
    private DataEncodeThread mEncodeThread;
    private File mRecordFile;
    private ArrayList<Short> dataList;
    private Handler errorHandler;


    private short[] mPCMBuffer;
    private byte[] mPCMBufferAudioData;
    private boolean mIsRecording = false;
    private boolean mSendError;
    private boolean mPause;
    //缓冲数量
    private int mBufferSize;
    private int playBufSize;
    //最大数量
    private int mMaxSize;
    //波形速度
//    private int mWaveSpeed = 300;
    private int mWaveSpeed = 16;


    private byte[] mMp3Buffer;
    private List<byte[]> buffers;
    private FileOutputStream mFileOutputStream;
    private boolean clearFlag = false;//是否要清空数据

    private InitCallback callback;
    private LivePush livePush;
    boolean isPush = false;//是否正在推数据
    /**
     * Default constructor. Setup recorder with default sampling rate 1 channel,
     * 16 bits pcm
     *
     * @param recordFile target file
     */
    public MP3Recorder(File recordFile) {
        mRecordFile = recordFile;
    }

    /**
     * Start recording. Create an encoding thread. Start record from this
     * thread.
     *
     * @throws IOException initAudioRecorder throws
     */
    public void start(InitCallback cback, LivePush lpush) throws IOException {
        if (mIsRecording) {
            return;
        }
        clearFlag = false;
        mIsRecording = true; // 提早，防止init或startRecording被多次调用
        this.livePush = lpush;
        initAudioRecorder();
        try {
            mAudioRecord.startRecording();
            this.callback = cback;
            audioTrack.play();//开始播放
            mFileOutputStream = new FileOutputStream(mRecordFile);
            mMp3Buffer = new byte[(int) (7200 + (mBufferSize * 2 * 1.25))];
            buffers = new ArrayList<>();

        } catch (Exception ex) {
            ex.printStackTrace();
        }
        new Thread() {
            boolean isError = false;
            @Override
            public void run() {
                //设置线程权限
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);

                callback.onCallback("");//通知主进程开始播放

                while (mIsRecording) {
                    try{
                        int readSize = mAudioRecord.read(mPCMBuffer, 0, mBufferSize);
//                        int readSize = mAudioRecord.read(mPCMBufferAudioData, 0, mBufferSize);
//                    short[] tmpBuf = new short[readSize];
//                    System.arraycopy(mPCMBuffer, 0, tmpBuf, 0, readSize);
                        //写入数据即播放
                    audioTrack.write(mPCMBuffer, 0, readSize);

                        if (readSize == AudioRecord.ERROR_INVALID_OPERATION ||
                                readSize == AudioRecord.ERROR_BAD_VALUE) {
                            if (errorHandler != null && !mSendError) {
                                mSendError = true;
                                errorHandler.sendEmptyMessage(ERROR_TYPE);
                                mIsRecording = false;
                                isError = true;
                            }
                        } else {
                            if (readSize > 0) {
                                if (mPause) {
                                    continue;
                                }

                                //保存数据方案1，保存原生数据进行推流
                                //pcm存------
//                                byte[] bytes = new byte[mPCMBuffer.length * 2];
//                                ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().put(mPCMBuffer);
//                                mFileOutputStream.write(bytes, 0, readSize);
//                                mFileOutputStream.flush();

                                //同步处理音频数据
                                long st = System.currentTimeMillis();
//                                int encodedSize = 0;//临时调试不走转mp3---注释打开下面代码则执行同步转mp3推流
                                //保存数据方案2，保存转换后的mp3数据进行推流
                                int encodedSize = LameUtil.encode(mPCMBuffer, mPCMBuffer, readSize, mMp3Buffer);
                                if (encodedSize > 0) {
                                    try {
                                        mFileOutputStream.write(mMp3Buffer, 0, encodedSize);
                                        mFileOutputStream.flush();
                                        Log.i("MP3Recorder-------",mMp3Buffer+"----------encodedSize："+encodedSize+"------------buf:"+mMp3Buffer[0]);
                                    } catch (Exception e) {
                                        e.printStackTrace();
                                        Log.i("MP3Recorder-------","mFileOutputStream:" + e.getMessage());
                                    }
                                    long et = System.currentTimeMillis();
                                    Log.i("MP3Recorder-------",mMp3Buffer+"----------"+readSize+"--mFileOutputStream----"+encodedSize+"---buffers.size:"+buffers.size()+"--用时:" + (et-st));
                                }
                            } else {
                                if (errorHandler != null && !mSendError) {
                                    mSendError = true;
                                    mIsRecording = false;
                                    isError = true;
                                }
                            }
                        }
                    }catch (Exception e){
                        e.printStackTrace();
                    }
                }
                try {
//                    audioTrack.stop();
//                    audioTrack.release();

                    LameUtil.flush(mMp3Buffer);
                    LameUtil.close();

                    // release and finalize audioRecord
                    mAudioRecord.stop();
                    mAudioRecord.release();
                    mAudioRecord = null;

                    if (mFileOutputStream != null) {
                        try {
                            mFileOutputStream.flush();
                        } catch (IOException e) {
                            e.printStackTrace();
                        }finally {
                            mFileOutputStream.close();
                        }
                    }



                } catch (Exception ex) {
                    ex.printStackTrace();
                }
            }

        }.start();
    }

    /**
     * 获取真实的音量。 [算法来自三星]
     *
     * @return 真实音量
     */
    @Override
    public int getRealVolume() {
        return mVolume;
    }

    private static final int MAX_VOLUME = 2000;

    /**
     * 根据资料假定的最大值。 实测时有时超过此值。
     *
     * @return 最大音量值。
     */
    public int getMaxVolume() {
        return MAX_VOLUME;
    }

    public void stop() {
        mPause = false;
        mIsRecording = false;
    }

    public boolean isRecording() {
        return mIsRecording;
    }

    /**
     * Initialize audio recorder
     */
    private void initAudioRecorder() throws IOException {
        mBufferSize = AudioRecord.getMinBufferSize(DEFAULT_SAMPLING_RATE,
                DEFAULT_CHANNEL_CONFIG, DEFAULT_AUDIO_FORMAT.getAudioFormat());
//        mBufferSize = 1024;
//        playBufSize = AudioTrack.getMinBufferSize(DEFAULT_SAMPLING_RATE,
//                AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT);

        int bytesPerFrame = DEFAULT_AUDIO_FORMAT.getBytesPerFrame();
        /* Get number of samples. Calculate the buffer size
         * (round up to the factor of given frame size)
         * 使能被整除，方便下面的周期性通知
         * */
        int frameSize = mBufferSize / bytesPerFrame;
        if (frameSize % FRAME_COUNT != 0) {
            frameSize += (FRAME_COUNT - frameSize % FRAME_COUNT);
            mBufferSize = frameSize * bytesPerFrame;
        }

        /* Setup audio recorder */
        mAudioRecord = new AudioRecord(DEFAULT_AUDIO_SOURCE,
                DEFAULT_SAMPLING_RATE, DEFAULT_CHANNEL_CONFIG, DEFAULT_AUDIO_FORMAT.getAudioFormat(),
                mBufferSize);

//        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, DEFAULT_SAMPLING_RATE,
//                AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT,
//                playBufSize, AudioTrack.MODE_STREAM);


        mPCMBuffer = new short[mBufferSize];
        mPCMBufferAudioData = new byte[mBufferSize];
        /*
         * Initialize lame buffer
         * mp3 sampling rate is the same as the recorded pcm sampling rate
         * The bit rate is 32kbps
         *
         */
        LameUtil.init(DEFAULT_SAMPLING_RATE, DEFAULT_LAME_IN_CHANNEL, DEFAULT_SAMPLING_RATE, DEFAULT_LAME_MP3_BIT_RATE, DEFAULT_LAME_MP3_QUALITY);
    }

    public boolean isPause() {
        return mPause;
    }

    /**
     * 是否暂停
     */
    public void setPause(boolean pause) {
        this.mPause = pause;
    }

    public static void deleteFile(String filePath) {
        File file = new File(filePath);
        if (file.exists()) {
            if (file.isFile()) {
                file.delete();
            } else {
                String[] filePaths = file.list();
                for (String path : filePaths) {
                    deleteFile(filePath + File.separator + path);
                }
                file.delete();
            }
        }
    }
}