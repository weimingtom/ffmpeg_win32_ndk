#define DEBUG_BUILD 0
#define DEBUG_BUILD_CB 0

#define USE_EVENT_MULTI_THREAD 0

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/pixfmt.h>
#include <libavutil/log.h>

#include <stdint.h>

#ifdef __MINGW32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void system_timer_delay(uint32_t ms)
{
#ifdef __MINGW32__
	Sleep(ms);
#else
# error "no system_timer_delay implements"
#endif
}

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "SDLMOD_events.h"
#include "SDLMOD_audio.h"

#define SDL_AUDIO_BUFFER_SIZE 1024 
#define MAX_AUDIOQ_SIZE (1 * 1024 * 1024)

#define FF_QUIT_EVENT (SDLMOD_USEREVENT + 2)

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} PacketQueue;

typedef struct VideoState {
    char            filename[1024];
    AVFormatContext *ic;
    int             videoStream, audioStream;
    AVStream        *audio_st;
    AVFrame         *audio_frame;
    PacketQueue     audioq;
    unsigned int    audio_buf_size;
    unsigned int    audio_buf_index;
    AVPacket        audio_pkt;
    uint8_t         *audio_pkt_data;
    int             audio_pkt_size;
    uint8_t         *audio_buf;
    uint8_t         *audio_buf1;
    DECLARE_ALIGNED(16,uint8_t,audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    enum AVSampleFormat  audio_src_fmt;
    enum AVSampleFormat  audio_tgt_fmt;
    int             audio_src_channels;
    int             audio_tgt_channels;
    int64_t         audio_src_channel_layout;
    int64_t         audio_tgt_channel_layout;
    int             audio_src_freq;
    int             audio_tgt_freq;
    struct SwrContext *swr_ctx;
	pthread_t*       parse_tid;
    int             quit;
} VideoState;

VideoState *global_video_state;

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mutex, NULL);
    q->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->cond, NULL);
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;
#if DEBUG_BUILD
	printf(">>>>>packet_queue_put (size %d)\n", q->size);
#endif
    pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
    if (!pkt1) {
        return -1;
    }
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    pthread_mutex_lock(q->mutex);
    if (!q->last_pkt) {
        q->first_pkt = pkt1;
    } else {
        q->last_pkt->next = pkt1;
    }

    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
	pthread_cond_signal(q->cond);
    pthread_mutex_unlock(q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

#if DEBUG_BUILD
	printf("<<<<<packet_queue_get start\n");
#endif

	pthread_mutex_lock(q->mutex);
    for(;;) {
        if(global_video_state->quit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt) {
                q->last_pkt = NULL;
            }
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;

            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
#if DEBUG_BUILD
	printf("<<<<<packet_queue_get SDL_CondWait begin\n");
#endif
        {
			struct timeval now;
			struct timespec timeout;
			int retcode;
			while (1)
			{
				gettimeofday(&now, NULL);
				timeout.tv_sec = now.tv_sec + 0;
				timeout.tv_nsec = now.tv_usec * 100;
				int ret;
				ret = pthread_cond_timedwait(q->cond, q->mutex, &timeout);
				if (ret == 0)
				{
					break;
				}
				else if (global_video_state->quit)
				{
					break;
				}
				else if (ret == ETIMEDOUT)
				{
					continue;
				}
				else
				{
					break;
				}
			}
        }
#if DEBUG_BUILD
	printf("<<<<<packet_queue_get SDL_CondWait end\n");
#endif
        }
    }

	pthread_mutex_unlock(q->mutex);

#if DEBUG_BUILD
	printf("<<<<<packet_queue_get end\n");
#endif
    return ret;
}

static void packet_queue_flush(PacketQueue *q) {
    AVPacketList *pkt, *pkt1;

	pthread_mutex_lock(q->mutex);
    for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
	pthread_mutex_unlock(q->mutex);
}

int audio_decode_frame(VideoState *is) {
    int len1, len2, decoded_data_size;
    AVPacket *pkt = &is->audio_pkt;
    int got_frame = 0;
    int64_t dec_channel_layout;
    int wanted_nb_samples, resampled_data_size;

    for (;;) {
		if (is->quit) return -1;
        while (is->audio_pkt_size > 0) {
			if (is->quit) return -1;
            if (!is->audio_frame) {
                if (!(is->audio_frame = avcodec_alloc_frame())) {
                    return AVERROR(ENOMEM);
                }
            } else 
                avcodec_get_frame_defaults(is->audio_frame);

            len1 = avcodec_decode_audio4(is->audio_st->codec, is->audio_frame, &got_frame,  pkt);
            if (len1 < 0) {
                // error, skip the frame
                is->audio_pkt_size = 0;
                break;
            }

            is->audio_pkt_data += len1;
            is->audio_pkt_size -= len1;

            if (!got_frame) 
                continue;

            decoded_data_size = av_samples_get_buffer_size(NULL,
                                is->audio_frame->channels,
                                is->audio_frame->nb_samples,
                                is->audio_frame->format, 1);

            dec_channel_layout = (is->audio_frame->channel_layout && is->audio_frame->channels
                                  == av_get_channel_layout_nb_channels(is->audio_frame->channel_layout))
                                 ? is->audio_frame->channel_layout
                                 : av_get_default_channel_layout(is->audio_frame->channels);

            wanted_nb_samples =  is->audio_frame->nb_samples;

            //fprintf(stderr, "wanted_nb_samples = %d\n", wanted_nb_samples);

            if (is->audio_frame->format != is->audio_src_fmt ||
                dec_channel_layout != is->audio_src_channel_layout ||
                is->audio_frame->sample_rate != is->audio_src_freq ||
                (wanted_nb_samples != is->audio_frame->nb_samples && !is->swr_ctx)) {
                if (is->swr_ctx) swr_free(&is->swr_ctx);
                is->swr_ctx = swr_alloc_set_opts(NULL,
                                                 is->audio_tgt_channel_layout,
                                                 is->audio_tgt_fmt,
                                                 is->audio_tgt_freq,
                                                 dec_channel_layout,
                                                 is->audio_frame->format,
                                                 is->audio_frame->sample_rate,
                                                 0, NULL);
                if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
                    fprintf(stderr, "swr_init() failed\n");
                    break;
                }
                is->audio_src_channel_layout = dec_channel_layout;
                is->audio_src_channels = is->audio_st->codec->channels;
                is->audio_src_freq = is->audio_st->codec->sample_rate;
                is->audio_src_fmt = is->audio_st->codec->sample_fmt;
            }
            if (is->swr_ctx) {
               // const uint8_t *in[] = { is->audio_frame->data[0] };
                const uint8_t **in = (const uint8_t **)is->audio_frame->extended_data; 
                uint8_t *out[] = { is->audio_buf2 };
				if (wanted_nb_samples != is->audio_frame->nb_samples) {
					 if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - is->audio_frame->nb_samples)
												 * is->audio_tgt_freq / is->audio_frame->sample_rate,
												 wanted_nb_samples * is->audio_tgt_freq / is->audio_frame->sample_rate) < 0) {
						 fprintf(stderr, "swr_set_compensation() failed\n");
						 break;
					 }
				 }

                len2 = swr_convert(is->swr_ctx, out,
                                   sizeof(is->audio_buf2)
                                   / is->audio_tgt_channels
                                   / av_get_bytes_per_sample(is->audio_tgt_fmt),
                                   in, is->audio_frame->nb_samples);
                if (len2 < 0) {
                    fprintf(stderr, "swr_convert() failed\n");
                    break;
                }
                if (len2 == sizeof(is->audio_buf2) / is->audio_tgt_channels / av_get_bytes_per_sample(is->audio_tgt_fmt)) {
                    fprintf(stderr, "warning: audio buffer is probably too small\n");
                    swr_init(is->swr_ctx);
                }
                is->audio_buf = is->audio_buf2;
                resampled_data_size = len2 * is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt);
            } else {
				resampled_data_size = decoded_data_size;
                is->audio_buf = is->audio_frame->data[0];
            }
            // We have data, return it and come back for more later
            return resampled_data_size;
        }

        if (pkt->data) av_free_packet(pkt);
		memset(pkt, 0, sizeof(*pkt));
        if (is->quit) return -1;
        if (packet_queue_get(&is->audioq, pkt, 1) < 0) return -1;
        is->audio_pkt_data = pkt->data;
        is->audio_pkt_size = pkt->size;
    }
}

void audio_callback(void *userdata, uint8_t *stream, int len) {
    VideoState *is = (VideoState *)userdata;
    int len1, audio_data_size;
	
#if DEBUG_BUILD_CB
	printf("<<<<<audio_callback %d (audioq size %d)\n", len, is->audioq.size);
#endif
    while (len > 0) {
		if (is->quit) break;
        if (is->audio_buf_index >= is->audio_buf_size) {
#if DEBUG_BUILD_CB
	printf("<<<<<audio_callback pre audio_decode_frame\n");
#endif
            audio_data_size = audio_decode_frame(is);
#if DEBUG_BUILD_CB
	printf("<<<<<audio_callback post audio_decode_frame\n");
#endif

            if(audio_data_size < 0) {
                /* silence */
                is->audio_buf_size = 1024;
                memset(is->audio_buf, 0, is->audio_buf_size);
            } else {
                is->audio_buf_size = audio_data_size;
            }
            is->audio_buf_index = 0;
        }

        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }

        memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
#if DEBUG_BUILD_CB
    printf("<<<<<audio_callback %d (audioq size %d) end\n", len, is->audioq.size);
#endif
}

int stream_component_open(VideoState *is, int stream_index) {
    AVFormatContext *ic = is->ic;
    AVCodecContext *codecCtx;
    AVCodec *codec;
    SDLMOD_AudioSpec wanted_spec, spec;
    int64_t wanted_channel_layout = 0;
    int wanted_nb_channels;
	const int next_nb_channels[] = {0, 0, 1 ,6, 2, 6, 4, 6};

    if (stream_index < 0 || stream_index >= ic->nb_streams) {
        return -1;
    }
	
    codecCtx = ic->streams[stream_index]->codec;
	wanted_nb_channels = codecCtx->channels;
	if(!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
		wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
	}
	
	wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
	wanted_spec.freq = codecCtx->sample_rate;
	if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
		fprintf(stderr, "Invalid sample rate or channel count!\n");
		return -1;
	}
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.silence = 0;
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = is;
	
	while(SDLMOD_OpenAudio(&wanted_spec, &spec) < 0) {
		fprintf(stderr, "SDL_OpenAudio (%d channels)\n", wanted_spec.channels);
		wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
		if(!wanted_spec.channels) {
			fprintf(stderr, "No more channel combinations to tyu, audio open failed\n");
			return -1;
		}
		wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
	}

	if (spec.format != AUDIO_S16SYS) {
		fprintf(stderr, "SDL advised audio format %d is not supported!\n", spec.format);
		return -1;
	}
	if (spec.channels != wanted_spec.channels) {
		wanted_channel_layout = av_get_default_channel_layout(spec.channels);
		if (!wanted_channel_layout) {
			fprintf(stderr, "SDL advised channel count %d is not supported!\n", spec.channels);
			return -1;
		}
	}

	fprintf(stderr, "%d: wanted_spec.format = %d\n", __LINE__, wanted_spec.format);
	fprintf(stderr, "%d: wanted_spec.samples = %d\n", __LINE__, wanted_spec.samples);
	fprintf(stderr, "%d: wanted_spec.channels = %d\n", __LINE__, wanted_spec.channels);
	fprintf(stderr, "%d: wanted_spec.freq = %d\n", __LINE__, wanted_spec.freq);

	fprintf(stderr, "%d: spec.format = %d\n", __LINE__, spec.format);
	fprintf(stderr, "%d: spec.samples = %d\n", __LINE__, spec.samples);
	fprintf(stderr, "%d: spec.channels = %d\n", __LINE__, spec.channels);
	fprintf(stderr, "%d: spec.freq = %d\n", __LINE__, spec.freq);

	is->audio_src_fmt = is->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
	is->audio_src_freq = is->audio_tgt_freq = spec.freq;
	is->audio_src_channel_layout = is->audio_tgt_channel_layout = wanted_channel_layout;
	is->audio_src_channels = is->audio_tgt_channels = spec.channels;
    
    codec = avcodec_find_decoder(codecCtx->codec_id);
    if (!codec || (avcodec_open2(codecCtx, codec, NULL) < 0)) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
	ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch(codecCtx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audioStream = stream_index;
        is->audio_st = ic->streams[stream_index];
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;
        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        packet_queue_init(&is->audioq);
        SDLMOD_PauseAudio(0);
        break;
        
    default:
        break;
    }
}
/*
static void stream_component_close(VideoState *is, int stream_index) {
	AVFormatContext *oc = is->;
	AVCodecContext *avctx;

	if(stream_index < 0 || stream_index >= ic->nb_streams)	return;
	avctx = ic->streams[stream_index]->codec;

}
*/
static int decode_thread(void *arg) {
    VideoState *is = (VideoState *)arg;
    AVFormatContext *ic = NULL;
    AVPacket pkt1, *packet = &pkt1;
    int ret, i, audio_index = -1;

    is->audioStream=-1;
    global_video_state = is;
    if (avformat_open_input(&ic, is->filename, NULL, NULL) != 0) {
		fprintf(stderr, "%s: avformat_open_input() error\n", is->filename);
        //return -1;
        goto fail;
    }
    is->ic = ic;
    if (avformat_find_stream_info(ic, NULL) < 0) {
        fprintf(stderr, "%s: avformat_find_stream_info() error\n", is->filename);
        //return -1;
        goto fail;
    }
    av_dump_format(ic, 0, is->filename, 0);
    for (i=0; i<ic->nb_streams; i++) {
        if (ic->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO && audio_index < 0) {
            audio_index=i;
            break;
        }
    }
    if (audio_index >= 0) {
        stream_component_open(is, audio_index);
    }
    if (is->audioStream < 0) {
        fprintf(stderr, "%s: could not open codecs\n", is->filename);
        goto fail;
    }
    // main decode loop
    for(;;) {
        if(is->quit) break;
        if (is->audioq.size > MAX_AUDIOQ_SIZE) {
            system_timer_delay(10);
            continue;
        }
        ret = av_read_frame(is->ic, packet);
        if (ret < 0) {
            if(ret == AVERROR_EOF || url_feof(is->ic->pb)) {
                break;
            }
            if(is->ic->pb && is->ic->pb->error) {
                break;
            }
            continue;
        }

        if (packet->stream_index == is->audioStream) {
            packet_queue_put(&is->audioq, packet);
        } else {
            av_free_packet(packet);
        }
    }

    while (!is->quit && is->audioq.size > 0) {
        system_timer_delay(100);
    }

fail: {
        SDLMOD_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDLMOD_PushEvent(&event);
    }

    return 0;
}

void *decodeThread(void *data)
{
	decode_thread(data);
	return NULL;
}

#ifdef main
#undef main
#endif

int main(int argc, char *argv[]) {
    SDLMOD_Event       event;
    VideoState      *is;
#ifdef __MINGW32__
	ptw32_processInitialize();
	//ptw32_processTerminate();
#endif
    is = (VideoState *)av_mallocz(sizeof(VideoState));

    if (argc < 2) {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }

    av_register_all();

	if (USE_EVENT_MULTI_THREAD)
	{
		if ( SDLMOD_StartEventLoop(SDLMOD_INIT_EVENTTHREAD) < 0 ) {
			fprintf(stderr, "Could not start SDLMOD event loop (multi thread)\n");
			exit(1);
		}
	}
	else
	{
		if ( SDLMOD_StartEventLoop(0) < 0 ) {
			fprintf(stderr, "Could not start SDLMOD event loop (main thread)\n");
			exit(1);
		}
	}
	
    if ( SDLMOD_AudioInit(NULL) < 0 ) {
        fprintf(stderr, "Could not initialize SDLMOD\n");
        exit(1);
    }
    
    av_strlcpy(is->filename, argv[1], sizeof(is->filename));
	{
		pthread_t tid;
		int err;
		err = pthread_create(&tid, NULL, decodeThread, is);
		if (err!=0)
		{
			printf("can't create thread: %s\n", strerror(err));
			exit(0);
		}
		else
		{
			is->parse_tid = &tid;
		}
	}
    if (!is->parse_tid) {
        av_free(is);
        return -1;
    }
	
    for(;;) {
        SDLMOD_WaitEvent(&event);
        switch(event.type) {
        case FF_QUIT_EVENT:
#if DEBUG_BUILD
			printf("got event FF_QUIT_EVENT\n");
#endif
            is->quit = 1;
            
            SDLMOD_StopEventLoop();

#if DEBUG_BUILD
            printf("after SDLMOD_StopEventLoop\n");
#endif
            SDLMOD_AudioQuit();      
            
#if DEBUG_BUILD
            printf("prepare exit\n");
#endif

			pthread_cond_destroy(is->audioq.cond);
            free(is->audioq.cond);
            is->audioq.cond = NULL;
            pthread_mutex_destroy(is->audioq.mutex);
            free(is->audioq.mutex);
            is->audioq.mutex = NULL;
#ifdef __MINGW32__
			//ptw32_processInitialize();
			ptw32_processTerminate();
#endif
            exit(0);
            break;
            
        default:
            break;
        }
    }
#if DEBUG_BUILD
            printf("end of program\n");
#endif
    return 0;
}
