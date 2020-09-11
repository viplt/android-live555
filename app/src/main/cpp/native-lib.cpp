#include <jni.h>
#include <string>

#include "liveMedia/include/liveMedia.hh"
#include "groupsock/include/GroupsockHelper.hh"
#include "BasicUsageEnvironment/include/BasicUsageEnvironment.hh"
#include "liveMedia/include/DarwinInjector.hh"


#include <android/log.h>

#define LOG_TAG "rtsp_server"
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


/************Darwin****************/

UsageEnvironment* dar_env;
char const* dar_inputFileName = "/storage/emulated/0/I still believe(cover).mp3";
char const* dar_remoteStreamName = "test.sdp"; // the stream name, as served by the DSS
MPEG4VideoStreamFramer* dar_videoSource;
RTPSink* dar_videoSink;
RTPSink* dar_audioSink;

char const* dar_programName;

void dar_usage() {
    *dar_env << "usage: " << dar_programName
         << " <Darwin Streaming Server name or IP address>\n";
    exit(1);
}

Boolean dar_awaitConfigInfo(RTPSink* sink); // forward
void dar_play(); // forward

extern "C" JNIEXPORT jstring JNICALL Java_cn_xag_androidforlive555_MainActivity_DarwinStart(JNIEnv *jenv, jobject instance, jstring fileName_) {
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    dar_env = BasicUsageEnvironment::createNew(*scheduler);
//    dar_inputFileName = dar_env->GetStringUTFChars(fileName_, 0);
    // Parse command-line arguments:
    dar_programName = "aaa";
    //if (argc != 2) dar_usage();
    char const* dssNameOrAddress = "192.168.3.8";
    // Create a 'Darwin injector' object:
    DarwinInjector* injector = DarwinInjector::createNew(*dar_env, dar_programName);
    // Create 'groupsocks' for RTP and RTCP.
    // (Note: Because we will actually be streaming through a remote Darwin server,
    // via TCP, we just use dummy destination addresses, port numbers, and TTLs here.)
    struct in_addr dummyDestAddress;
    dummyDestAddress.s_addr = 0;
//    dummyDestAddress.s_addr = our_inet_addr(dssNameOrAddress);

    Groupsock rtpGroupsockVideo(*dar_env, dummyDestAddress, 0, 0);
    Groupsock rtcpGroupsockVideo(*dar_env, dummyDestAddress, 0, 0);

    // Create a 'H264 Video RTP' sink from the RTP 'groupsock':
    dar_videoSink = H264VideoRTPSink::createNew(*dar_env, &rtpGroupsockVideo, 96);

    // HACK, specifically for MPEG-4 video:
    // Before we can use this RTP sink, we need its MPEG-4 'config' information (for
    // use in the SDP description).  Unfortunately, this config information depends
    // on the the properties of the MPEG-4 input data.  Therefore, we need to start
    // 'playing' this RTP sink from the input source now, and wait until we get
    // the needed config information, before continuing:
    // that we need:
    *dar_env << "Beginning streaming...\n";
    dar_play();

    if (!dar_awaitConfigInfo(dar_videoSink)) {
        *dar_env << "Failed to get H264 'config' information from input file: "
             << dar_env->getResultMsg() << "\n";
        exit(1);
    }

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidthVideo = 500; // in kbps; for RTCP b/w share
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen+1];
    gethostname((char*)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case
    RTCPInstance* videoRTCP =
            RTCPInstance::createNew(*dar_env, &rtcpGroupsockVideo,
                                    estimatedSessionBandwidthVideo, CNAME,
                                    dar_videoSink, NULL /* we're a server */);
    // Note: This starts RTCP running automatically

    // Add these to our 'Darwin injector':
    injector->addStream(dar_videoSink, videoRTCP);

    // Next, specify the destination Darwin Streaming Server:
    if (!injector->setDestination(dssNameOrAddress, dar_remoteStreamName,
                                  dar_programName, "LIVE555 Streaming Media")) {
        *dar_env << "injector->setDestination() failed: "
                 << dar_env->getResultMsg() << "\n";
        exit(1);
    }

    *dar_env << "Play this stream (from the Darwin Streaming Server) using the URL:\n"
             << "\trtsp://" << dssNameOrAddress << "/" << dar_remoteStreamName << "\n";

    LOGE("URL : %s:554/%s", dssNameOrAddress,dar_remoteStreamName);
    dar_env->taskScheduler().doEventLoop(); // does not return


    std::string hello = "ok";
    return jenv->NewStringUTF(hello.c_str());
}


void dar_afterPlaying(void* clientData) {
    *dar_env << "...done reading from file\n";

    Medium::close(dar_videoSource);
    // Note: This also closes the input file that this source read from.

    // Start playing once again:
    dar_play();
}

void dar_play() {
    // Open the input file as a 'byte-stream file source':
    ByteStreamFileSource* fileSource
            = ByteStreamFileSource::createNew(*dar_env, dar_inputFileName);
    if (fileSource == NULL) {
        *dar_env << "Unable to open file \"" << dar_inputFileName
             << "\" as a byte-stream file source\n";
        exit(1);
    }

    FramedSource* source = fileSource;

    // Create a framer for the Video Elementary Stream:
//    FramedSource* videoSource = H264VideoStreamFramer::createNew(*dar_env, source);
//    FramedSource* wav_mp3_source = MP3ADURTPSource::createNew(*dar_env, source);
//    FramedSource* wav_mp3_source = AC3AudioStreamFramer::createNew(*dar_env, source);
    FramedSource* wav_mp3_source = MPEG1or2AudioStreamFramer::createNew(*dar_env, source);

    // Finally, start playing:
    *dar_env << "Beginning to read from file...\n";
//    dar_videoSink->startPlaying(*videoSource, dar_afterPlaying, dar_videoSink);
    dar_audioSink->startPlaying(*wav_mp3_source, dar_afterPlaying, dar_audioSink);
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

extern "C" JNIEXPORT jstring JNICALL Java_cn_xag_androidforlive555_MainActivity_startWAVOrMp3(JNIEnv *jenv, jobject instance, jstring fileName_) {
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    dar_env = BasicUsageEnvironment::createNew(*scheduler);
//    dar_inputFileName = dar_env->GetStringUTFChars(fileName_, 0);
    // Parse command-line arguments:
    dar_programName = "wavormp3";
    //if (argc != 2) dar_usage();
    char const* dssNameOrAddress = "192.168.3.8";
    // Create a 'Darwin injector' object:
    DarwinInjector* injector = DarwinInjector::createNew(*dar_env, dar_programName);
    // Create 'groupsocks' for RTP and RTCP.
    // (Note: Because we will actually be streaming through a remote Darwin server,
    // via TCP, we just use dummy destination addresses, port numbers, and TTLs here.)
    struct in_addr dummyDestAddress;
    dummyDestAddress.s_addr = 0;
//    dummyDestAddress.s_addr = our_inet_addr(dssNameOrAddress);

    Groupsock rtpGroupsock(*dar_env, dummyDestAddress, 0, 0);
    Groupsock rtcpGroupsock(*dar_env, dummyDestAddress, 0, 0);

    // Create a 'WAV Video RTP' sink from the RTP 'groupsock':
//    dar_videoSink = H264VideoRTPSink::createNew(*dar_env, &rtpGroupsock, 96);
//    dar_audioSink = MP3ADURTPSink::createNew(*dar_env, &rtpGroupsock);
//    dar_audioSink = AC3AudioRTPSink::createNew(*dar_env, &rtpGroupsock,96,0);
    dar_audioSink = MPEG1or2AudioRTPSink::createNew(*dar_env, &rtpGroupsock);
//    dar_audioSink = AC3AudioStreamFramer::createNew(*dar_env, &rtpGroupsock);

    // HACK, specifically for MPEG-4 video:
    // Before we can use this RTP sink, we need its MPEG-4 'config' information (for
    // use in the SDP description).  Unfortunately, this config information depends
    // on the the properties of the MPEG-4 input data.  Therefore, we need to start
    // 'playing' this RTP sink from the input source now, and wait until we get
    // the needed config information, before continuing:
    // that we need:
    *dar_env << "Beginning streaming...\n";
    dar_play();

//    if (!dar_awaitConfigInfo(dar_audioSink)) {
//        *dar_env << "Failed to get wavormp3 'config' information from input file: "
//                 << dar_env->getResultMsg() << "\n";
//        exit(1);
//    }

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
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
                                  dar_programName, "LIVE555 Streaming Media")) {
        *dar_env << "injector->setDestination() failed: "
                 << dar_env->getResultMsg() << "\n";
        exit(1);
    }

    *dar_env << "Play this stream (from the Darwin Streaming Server) using the URL:\n"
             << "\trtsp://" << dssNameOrAddress << "/" << dar_remoteStreamName << "\n";

    LOGE("URL : %s:554/%s", dssNameOrAddress,dar_remoteStreamName);
    dar_env->taskScheduler().doEventLoop(); // does not return


    std::string hello = "ok";
    return jenv->NewStringUTF(hello.c_str());
}








