/*
 * Copyright (c) 2012 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat demuxing API use example.
 *
 * Show how to use the libavformat and libavcodec API to demux and
 * decode audio and video data.
 * @example doc/examples/demuxing.c
 */
#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define  LOG_TAG    "VideoDemo"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static uint8_t **audio_dst_data = NULL;
static int       audio_dst_linesize;
static int audio_dst_bufsize;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
static int audio_frame_count = 0;

/*
AVFormatContext *pFormatCtx;
AVCodecContext *pCodecCtx, *pCodecCtxAudio;
AVFrame *pFrame;
int videoStream, audioStream;

//FIXME:
int16_t* samples = NULL; 
int sample_size;
*/

static AVFrame *pFrameRGB;
static uint8_t *buffer;

static int stop;

static ANativeWindow *native_window; //the android native window where video will be rendered
static ANativeWindow_Buffer wbuffer;

static JNIEnv * Android_env = NULL;
static jobject Android_obj = NULL;
static jmethodID Android_methodid_log = NULL;
static jmethodID Android_methodid_write = NULL;
static void Android_callback_log(char* str)
{
    jclass stringClass;
    jmethodID cid;
    jcharArray elemArr;
    jstring result;
    jchar *chars = NULL;
    jint len;
    int i;
    JNIEnv * env;
    jobject obj;

    if (Android_env != NULL && Android_obj != NULL)
    {
    	env = Android_env;
    	obj = Android_obj;
		len = strlen(str);
		chars = (jchar *) malloc(len * sizeof(jchar));
		for (i = 0; i < len; i++)
		{
			chars[i] = str[i];
		}

		stringClass = (*env)->FindClass(env, "java/lang/String");
		if (stringClass == NULL)
		{
			return ;
		}

		cid = (*env)->GetMethodID(env, stringClass, "<init>", "([C)V");
		if (cid == NULL)
		{
			return ;
		}

		elemArr = (*env)->NewCharArray(env, len);
		if (elemArr == NULL)
		{
			return ;
		}

		(*env)->SetCharArrayRegion(env, elemArr, 0, len, chars);
		result = (*env)->NewObject(env, stringClass, cid, elemArr);
		(*env)->DeleteLocalRef(env, elemArr);

		if (Android_methodid_log != NULL)
		{
			(*env)->CallVoidMethod(env, obj, Android_methodid_log, result);
		}
		(*env)->DeleteLocalRef(env, result);
		(*env)->DeleteLocalRef(env, stringClass);

		if (chars != NULL)
		{
			free(chars);
			chars = NULL;
		}
    }
}
static void Android_callback_write(char* str, int len)
{
    //jclass stringClass;
    //jmethodID cid;
    jbyteArray elemArr;
    //jstring result;
    jbyte *chars = NULL;
    //jint len;
    int i;
    JNIEnv * env;
    jobject obj;

    if (Android_env != NULL && Android_obj != NULL)
    {
    	env = Android_env;
    	obj = Android_obj;
		chars = (jbyte *) malloc(len * sizeof(jbyte));
		for (i = 0; i < len; i++)
		{
			chars[i] = str[i];
		}
		elemArr = (*env)->NewByteArray(env, len);
		if (elemArr == NULL)
		{
			return ;
		}

		(*env)->SetByteArrayRegion(env, elemArr, 0, len, chars);
		
		if (Android_methodid_log != NULL)
		{
			(*env)->CallVoidMethod(env, obj, Android_methodid_write, elemArr);
		}
		(*env)->DeleteLocalRef(env, elemArr);
		
		if (chars != NULL)
		{
			free(chars);
			chars = NULL;
		}
    }
}

static void audio_add_1()
{
	struct SwrContext *swr_ctx;
	int ret;

	swr_ctx = swr_alloc_set_opts(NULL, 
		audio_dec_ctx->channel_layout, AV_SAMPLE_FMT_S16, audio_dec_ctx->sample_rate, 
		audio_dec_ctx->channel_layout, audio_dec_ctx->sample_fmt, audio_dec_ctx->sample_rate, 
		0, NULL);
	swr_init(swr_ctx);
	ret = swr_convert(swr_ctx, audio_dst_data/*dst_data*/, frame->nb_samples,
				(const uint8_t **)frame->data/*&audio_dst_data[0]*/, frame->nb_samples);
	swr_free(&swr_ctx);
	if (ret > 0)
	{
		int dst_bufsize = 
			av_samples_get_buffer_size(NULL, av_frame_get_channels(frame),
				frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
		if (0)
		{
			fwrite(audio_dst_data[0], 1, dst_bufsize, audio_dst_file);
		}
		else
		{
			Android_callback_write(audio_dst_data[0], dst_bufsize);
		}
	}
}

static void draw_add_1()
{
	struct SwsContext *img_convert_ctx = NULL;

	int target_width = ANativeWindow_getWidth(native_window);
	int target_height = ANativeWindow_getHeight(native_window);
	LOGI("window size: %d, %d", target_width, target_height);
	LOGI("media size: %d, %d", video_dec_ctx->width, video_dec_ctx->height);
	LOGI("frame size: %d, %d", frame->width, frame->height);
	if (video_dec_ctx->pix_fmt != AV_PIX_FMT_NONE)
		LOGI("src pix_fmt: %s", av_get_pix_fmt_name(video_dec_ctx->pix_fmt));
	else
		LOGI("src pix_fmt: none");

	img_convert_ctx = sws_getContext(video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
			video_dec_ctx->width, video_dec_ctx->height, PIX_FMT_RGB565,
			SWS_BICUBIC, NULL, NULL, NULL);
	if(img_convert_ctx == NULL){
		LOGE("Couldn't initialize conversion context");
		return;
	}

	sws_scale(img_convert_ctx,
			(const uint8_t* const *)frame->data, frame->linesize, 0, video_dec_ctx->height,
			pFrameRGB->data, pFrameRGB->linesize);
	sws_freeContext(img_convert_ctx);
	LOGI("after scale frame size: %d, %d", pFrameRGB->width, pFrameRGB->height);

	LOGI("ready to set buffers");
	ANativeWindow_setBuffersGeometry(native_window, video_dec_ctx->width, video_dec_ctx->height, WINDOW_FORMAT_RGB_565);

	LOGI("ready to lock native window");
	if (ANativeWindow_lock(native_window, &wbuffer, NULL) < 0) {
		LOGE("Unable to lock window buffer");
		return;
	}

	LOGI("mem copy frame");
	LOGI("linesize = %d linesize1 = %d linesize2 = %d linesize3 = %d ph = %d pw = %d",
		frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->linesize[3],
		frame->height, frame->width);
	memcpy(wbuffer.bits, buffer,  frame->width * frame->height * 2);

	LOGI("memcpy done and unlock window");
	ANativeWindow_unlockAndPost(native_window);
}

static void free_add_1()
{
	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);
}

static void init_add_0()
{
	video_stream_idx = -1;
	audio_stream_idx = -1;
	frame = NULL;
	video_frame_count = 0;
	audio_frame_count = 0;
	fmt_ctx = NULL;
	video_dec_ctx = NULL;
	audio_dec_ctx = NULL;
	video_stream = NULL;
	audio_stream = NULL;
}

static void init_add_1()
{
	// Allocate an AVFrame structure
	pFrameRGB = avcodec_alloc_frame();
	
	// Determine required buffer size and allocate buffer
	int numBytes = avpicture_get_size(PIX_FMT_RGB565, video_dec_ctx->width, video_dec_ctx->height);
	buffer = (uint8_t*) av_malloc(numBytes * sizeof(uint8_t));
	LOGI("buffer size: %d", numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture*)pFrameRGB, buffer, PIX_FMT_RGB565, video_dec_ctx->width, video_dec_ctx->height);
}

static int decode_packet(int *got_frame, int cached)
{
    int ret = 0;

    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            LOGE("Error decoding video frame\n");
            return ret;
        }

        if (*got_frame) {
            LOGI("video_frame%s n:%d coded_n:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number,
                   av_ts2timestr(frame->pts, &video_dec_ctx->time_base));
			//Android_callback_log("======================");
            //Android_callback_write("======================", 20);
            if (0)
            {
				/* copy decoded frame to destination buffer:
			          * this is required since rawvideo expects non aligned data */
				av_image_copy(video_dst_data, video_dst_linesize,
							  (const uint8_t **)(frame->data), frame->linesize,
							  video_dec_ctx->pix_fmt, video_dec_ctx->width, video_dec_ctx->height);
				/* write to rawvideo file */
				fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
			}
			else
			{
				draw_add_1();
			}
        }
    } else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            LOGE("Error decoding audio frame");
            return ret;
        }

        if (*got_frame) {
            LOGI("audio_frame%s n:%d nb_samples:%d pts:%s",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
				
			ret = av_samples_alloc(audio_dst_data, &audio_dst_linesize, av_frame_get_channels(frame),
								   frame->nb_samples, frame->format, 1);
			if (ret < 0) {
				LOGE("Could not allocate audio buffer");
				return AVERROR(ENOMEM);
			}
				
			/* TODO: extend return code of the av_samples_* functions so that this call is not needed */
			audio_dst_bufsize =
				av_samples_get_buffer_size(NULL, av_frame_get_channels(frame),
					frame->nb_samples, frame->format, 1);
				
			if (0)
			{				
				/* copy audio data to destination buffer:
				* this is required since rawaudio expects non aligned data */
				av_samples_copy(audio_dst_data, frame->data, 0, 0,
                            frame->nb_samples, av_frame_get_channels(frame), frame->format);
				/* write to rawaudio file */
				fwrite(audio_dst_data[0], 1, audio_dst_bufsize, audio_dst_file);
            }
            else
            {
				audio_add_1();
            }
            av_freep(&audio_dst_data[0]);
        }
    }

    return ret;
}

static int open_codec_context(int *stream_idx,
                              AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        LOGE("Could not find %s stream in input file '%s'",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        *stream_idx = ret;
        st = fmt_ctx->streams[*stream_idx];

        /* find decoder for the stream */
        dec_ctx = st->codec;
        dec = avcodec_find_decoder(dec_ctx->codec_id);
        if (!dec) {
            LOGE("Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }

        if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
            LOGE("Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
    }

    return 0;
}

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    LOGE("sample format %s is not supported as output format",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

void main()
{
    int ret = 0, got_frame;
    
    init_add_0();
    src_filename = "file:/sdcard/inlove_mpeg.mpg";
    video_dst_filename = "/sdcard/video_dst_filename";
    audio_dst_filename = "/sdcard/audio_dst_filename";
    
    /* register all formats and codecs */
    av_register_all();

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        LOGE("Could not open source file %s", src_filename);
        return;
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Could not find stream information");
        return;
    }

    if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dec_ctx = video_stream->codec;
		
		if (0)
		{
			video_dst_file = fopen(video_dst_filename, "wb");
			if (!video_dst_file) {
				LOGE("Could not open destination file %s", video_dst_filename);
				ret = 1;
				goto end;
			}
		}

        /* allocate image where the decoded image will be put */
        ret = av_image_alloc(video_dst_data, video_dst_linesize,
                             video_dec_ctx->width, video_dec_ctx->height,
                             video_dec_ctx->pix_fmt, 1);
        if (ret < 0) {
            LOGE("Could not allocate raw video buffer");
            goto end;
        }
        video_dst_bufsize = ret;
    }

    if (open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        int nb_planes;

        audio_stream = fmt_ctx->streams[audio_stream_idx];
        audio_dec_ctx = audio_stream->codec;
        
        if (0)
        {
			audio_dst_file = fopen(audio_dst_filename, "wb");
			if (!audio_dst_file) {
				LOGE("Could not open destination file %s", video_dst_filename);
				ret = 1;
				goto end;
			}
		}

        nb_planes = av_sample_fmt_is_planar(audio_dec_ctx->sample_fmt) ?
            audio_dec_ctx->channels : 1;
        audio_dst_data = av_mallocz(sizeof(uint8_t *) * nb_planes);
        if (!audio_dst_data) {
            LOGE("Could not allocate audio data buffers");
            ret = AVERROR(ENOMEM);
            goto end;
        }
    }
    
    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    if (!audio_stream && !video_stream) {
        LOGE("Could not find audio or video stream in the input, aborting");
        ret = 1;
        goto end;
    }
    
    frame = avcodec_alloc_frame();
    if (!frame) {
        LOGE("Could not allocate frame");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    init_add_1();

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    if (video_stream)
        LOGI("Demuxing video from file '%s' into '%s'", src_filename, video_dst_filename);
    if (audio_stream)
        LOGI("Demuxing audio from file '%s' into '%s'", src_filename, audio_dst_filename);

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0 && !stop) {
        decode_packet(&got_frame, 0);
        av_free_packet(&pkt);
    }
    
    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;
    do {
        decode_packet(&got_frame, 1);
    } while (got_frame && !stop);

    printf("Demuxing succeeded.\n");
    
    if (video_stream) {
        LOGI("Play the output video file with the command:");
        LOGI("ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s",
               av_get_pix_fmt_name(video_dec_ctx->pix_fmt), video_dec_ctx->width, video_dec_ctx->height,
               video_dst_filename);
    }

    if (audio_stream) {
        const char *fmt;

        if ((ret = get_format_from_sample_fmt(&fmt, audio_dec_ctx->sample_fmt)) < 0)
            goto end;
        LOGI("Play the output audio file with the command:");
        LOGI("ffplay -f %s -ac %d -ar %d %s\n",
               fmt, audio_dec_ctx->channels, audio_dec_ctx->sample_rate,
               audio_dst_filename);
    }

end:
    if (video_dec_ctx)
        avcodec_close(video_dec_ctx);
    if (audio_dec_ctx)
        avcodec_close(audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (0)
    {
		if (video_dst_file)
			fclose(video_dst_file);
		if (audio_dst_file)
			fclose(audio_dst_file);
	}
    av_free(frame);
    av_free(video_dst_data[0]);
    av_free(audio_dst_data);
	free_add_1();
    return;
}

JNIEXPORT void JNICALL Java_com_weikun_videodemo_MainActivity_nativeVideoPlay(JNIEnv * env, jobject obj)
{
	pthread_t decodeThread;
	jclass cls;
	
	LOGI("create thread to play video");
	
	//cd bin/classes
	//javap -s -private com.weikun.videodemo.MainActivity
	cls = (*env)->GetObjectClass(env, obj);
	Android_methodid_log = (*env)->GetMethodID(env, cls, "log", "(Ljava/lang/String;)V");
	Android_methodid_write = (*env)->GetMethodID(env, cls, "write", "([B)V");
	Android_env = env;
	Android_obj = obj;
	
	stop = 0;
	if (0)
	{
		pthread_create(&decodeThread, NULL, (void*)main, NULL);
	}
	else
	{
		main();
	}
}

JNIEXPORT void JNICALL Java_com_weikun_videodemo_MainActivity_nativeVideoStop(JNIEnv * env, jobject obj)
{
	stop = 1;
}

JNIEXPORT int JNICALL Java_com_weikun_videodemo_MainActivity_nativeInit(JNIEnv * env, jobject obj) 
{
	return 0;
}

JNIEXPORT void JNICALL Java_com_weikun_videodemo_MainActivity_nativeSurfaceInit(JNIEnv *env, jobject obj, jobject surface) 
{
	ANativeWindow *new_native_window = ANativeWindow_fromSurface(env, surface);
	LOGI("Received surface %p (native window %p)", surface, new_native_window);
	if (native_window) {
		ANativeWindow_release(native_window);
		if (native_window == new_native_window) {
			LOGI("New native window is the same as the previous one %p", native_window);
			return;
		} else {
			LOGI("Released previous native window %p", native_window);
		}
	}
	native_window = new_native_window;
}

JNIEXPORT void JNICALL Java_com_weikun_videodemo_MainActivity_nativeSurfaceFinalize(JNIEnv *env, jobject obj) 
{
	LOGI("Releasing Native Window %p", native_window);
	ANativeWindow_release(native_window);
	native_window = NULL;
}
