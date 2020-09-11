#include <jni.h>
#include <string>

#include "liveMedia/include/liveMedia.hh"
#include "groupsock/include/GroupsockHelper.hh"
#include "BasicUsageEnvironment/include/BasicUsageEnvironment.hh"
#include "liveMedia/include/DarwinInjector.hh"


#include <android/log.h>

#include<iostream>
#include <jni.h>
#include <jni.h>
#include "JavaListener.h"
#include "UsageEnvironment/include/UsageEnvironment.hh"
#include <jni.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>

#define LOG_TAG "rtsp_server"
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


/************Darwin****************/

UsageEnvironment* dar_env;
//char const* dar_inputFileName = "/storage/emulated/0/I still believe(cover).mp3";
//char const* dar_inputFileName;
//char const* dar_remoteStreamName; // the stream name, as served by the DSS
char const* dar_inputFileName;
u_int64_t seekPosByte = 0;//当前播放多少字节
char const* rangeLine;
char const* auxSDPLine;
unsigned rtpTimestampFrequency;
char const* dar_remoteStreamName;
char const* dssNameOrAddress;
FramedSource* wav_mp3_source;
RTPSink* dar_videoSink;
RTPSink* dar_audioSink;
DarwinInjector* injector;

char const* dar_programName;

Boolean isruning = false;
Boolean isplaying = false;

//定义一个全局 java vm 实例
JavaVM *jvm;
pthread_t childThread;
JavaListener *javaListener;

JavaListener *call_javaListener;//改成实时推流回调事件
const char *msg;//1连接中 2连接成功 3连接失败 4连接异常中断 5推流中 6断开连接 7播放结束 8暂停结束
//在加载动态库时调用 JNI_Onload 方法，在这里可以得到 JavaVM 实例对象
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    jvm = vm;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    return JNI_VERSION_1_6;
}
//在构造函数接收对应的参数
JavaListener::JavaListener(JavaVM *vm, JNIEnv *env, jobject jobj) {
    this->vm = vm;
    this->env = env;
    this->jobj = jobj;

    jclass jclaz = env->GetObjectClass(jobj);

    //得到 jmethodid 实例
    jmethod = env->GetMethodID(jclaz, "onCallback", "(Ljava/lang/String;)V");

}
//在子线程回调这个方法 onSuccess
void JavaListener::onSuccess(const char *msg) {

    //得到子线程相关的 JNIEnv 实例
    JNIEnv *env;
    vm->AttachCurrentThread(&env, 0);

    //将需要传递给 Java 层 onSuccess 的 msg 转化为 jstring 实例
    jstring jmsg = env->NewStringUTF(msg);

    //调用 Java 层的函数
    env->CallVoidMethod(jobj, jmethod, jmsg);
    //删除本地引用 jmsg，避免内存泄露
    env->DeleteLocalRef(jmsg);
    //取消挂载线程
    vm->DetachCurrentThread();
}

pthread_cond_t cond;
pthread_mutex_t mutex;
struct timeval now;
struct timespec outtime;
// 单位毫秒
void LTSleep(int nHm) {
    gettimeofday(&now, NULL);
    now.tv_usec += 1000*nHm;
    if (now.tv_usec > 1000000) {
        now.tv_sec += now.tv_usec / 1000000;
        now.tv_usec %= 1000000;
    }

    outtime.tv_sec = now.tv_sec;
    outtime.tv_nsec = now.tv_usec * 1000;

    pthread_cond_timedwait(&cond, &mutex, &outtime);
}


Boolean dar_awaitConfigInfo(RTPSink* sink); // forward
void dar_play(); // forward

int fd = 0;
int maxBufSize = 33000;

void dar_afterPlaying(void* clientData) {
    *dar_env << "...done reading from file\n";
//
    Medium::close(wav_mp3_source);
    dar_audioSink->resetPresentationTimes();
    if(strstr(dar_inputFileName,"ann.mp3")){
        //如果是ann.mp3文件，则表示是推实时录音文件---播放完，立马再循环播放
        dar_play();//开发时候开启
    }else if(strstr(dar_inputFileName,"inn.mp3")){
//        dar_audioSink->presetNextTimestamp();
        dar_play();//开发时候开启
    }else if(strstr(dar_inputFileName,"pcm")){
        dar_play();//开发时候开启
    }else{
        LOGI("live555 : %s play over ",dar_inputFileName);
        javaListener->onSuccess("7");
    }
//    dar_play();//开发时候开启
//    Medium::close(dar_audioSink);
//    Medium::close(injector);
}
//开始推流
void dar_play() {
    // Open the input file as a 'byte-stream file source':
    ByteStreamFileSource* fileSource = ByteStreamFileSource::createNew(*dar_env, dar_inputFileName);
    if (fileSource == NULL) {
        *dar_env << "Unable to open file \"" << dar_inputFileName
                 << "\" as a byte-stream file source\n";
//        exit(1);
        LOGE("live555 : %s file not found", dar_inputFileName);
        return;
    };
    *dar_env << "Beginning to read from file...======\n";
//    LOGI("live555 : Beginning to read from file...%s -----%llu",dar_inputFileName,fileSource->fileSize());
    if(strstr(dar_inputFileName,"ann.mp3")){
        //如果是播放实时录音文件，需要文件长度大于maxBufSize*2再播放
        if(seekPosByte == 0){
            if(fileSource->fileSize() < maxBufSize*2){
                LTSleep(200);
                dar_play();
                return;
            }
        }
        u_int64_t seekPos = 0;
        seekPosByte = fileSource->fileSize();
        seekPos = seekPosByte - 33000;//每次播放都从当前录音文件的最后1秒左右开始播放
        LOGI("live555 : Beginning to read from file......-----%s -----%llu-----%llu-----%llu",dar_inputFileName,seekPosByte,seekPos,(seekPosByte - seekPos));
        //----------执行下在一行代码，指定跳过字节就可以起到实时推的效果，但是每次开始推送10秒左右就开始就开始有杂音了
//        fileSource->seekToByteAbsolute(seekPos);
        dar_audioSink->resetPresentationTimes();//我也不知道有没有起作用先加上再说
    }else if(strstr(dar_inputFileName,"inn.mp3")){
        if(fileSource->fileSize() < maxBufSize){
            LTSleep(200);
            dar_play();
            return;
        }
        u_int64_t seekPos = fileSource->fileSize() - (maxBufSize);
        fileSource->seekToByteAbsolute(seekPos);
        dar_audioSink->resetPresentationTimes();
    }else{
//        u_int4_t seek6Pos = fileSource->fileSize() - (44100 * 3);//seekPos 160000 音乐文件跳过有5秒左右
//        fileSource->seekToByteAbsolute(seekPos);
        dar_audioSink->resetPresentationTimes();
    }

    FramedSource* wav_mp3_source;
    if(strstr(dar_inputFileName,".mp3")){
        //如果推mp3用MPEG1or2AudioStreamFramer
        FramedSource* source = fileSource;
        wav_mp3_source = MPEG1or2AudioStreamFramer::createNew(*dar_env, source);//推送MP3
        dar_audioSink->startPlaying(*wav_mp3_source, dar_afterPlaying, dar_audioSink);
    } else{
        //如果推wav、pcm用 WAVAudioFileSource 或者  uLawFromPCMAudioSource  都无效，不知道是不是哪用错了反反正都是吓猜的
        if(fileSource->fileSize() < maxBufSize*2){
            LTSleep(200);
            dar_play();
            return;
        }
//        WAVAudioFileSource* wavSource = WAVAudioFileSource::createNew(*dar_env, "/storage/emulated/0/AudioRecorderMp3/recorder/dddddd.wav");
//        wav_mp3_source = uLawFromPCMAudioSource::createNew(*dar_env, wavSource, 1/*little-endian*/);
//        dar_audioSink->startPlaying(*wav_mp3_source, dar_afterPlaying, dar_audioSink);
        wav_mp3_source = uLawFromPCMAudioSource::createNew(*dar_env, fileSource, 1/*little-endian*/);

//        wav_mp3_source = EndianSwap16::createNew(*dar_env, wavSource);
//        wav_mp3_source = PCMFromuLawAudioSource::createNew(*dar_env, wavSource);
        dar_audioSink->startPlaying(*wav_mp3_source, dar_afterPlaying, dar_audioSink);
    }

    javaListener->onSuccess("5");
}

static char doneFlag = 0;

static void checkForAuxSDPLine(void* clientData) {
    RTPSink* sink = (RTPSink*)clientData;
    if (sink->auxSDPLine() != NULL) {
        // Signal the event loop that we're done:
        doneFlag = ~0;
    } else {
        // No luck yet.  Try again, after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        dar_env->taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                     (TaskFunc*)checkForAuxSDPLine, sink);
    }
}

Boolean dar_awaitConfigInfo(RTPSink* sink) {
    // Check whether the sink's 'auxSDPLine()' is ready:
    checkForAuxSDPLine(sink);

    dar_env->taskScheduler().doEventLoop(&doneFlag);

    char const* auxSDPLine = sink->auxSDPLine();
    return auxSDPLine != NULL;
}


void *conn_childCallback(void *data) {
    //3. 在子线程中得到主线程传递过来的 JavaListener 实例
    JavaListener *javaListener = (JavaListener *) data;
    //4. 通过 JavaListener 去执行对应的 onSuccess 函数。
    msg = "2";
    javaListener->onSuccess(msg);
    //5. 退出线程
    pthread_exit(&childThread);
}
void *play_childCallback(void *data) {
    //3. 在子线程中得到主线程传递过来的 JavaListener 实例
//    JavaListener *javaListener = (JavaListener *) data;
    //4. 通过 JavaListener 去执行对应的 onSuccess 函数。
//    msg = "5";
//    javaListener->onSuccess(msg);
    dar_audioSink->resetPresentationTimes();//重新播放
    dar_play();//开始播放
    //5. 退出线程
    pthread_exit(&childThread);
}

void *pause_childCallback(void *data) {
    //3. 在子线程中得到主线程传递过来的 JavaListener 实例
    JavaListener *javaListener = (JavaListener *) data;
    //4. 通过 JavaListener 去执行对应的 onSuccess 函数。
    msg = "9";
    javaListener->onSuccess(msg);//暂停之后播放
    dar_audioSink->stopPlaying();//暂停播放
    //5. 退出线程
    pthread_exit(&childThread);
}
void *stop_childCallback(void *data) {
    isruning = false;
    isplaying = false;
    if(fd != 0){
        close(fd);
        fd = 0;
    }
    //3. 在子线程中得到主线程传递过来的 JavaListener 实例
    JavaListener *javaListener = (JavaListener *) data;
    //4. 通过 JavaListener 去执行对应的 onSuccess 函数。

    dar_audioSink->stopPlaying();//暂停播放

    dar_audioSink->close(wav_mp3_source);

    injector->close(dar_audioSink);

    Medium::close(wav_mp3_source);

    Medium::close(dar_audioSink);

    Medium::close(injector);

    doneFlag = ~0;
    dar_env->taskScheduler().doEventLoop(&doneFlag);//结束当前推流服务
    msg = "8";
    javaListener->onSuccess(msg);
    //5. 退出线程
    pthread_exit(&childThread);
//    pthread_exit(0);
}

//线程执行体------------初始调用化推流sink
void *childCallback(void *data) {

    //3. 在子线程中得到主线程传递过来的 JavaListener 实例
    JavaListener *javaListener = (JavaListener *) data;
    //4. 通过 JavaListener 去执行对应的 onSuccess 函数。
    javaListener->onSuccess(msg);

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    dar_env = BasicUsageEnvironment::createNew(*scheduler);
//    dar_inputFileName = dar_env->GetStringUTFChars(fileName_, 0);
    // Parse command-line arguments:
    dar_programName = "live555  MPEG-1 or 2 Audio, streamed by the HUAYIN Media Server";
    //if (argc != 2) dar_usage();
//    char const* dssNameOrAddress = "192.168.3.8";
    // Create a 'Darwin injector' object:
//    DarwinInjector* injector = DarwinInjector::createNew(*dar_env, dar_programName);
    injector = DarwinInjector::createNew(*dar_env, dar_programName);
    // Create 'groupsocks' for RTP and RTCP.
    // (Note: Because we will actually be streaming through a remote Darwin server,
    // via TCP, we just use dummy destination addresses, port numbers, and TTLs here.)
    struct in_addr dummyDestAddress;
    dummyDestAddress.s_addr = 0;
//    dummyDestAddress.s_addr = our_inet_addr(dssNameOrAddress);

    Groupsock rtpGroupsock(*dar_env, dummyDestAddress, 0, 0);
    Groupsock rtcpGroupsock(*dar_env, dummyDestAddress, 0, 0);

    //如果是mp3文件就用MPEG1or2AudioRTPSink
    if(strstr(dar_inputFileName,".mp3")){
        dar_audioSink = MPEG1or2AudioRTPSink::createNew(*dar_env, &rtpGroupsock);//推送MP3
        dar_audioSink->setRTPTimestampFrequency(rtpTimestampFrequency);
        dar_audioSink->setAuxSDPLine(auxSDPLine);//新增了方法
    }else{
        //否则就是wav或者pcm  ----但是这里推pcm和wav文件，一直没有成功过
        dar_audioSink = SimpleRTPSink::createNew(*dar_env, &rtpGroupsock,96, 32000, "audio", "L16", 1);
    }

    // HACK, specifically for MPEG-4 video:
    // Before we can use this RTP sink, we need its MPEG-4 'config' information (for
    // use in the SDP description).  Unfortunately, this config information depends
    // on the the properties of the MPEG-4 input data.  Therefore, we need to start
    // 'playing' this RTP sink from the input source now, and wait until we get
    // the needed config information, before continuing:
    // that we need:
//    *dar_env << "Beginning streaming...\n";
//    dar_play();

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 100; // in kbps; for RTCP b/w share  ---lt  默认500
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen+1];
    gethostname((char*)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case
    RTCPInstance* RTCP =
            RTCPInstance::createNew(*dar_env, &rtcpGroupsock,
                                    estimatedSessionBandwidth, CNAME,
                                    dar_audioSink, NULL /* we're a server */);
    // Note: This starts RTCP running automatically

    // Add these to our 'Darwin injector':
    injector->addStream(dar_audioSink, RTCP);

    // Next, specify the destination Darwin Streaming Server:
    if (!injector->setDestination(dssNameOrAddress, dar_remoteStreamName,
//                                  dar_programName, "/SONG/I still believe(cover).mp3 LIVE555 Streaming Media")) {
                                  dar_programName, dar_inputFileName,rangeLine,auxSDPLine)) {
        *dar_env << "injector->setDestination() failed: "
                 << dar_env->getResultMsg() << "\n";
        LOGE("live555 : injector->setDestination() failed: %s", dar_env->getResultMsg());
//        exit(1);
        //5. 退出线程
//        pthread_exit(&childThread);
//        //2. 创建子线程，将 javaListener 传递给线程执行体 childCallback
//        pthread_create(&childThread, NULL, conn_childCallback, javaListener);
        msg = "3";
        javaListener->onSuccess(msg);
        //5. 退出线程
        pthread_exit(&childThread);
        LOGE("live555 : exit");
    }
    msg = "2";
    javaListener->onSuccess(msg);

    *dar_env << "Play this stream (from the Darwin Streaming Server) using the URL:\n"
             << "\trtsp://" << dssNameOrAddress << "/" << dar_remoteStreamName << "\n";

    LOGI("live555 Play this stream (from the Darwin Streaming Server) using the URL:  rtsp://%s:554/%s", dssNameOrAddress,dar_remoteStreamName);

    *dar_env << "Beginning streaming...\n";
//    dar_play();

    dar_env->taskScheduler().doEventLoop(); // does not return
    //5. 退出线程
    pthread_exit(&childThread);
    LOGE("live555 : exit");
}


extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_init(JNIEnv *env, jobject thiz, jstring server_ip,jstring stream_name,jstring filepath
        ,jstring jrangeLine,jstring jauxSDPLine,jint jrtpTimestampFrequency,jobject callback) {
    //1. 传递给子线程的对象 JavaListener ，**这里需要 instance 转化为全局的实例**
//    JavaListener *javaListener = new JavaListener(jvm, env, env->NewGlobalRef(callback));
    javaListener = new JavaListener(jvm, env, env->NewGlobalRef(callback));

    dar_remoteStreamName = env->GetStringUTFChars(stream_name, 0);

    dssNameOrAddress = env->GetStringUTFChars(server_ip, 0);

    dar_inputFileName = env->GetStringUTFChars(filepath, 0);

    rangeLine = env->GetStringUTFChars(jrangeLine, 0);

    auxSDPLine = env->GetStringUTFChars(jauxSDPLine, 0);

    rtpTimestampFrequency = jrtpTimestampFrequency;

    msg = "1";
    //2. 创建子线程，将 javaListener 传递给线程执行体 childCallback
    pthread_create(&childThread, NULL, childCallback, javaListener);

    //2. 创建子线程，将 javaListener 传递给线程执行体 childCallback
//    pthread_create(&childThread, NULL, conn_childCallback, javaListener);

    std::string hello = "ok";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_start(JNIEnv *env, jobject thiz, jstring filepath) {
    dar_inputFileName = env->GetStringUTFChars(filepath, 0);
    //2. 创建子线程，将 javaListener 传递给线程执行体 childCallback
    pthread_create(&childThread, NULL, play_childCallback, javaListener);
    seekPosByte = 0;
    std::string hello = "ok";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_stop(JNIEnv *env, jobject thiz) {
    pthread_create(&childThread, NULL, stop_childCallback, javaListener);
    std::string hello = "ok";
    seekPosByte = 0;
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_pause(JNIEnv *env, jobject thiz) {
    pthread_create(&childThread, NULL, pause_childCallback, javaListener);
    std::string hello = "ok";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_play(JNIEnv *env, jobject thiz) {
    if(fd != 0){//先关闭
        close(fd);
        fd = 0;
    }
    pthread_create(&childThread, NULL, play_childCallback, javaListener);
    std::string hello = "ok";
    seekPosByte = 0;
    return env->NewStringUTF(hello.c_str());
}


extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_push(JNIEnv *env, jobject thiz, jbyteArray byteArray,jobject callback) {
    call_javaListener = new JavaListener(jvm, env, env->NewGlobalRef(callback));
    jbyte* receivedbyte = env->GetByteArrayElements(byteArray, 0);
    jsize bsize = env->GetArrayLength(byteArray);
    LOGI("live555 push MP3Recorder buf %d   ",bsize);
    std::string hello = "ok";
    return env->NewStringUTF(hello.c_str());
}


extern "C" JNIEXPORT jstring JNICALL Java_com_live555_push_LivePush_pushList(JNIEnv *env, jobject thiz, jobject byteList,jobject callback) {
    call_javaListener = new JavaListener(jvm, env, env->NewGlobalRef(callback));
    std::string hello = "ok";
    return env->NewStringUTF(hello.c_str());
}
