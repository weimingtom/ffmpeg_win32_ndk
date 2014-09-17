#define DEBUG_SDLMOD_TIMER 0
#define USE_GL 1
#define USE_SDLMOD_TIMER 1
#define USE_EVENT_MULTI_THREAD 0
#define USE_EVENT_LOOP_MT 1
#define USE_SWS_YUV_CONVERT 1

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/pixfmt.h>
#include <libavutil/log.h>

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>



#include "SDLMOD_events.h"
#include "SDLMOD_video.h"
#include "TextureLoader.h"




#if USE_GL

#include <gl/gl.h>
#include <gl/glu.h>	
#include <gl/glut.h>

#define FILENAME "image1.bmp"
#define TITLE "OpenGL & GLUT sample program"

static unsigned int g_TextureArray[1];
static int g_ID = 0;
static unsigned int g_TextureWidth = 100;
static unsigned int g_TextureHeight = 100;

static int win;
static int menyid;
static int animeringsmeny;
static int springmeny;
static int val = 0;

unsigned char * texture_data = NULL;

int textureContainsData = 0;

#define Rmask 0x000000FF
#define Gmask 0x0000FF00
#define Bmask 0x00FF0000
#define Amask 0xFF000000

#endif









#define  MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define VIDEO_PICTURE_QUEUE_SIZE 1

#define FF_ALLOC_EVENT   (SDLMOD_USEREVENT)
#define FF_REFRESH_EVENT (SDLMOD_USEREVENT + 1)
#define FF_QUIT_EVENT (SDLMOD_USEREVENT + 2)

#define SDL_VIDEO_MODE_BPP 24
//#define SDL_VIDEO_MODE_FLAGS SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0
#define SAMPLE_CORRECTION_PERCENT_MAX 10

#define DEFAULT_AV_SYNC_TYPE AV_SYNC_VIDEO_MASTER

enum {
	AV_SYNC_VIDEO_MASTER,
	AV_SYNC_EXTERNAL_MASTER,
};

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} PacketQueue;

typedef struct VideoPicture {
	SDLMOD_Overlay *bmp;
	SDLMOD_Surface *sfc;
	int width, height;
	int allocated;
	double pts;
} VideoPicture;

typedef struct VideoState {
    char            filename[1024];
    AVFormatContext *ic;
	int				seek_req;
	int				seek_flags;
	int				seek_pos;

   	VideoPicture	pictq[VIDEO_PICTURE_QUEUE_SIZE];
	int				pictq_size, pictq_rindex, pictq_windex;
	pthread_mutex_t		*pictq_mutex;
	pthread_cond_t		*pictq_cond;
	int             videoStream;
	AVStream		*video_st;
	PacketQueue		videoq;
	double			video_clock; 

	/* 记录当前播放的帧，在刷新视频的时候会同时更新下面2个参数 */
	/* pts表示的是avcodec的内部时间，而pts_time的单位是微妙，  */
	/* 转换公式:pts_time = pts * TIME_BASE                     */
	int64_t			video_current_pts; 
	/* 当前帧的时间 = av_gettime() */
	int64_t			video_current_pts_time;
	pthread_t		*video_tid;

    struct SwrContext *swr_ctx;
    pthread_t      *parse_tid;
    int             quit;
	int				av_sync_type;

	double			frame_timer;
	double			frame_last_pts;
	double			frame_last_delay;
	double			external_clock_base;
} VideoState;

VideoState *global_video_state;
uint64_t global_video_pkt_pts = AV_NOPTS_VALUE;
int g_video_width, g_video_height;
char g_video_resized;
SDLMOD_Surface *screen;
pthread_mutex_t	*screen_mutex;
AVPacket flush_pkt;	

void save_bmp(void)
{
	if (screen)
	{
		SDLMOD_LockSurface(screen);
		dumpBMPRaw("image1_out.bmp", screen->pixels, screen->w, screen->h);
		SDLMOD_UnlockSurface(screen);
	}
}

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mutex, NULL);
    q->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->cond, NULL);
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;

	if(pkt != &flush_pkt && av_dup_packet(pkt) < 0) {
		return -1;
	}

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
    }

	pthread_mutex_unlock(q->mutex);

    return ret;
}

static void packet_queue_flush(PacketQueue *q) {
    AVPacketList *pkt, *pkt1;

	pthread_mutex_lock(q->mutex);

    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
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

int queue_picture(VideoState *is, AVFrame *pFrame, double pts) {
	VideoPicture *vp;
	enum PixelFormat dst_pix_fmt;
	AVPicture pict;
	static struct SwsContext *img_convert_ctx;

	//printf("queue_picture 1\n");
	pthread_mutex_lock(is->pictq_mutex);
	while(is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !is->quit) {
		{
	//printf("queue_picture 1 pictq_size = %d >= %d, \n", is->pictq_size, VIDEO_PICTURE_QUEUE_SIZE);
			struct timeval now;
			struct timespec timeout;
			int retcode;
			while (1)
			{
				gettimeofday(&now, NULL);
				timeout.tv_sec = now.tv_sec + 0;
				timeout.tv_nsec = now.tv_usec * 100;
				int ret;
				ret = pthread_cond_timedwait(is->pictq_cond, is->pictq_mutex, &timeout);
				if (ret == 0)
				{
					break;
				}
				else if (!(is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !is->quit))
				{
					break;
				}
				else if (is->quit)
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
	}
	pthread_mutex_unlock(is->pictq_mutex);
	//printf("queue_picture 2\n");
	if(is->quit) return -1;
	vp = &is->pictq[is->pictq_windex];
#if !USE_SWS_YUV_CONVERT
	if(!vp->bmp || 
#else
	if(!vp->sfc || 
#endif
		vp->width != is->video_st->codec->width || 
		vp->height != is->video_st->codec->height || 
		g_video_resized ){
		SDLMOD_Event event;
		vp->allocated = 0;
		event.type = FF_ALLOC_EVENT;
		event.user.data1 = is;
		SDLMOD_PushEvent(&event);
		pthread_mutex_lock(is->pictq_mutex);
		while(!vp->allocated && !is->quit) {
			struct timeval now;
			struct timespec timeout;
			int retcode;
			while (1)
			{
				gettimeofday(&now, NULL);
				timeout.tv_sec = now.tv_sec + 0;
				timeout.tv_nsec = now.tv_usec * 100;
				int ret;
				ret = pthread_cond_timedwait(is->pictq_cond, is->pictq_mutex, &timeout);
				if (ret == 0)
				{
					break;
				}
				else if (!(!vp->allocated && !is->quit))
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
		pthread_mutex_unlock(is->pictq_mutex);
		if(is->quit) return -1;
	}
	//printf("queue_picture 3\n");
#if !USE_SWS_YUV_CONVERT
	if(vp->bmp) {
#else
	if(vp->sfc) {
#endif

#if !USE_SWS_YUV_CONVERT
		SDLMOD_LockYUVOverlay(vp->bmp);
		dst_pix_fmt = PIX_FMT_YUV420P;
		pict.data[0] = vp->bmp->pixels[0];
		pict.data[1] = vp->bmp->pixels[2];
		pict.data[2] = vp->bmp->pixels[1];

		pict.linesize[0] = vp->bmp->pitches[0];
		pict.linesize[1] = vp->bmp->pitches[2];
		pict.linesize[2] = vp->bmp->pitches[1];

		if(img_convert_ctx == NULL) {
			int w = is->video_st->codec->width;
			int h = is->video_st->codec->height;
			img_convert_ctx = sws_getContext(w, h, is->video_st->codec->pix_fmt,
											 w, h, dst_pix_fmt, SWS_BICUBIC, 
											 NULL, NULL, NULL);
			if(img_convert_ctx == NULL) {
				fprintf(stderr, "Connot initialize the convertion context!\n");
				exit(1);
			}
		}
		sws_scale(img_convert_ctx, (const uint8_t**)pFrame->data, pFrame->linesize,
									0, is->video_st->codec->height, pict.data, pict.linesize);
		SDLMOD_UnlockYUVOverlay(vp->bmp);
#else
		SDLMOD_LockSurface(vp->sfc);
		dst_pix_fmt = PIX_FMT_RGB24;
		pict.data[0] = &((uint8_t*)vp->sfc->pixels)[0];
		pict.data[1] = &((uint8_t*)vp->sfc->pixels)[1];
		pict.data[2] = &((uint8_t*)vp->sfc->pixels)[2];

		pict.linesize[0] = vp->sfc->pitch;
		pict.linesize[1] = vp->sfc->pitch;
		pict.linesize[2] = vp->sfc->pitch;

		if(img_convert_ctx == NULL) {
			int w = is->video_st->codec->width;
			int h = is->video_st->codec->height;
			img_convert_ctx = sws_getContext(w, h, is->video_st->codec->pix_fmt,
											 w, h, dst_pix_fmt, SWS_BICUBIC, 
											 NULL, NULL, NULL);
			if(img_convert_ctx == NULL) {
				fprintf(stderr, "Connot initialize the convertion context!\n");
				exit(1);
			}
		}
		sws_scale(img_convert_ctx, (const uint8_t**)pFrame->data, pFrame->linesize,
									0, is->video_st->codec->height, pict.data, pict.linesize);
		SDLMOD_UnlockSurface(vp->sfc);
#endif
		
		vp->pts = pts;
		if(++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) is->pictq_windex = 0;
		pthread_mutex_lock(is->pictq_mutex);
		is->pictq_size++;
		pthread_mutex_unlock(is->pictq_mutex);
	}
	//printf("queue_picture end\n");
	return 0;
}

/* 获取当前视频时间 */
static double get_video_clock(VideoState *is) {
	double delta;

	delta = (av_gettime() - is->video_current_pts_time) / 1000000.0;
	return is->video_current_pts + delta;
}

static double get_external_clock(VideoState *is) {
	return (av_gettime() / 1000000.0) - is->external_clock_base;
}

/* 这个函数根据不同的同步方法来获取当前的播放时间 */
/* 可能是video,audio,local time                   */
static double get_master_clock(VideoState *is) {
	if(is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
		return get_video_clock(is);
	} else {
		return get_external_clock(is);
	}
}

double synchronize_video(VideoState *is, AVFrame *src_frame, double pts) {
	double frame_delay;

	if(pts != 0) {
		is->video_clock = pts;
	} else {
		pts = is->video_clock;
	}

	frame_delay = av_q2d(is->video_st->codec->time_base);
	frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
	is->video_clock += frame_delay;
	return pts;
}

int our_get_buffer(struct AVCodecContext *c, AVFrame *pic) {
	int ret = avcodec_default_get_buffer(c, pic);
	uint64_t *pts = (uint64_t*)av_mallocz(sizeof(uint64_t));
	*pts = global_video_pkt_pts;
	pic->opaque = pts;
	//fprintf(stderr, "2: AVFrame.pkt_pts = %lld\n", pic->opaque);
	return ret;
}	

void our_release_buffer(struct AVCodecContext *c, AVFrame *pic) {
	if(pic) av_freep(&pic->opaque);
	avcodec_default_release_buffer(c, pic);
}

int video_thread(void *arg) {
	VideoState *is = (VideoState*)arg;
	AVPacket pkt1, *packet = &pkt1;
	int len1, frameFinished;
	AVFrame *pFrame;
	double pts;

	pFrame = avcodec_alloc_frame();

	for(;;) {
		//printf("video_thread loop 1\n");
		if(packet_queue_get(&is->videoq, packet, 1) < 0) {
			fprintf(stderr, "%d: packet_queue_get errror\n", __LINE__);
			break;
		}
		//printf("video_thread loop 2\n");
		if(packet->data == flush_pkt.data) {
			avcodec_flush_buffers(is->video_st->codec);
			continue;
		}
		//printf("video_thread loop 3\n");
		pts = 0;
		global_video_pkt_pts = packet->pts;
		//printf("video_thread loop 4\n");
		len1 = avcodec_decode_video2(is->video_st->codec, pFrame, &frameFinished, packet);
/*
		if(packet->dts == AV_NOPTS_VALUE && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE) {
			pts = *(uint64_t*)pFrame->opaque;
		} else if(packet->dts != AV_NOPTS_VALUE) {
			pts = packet->dts;
		} else {
			pts = 0;
		}

		pts *= av_q2d(is->video_st->time_base);
*/
		//printf("video_thread loop 5\n");
		if(frameFinished) {
			//printf("video_thread loop 6\n");
			pts = synchronize_video(is, pFrame, pts);
			//printf("video_thread loop 7\n");
			if(queue_picture(is, pFrame, pts) < 0) 
			{
				//printf("video_thread loop 8\n");
				break;
			}
		}
		//printf("video_thread loop 6\n");
		av_free_packet(packet); 
	}
	av_free(pFrame);
	//printf("video_thread loop end\n");
	return 0;
}

void *videoThread(void *data)
{
	video_thread(data);
	return NULL;
}

int stream_component_open(VideoState *is, int stream_index) {
		AVFormatContext *ic = is->ic;
		AVCodecContext *codecCtx;
		AVCodec *codec;
		int64_t wanted_channel_layout = 0;
		int wanted_nb_channels;
		const int next_nb_channels[] = {0, 0, 1 ,6, 2, 6, 4, 6};

		if (stream_index < 0 || stream_index >= ic->nb_streams) {
			return -1;
		}
		
		codecCtx = ic->streams[stream_index]->codec;

    codec = avcodec_find_decoder(codecCtx->codec_id);
    if (!codec || (avcodec_open2(codecCtx, codec, NULL) < 0)) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }

//	ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch(codecCtx->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		is->videoStream = stream_index;
		is->video_st = ic->streams[stream_index];
		is->frame_timer = (double)av_gettime() / 1000000.0;
		is->frame_last_delay = 40e-3;
		is->video_current_pts_time = av_gettime();
		packet_queue_init(&is->videoq);
		//is->video_tid = SDL_CreateThread(video_thread, is);
		{
			int err;
			is->video_tid = (pthread_t *)malloc(sizeof(pthread_t));
			err = pthread_create(is->video_tid, NULL, videoThread, is);
			if (err!=0)
			{
				free(is->video_tid);
				fprintf(stderr, "can't create thread: %s\n", strerror(err));
				exit(0);
			}
		}
		
		codecCtx->get_buffer = our_get_buffer;
		codecCtx->release_buffer = our_release_buffer;
    default:
        break;
    }
}



static int decode_interrupt_cb(void *arg) {
	return (global_video_state && global_video_state->quit);
}

static int decode_thread(void *arg) {
    VideoState *is = (VideoState *)arg;
    AVFormatContext *ic = NULL;
    AVPacket pkt1, *packet = &pkt1;
    int ret, i;
	int video_index = -1;

	is->videoStream = -1;

    global_video_state = is;
    if (avformat_open_input(&ic, is->filename, NULL, NULL) != 0) {
        return -1;
    }

    /* 这个回调函数将赋值给AVFormatContex,这样当读取流出现问题的时候会调用我们的自己的处理 */
	static const AVIOInterruptCB int_cb = { decode_interrupt_cb, NULL };
	ic->interrupt_callback = int_cb;

    is->ic = ic;
	is->external_clock_base = 0;
	is->external_clock_base = get_external_clock(is);

    if (avformat_find_stream_info(ic, NULL) < 0) {
        return -1;
    }

    av_dump_format(ic, 0, is->filename, 0);

    for(i=0; i<ic->nb_streams; i++) {
		if(ic->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && video_index < 0)
			video_index = i;
    }
	
	if(video_index >= 0) stream_component_open(is, video_index); 

    /* 开始解码主循环 */
    for(;;) {
        if(is->quit) 
		  break;
		/* 这里处理视频的快进和快退 */
		if(is->seek_req) {
			int stream_index = -1;
			int64_t seek_target = is->seek_pos;

			is->external_clock_base = 0;
			is->external_clock_base = get_external_clock(is) - (seek_target / 1000000.0);
			
			if(is->videoStream >= 0) 
			  stream_index = is->videoStream;
		
			/* av_rescale_q(a, b, c):通过计算a*b/c来把一个时间基调整到另一个时间基 */
			/* 使用这个函数的原因是可以防止计算溢出，AV_TIME_BASE_Q是AV_TIME_BASE作*/
			/* 为分母的版本                                                        */
			if(stream_index >= 0) 
				seek_target = av_rescale_q(seek_target, AV_TIME_BASE_Q , ic->streams[stream_index]->time_base);

			if(av_seek_frame(is->ic, stream_index, seek_target, is->seek_flags) < 0) {
				fprintf(stderr, "%d: %s error seek\n", __LINE__, is->filename);
			} else {
				/* 跳转后需要清空我们自己的缓冲队列和avcodec内部缓冲*/
				/* 然后放入一个用来标识刷新队列的标志包             */
				if(is->videoStream >= 0) {
					packet_queue_flush(&is->videoq);
					packet_queue_put(&is->videoq, &flush_pkt);
				}
			}

			is->seek_req = 0;
		}
		//printf("decode_thread loop check videoq size %d\n", is->videoq.size);
        if (is->videoq.size > MAX_VIDEOQ_SIZE) {
			//printf("decode_thread loop delay %d\n", is->videoq.size);
            SDLMOD_Delay(10);
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

		if(packet->stream_index == is->videoStream) {
			packet_queue_put(&is->videoq, packet);
        } else {
            av_free_packet(packet);
        }
        //printf("decode_thread loop %d\n", is->videoq.size);
    }

	printf("decode_thread loop wait start\n");
	save_bmp();
    while (!is->quit && is->videoq.size > 0) {
        //printf("decode_thread delay %d\n", is->videoq.size);
        SDLMOD_Delay(100);
    }

fail: {
        SDLMOD_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDLMOD_PushEvent(&event);
        printf("push FF_QUIT_EVENT\n");
    }

    return 0;
}

void alloc_picture(void *userdata) {
	VideoState *is = (VideoState*)userdata;
	VideoPicture *vp;

	vp = &is->pictq[is->pictq_windex];
#if !USE_SWS_YUV_CONVERT
	if(vp->bmp) {
		SDLMOD_FreeYUVOverlay(vp->bmp);
	}
#else
	if(vp->sfc) {
		SDLMOD_FreeSurface(vp->sfc);
	}
#endif

	if(g_video_resized) {
		pthread_mutex_lock(screen_mutex);
		screen = NULL;
		//screen = SDL_SetVideoMode(g_video_width, g_video_height, SDL_VIDEO_MODE_BPP, SDL_VIDEO_MODE_FLAGS);
		screen = SDLMOD_CreateRGBSurface(0,
			g_video_width, g_video_height, SDL_VIDEO_MODE_BPP,
			Rmask, Gmask, Bmask, Amask
			);
		pthread_mutex_unlock(screen_mutex);
		g_video_resized = 0;
	}
#if !USE_SWS_YUV_CONVERT
	vp->bmp = SDLMOD_CreateYUVOverlay(is->video_st->codec->width,
								   is->video_st->codec->height,
								   SDLMOD_YV12_OVERLAY,
								   screen);
#else
	vp->sfc = SDLMOD_CreateRGBSurface(0,
			is->video_st->codec->width, is->video_st->codec->height, 
			SDL_VIDEO_MODE_BPP, Rmask, Gmask, Bmask, Amask
			);
#endif
	
	vp->width = is->video_st->codec->width;
	vp->height = is->video_st->codec->height;

	pthread_mutex_lock(is->pictq_mutex);
	vp->allocated = 1;
	pthread_cond_signal(is->pictq_cond);
	pthread_mutex_unlock(is->pictq_mutex);
}

static uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque) {
	SDLMOD_Event event;
	event.type = FF_REFRESH_EVENT;
	event.user.data1 = opaque;
	SDLMOD_PushEvent(&event);

	return 0; // 0 means stop timer
}

/* delay毫秒刷新 */
static void schedule_refresh(VideoState *is, int delay) {
	SDLMOD_AddTimer(delay, sdl_refresh_timer_cb, is);
}

void video_display(VideoState *is) {
	SDLMOD_Rect rect;
	VideoPicture *vp;
	AVPicture pict;
	float aspect_ratio;
	int w, h, x, y;
	int i;

	vp = &is->pictq[is->pictq_rindex];
#if !USE_SWS_YUV_CONVERT
	if(vp->bmp) {
#else
	if(vp->sfc) {
#endif
		if(is->video_st->codec->sample_aspect_ratio.num == 0) {
			aspect_ratio = 0;
		} else {
			aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio) *
								  is->video_st->codec->width / is->video_st->codec->height;
		}
		if(aspect_ratio <= 0.0) 
			aspect_ratio = (float)is->video_st->codec->width / (float)is->video_st->codec->height;
		
		h = screen->h;
		w = ((int)(h * aspect_ratio)) & -3;
		if(w > screen->w) {
			w = screen->w;
			h = ((int)(w / aspect_ratio)) & -3;
		}
		x = (screen->w - w) / 2;
		y = (screen->h - h) / 2;

		rect.x = x;
		rect.y = y;
		rect.w = w;
		rect.h = h;

#if !USE_SWS_YUV_CONVERT
		SDLMOD_DisplayYUVOverlay(vp->bmp, &rect);
#else
		if (vp->sfc->w > 0 && vp->sfc->h > 0 && rect.w > 0 && rect.h > 0)
		{
			SDLMOD_Rect srcrect;
			srcrect.x = 0;
			srcrect.y = 0;
			srcrect.w = vp->sfc->w;
			srcrect.h = vp->sfc->h;
			SDLMOD_LockSurface(screen);
			//FIXME: SoftStretch doesn't support empty rect (dstrect->h == 0), will crash.
			SDLMOD_SoftStretch(vp->sfc, &srcrect, screen, &rect);
			SDLMOD_UnlockSurface(screen);
		}
#endif
#if USE_GL
		glutPostRedisplay();
#endif
	}
}

void stream_seek(VideoState *is, int64_t pos, int rel) {
	if(!is->seek_req) {
		is->seek_pos = pos;
		is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
		is->seek_req = 1;
	}
}

void video_refresh_timer(void *userdata) {
	VideoState *is = (VideoState*)userdata;
	VideoPicture *vp;
	double actual_delay, delay, sync_threshold, ref_clock, diff;
	if(is->video_st) {
		if(is->pictq_size == 0) {
			schedule_refresh(is, 1);		
		} else {
			vp = &is->pictq[is->pictq_rindex];
			is->video_current_pts = vp->pts;
			is->video_current_pts_time = av_gettime();
			delay = vp->pts - is->frame_last_pts;
			if(delay <= 0 || delay >= 1.0) delay = is->frame_last_delay;
			is->frame_last_delay = delay;
			is->frame_last_pts = vp->pts;
			if(is->av_sync_type != AV_SYNC_VIDEO_MASTER) {
				ref_clock = get_master_clock(is);
				diff = vp->pts - ref_clock;
				sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
				if(fabs(diff) < AV_NOSYNC_THRESHOLD) {
					if(diff <= -sync_threshold)	{
						delay = 0;
					} else if(diff >= sync_threshold) {
						delay = 2 * delay;
					}
				}
			}
			is->frame_timer += delay;	
			actual_delay = is->frame_timer - (av_gettime() / 1000000.0);
			if(actual_delay < 0.010) actual_delay = 0.010;
			schedule_refresh(is, (int)(actual_delay * 1000 + 0.5));
			video_display(is);
			if(++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) is->pictq_rindex = 0;
			pthread_mutex_lock(is->pictq_mutex);
			is->pictq_size--;
			//printf("video_refresh_timer signal %d\n", is->pictq_size);
			pthread_cond_signal(is->pictq_cond);
			pthread_mutex_unlock(is->pictq_mutex);
		 }
	} else
	  schedule_refresh(is, 100);
}

void *decodeThread(void *data)
{
	decode_thread(data);
	return NULL;
}

//#ifdef main
//#undef main
//#endif


#if !DEBUG_SDLMOD_TIMER

void *event_loop(void *data)
{
	SDLMOD_Event event;
	VideoState *is;
	is = (VideoState *)data;
    for(;;) {
		double incr, pos;

        SDLMOD_WaitEvent(&event);
        switch(event.type) {
#if 0
		case SDL_VIDEORESIZE:
			g_video_width = event.resize.w;
			g_video_height = event.resize.h;
			g_video_resized = 1;
			break;
		
		case SDL_KEYDOWN:
			switch(event.key.keysym.sym) {
			case SDLK_LEFT:
				incr = -1.0;
				goto do_seek;
			case SDLK_RIGHT:
				incr = 1.0;
				goto do_seek;
			case SDLK_UP:
				incr = 6.0;
				goto do_seek;
			case SDLK_DOWN:
				incr = -6.0;
				goto do_seek;
			do_seek:
				if(global_video_state) {
					/* 获取当前播放位置 */
					pos = get_master_clock(global_video_state);
					pos += incr;
					stream_seek(global_video_state, (int64_t)(pos * AV_TIME_BASE), incr);
				}
			default: 
				break;
			}
			break;
#endif

        case FF_QUIT_EVENT:
        case SDLMOD_QUIT:
            SDLMOD_TimerQuit(); //FIXME: no refresh op
            SDLMOD_StopEventLoop();
            is->quit = 1;
            exit(0);
            break;
		case FF_ALLOC_EVENT:
			//printf("FF_ALLOC_EVENT begin\n");
			alloc_picture(event.user.data1);
			//printf("FF_ALLOC_EVENT end\n");
			break;
			
		case FF_REFRESH_EVENT:
			//printf("FF_REFRESH_EVENT begin\n");
			video_refresh_timer(event.user.data1);
			//printf("FF_REFRESH_EVENT end\n");
			break;
			
        default:
			printf("event.type == %d\n", event.type);
            break;
        }
    }
}

#if USE_GL

void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0.0f, 0.0f, 0.0f);
	
	if (1)
	{
		if (textureContainsData) {
			glDeleteTextures(1, &g_TextureArray[g_ID]);
		}
		
		glGenTextures(1, &g_TextureArray[g_ID]);
		glBindTexture(GL_TEXTURE_2D, g_TextureArray[g_ID]);
		
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		
		//glTexImage2D(GL_TEXTURE_2D, 0, nOfColors, frame->w, frame->h, 0,
		//			  texture_format, GL_UNSIGNED_BYTE, frame->pixels);
			
			
		if (0)
		{	
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 
				g_TextureWidth, g_TextureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
				texture_data);
		}
		else
		{
			//FIXME: multi thread here, screen will be null (not timer thread)
			pthread_mutex_lock(screen_mutex);
			if (screen != NULL)
			{
				SDLMOD_LockSurface(screen);
				//dumpBMPRaw("image1_out.bmp", screen->pixels, screen->w, screen->h);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 
					screen->w, screen->h, 0, GL_RGB, GL_UNSIGNED_BYTE, 
					screen->pixels);
				SDLMOD_UnlockSurface(screen);
			}
			pthread_mutex_unlock(screen_mutex);
		}
		
		textureContainsData = 1;
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, g_TextureArray[0]);
	}
	
	glScalef(1.0f, -1.0f, 1.0f);
	
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);
	glEnd();

	glutSwapBuffers();
}

void reshape(int w, int h)
{
	g_video_width = w;
	g_video_height = h;
	g_video_resized = 1;
	
	if (h == 0)
	{
		h = 1;
	}
	glViewport(0, 0, w, h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	//FIXME:
	//gluPerspective(45.0f, (GLfloat)w / (GLfloat)h, 0.1f, 50.0f);

	//gluOrtho2D(-1.0f, 1.0f, -1.0f, 1.0f);
	glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void mouse(int button, int state, int mx, int my)
{
}

void motion(int mx, int my)
{
}

void idle(void)
{
}

int initEnvironment(void)
{	
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_TEXTURE_2D);

	if (1)
	{
		texture_data = loadBMPRaw(FILENAME, &g_TextureWidth, &g_TextureHeight, 0, 1);	
		if (texture_data)
		{
			glGenTextures(1, &g_TextureArray[g_ID]);
			glBindTexture(GL_TEXTURE_2D, g_TextureArray[g_ID]);
			//gluBuild2DMipmaps(GL_TEXTURE_2D, 3, g_TextureWidth, g_TextureHeight, GL_RGB, GL_UNSIGNED_BYTE, data);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_TextureWidth, g_TextureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, texture_data);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			textureContainsData = 1;
			//free(texture_data);
			return 1;
		}
		else
		{
			return(0);
		}
	}
	else
	{
		texture_data = NULL;
		textureContainsData = 0;
		return 1;
	}
}

void menu(int value){
	printf("menu %d\n", value);
	if (value == 0) 
	{
		//glutDestroyWindow(win);
		//exit(0);
        SDLMOD_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = NULL;
        SDLMOD_PushEvent(&event);
        printf("push FF_QUIT_EVENT\n");
	}
	else if (value == 7)
	{
		save_bmp();
	}
	else 
	{
		val = value;
	}
	glutPostRedisplay();
}

void createMenu(void){
	//animeringsmeny = glutCreateMenu(menu);
	//glutAddMenuEntry("menu 1", 1);
	//glutAddMenuEntry("menu 2", 2);
	
	//springmeny = glutCreateMenu(menu);
	//glutAddMenuEntry("menu 3", 3);
	//glutAddMenuEntry("menu 4", 4);
	
	menyid = glutCreateMenu(menu);
	//glutAddSubMenu("menu 5", animeringsmeny);
	//glutAddSubMenu("menu 6", springmeny);
	glutAddMenuEntry("snapshot", 7);
	glutAddMenuEntry("exit", 0);
	
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

#endif







int main(int argc, char *argv[]) {
	double			pos;
    VideoState      *is;


    if(argc < 2) {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }

#ifdef __MINGW32__
	ptw32_processInitialize();
	//ptw32_processTerminate();
#endif

	//XInitThreads();

    is = (VideoState *)av_mallocz(sizeof(VideoState));

	avcodec_register_all();
	//avdevice_register_all();
	avfilter_register_all();
    av_register_all();
	avformat_network_init();

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
	if (SDLMOD_TimerInit() != 0)
	{
		fprintf(stderr, "SDLMOD_TimerInit failed\n");
		exit(1);
	}

	g_video_width = 640;
	g_video_height = 480;
	g_video_resized = 0;

	//screen = SDL_SetVideoMode(g_video_width, g_video_height, SDL_VIDEO_MODE_BPP, SDL_VIDEO_MODE_FLAGS);
	screen = SDLMOD_CreateRGBSurface(0,
		g_video_width, g_video_height, SDL_VIDEO_MODE_BPP,
		Rmask, Gmask, Bmask, Amask
		);
	if(!screen) {
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

    screen_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(screen_mutex, NULL);

    av_strlcpy(is->filename, argv[1], sizeof(is->filename));

	is->pictq_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(is->pictq_mutex, NULL);
    is->pictq_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(is->pictq_cond, NULL);
    
	schedule_refresh(is, 40);

	is->av_sync_type = AV_SYNC_VIDEO_MASTER;

    {
		int err;
		is->parse_tid = (pthread_t *)malloc(sizeof(pthread_t));
		err = pthread_create(is->parse_tid, NULL, decodeThread, is);
		if (err!=0)
		{
			free(is->parse_tid);
			printf("can't create thread: %s\n", strerror(err));
			exit(0);
		}
	}
    if (!is->parse_tid) {
        av_free(is);
        return -1;
    }

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t*)"FLUSH";

#if USE_EVENT_LOOP_MT
	{
		pthread_t pid;
		pthread_create(&pid, NULL, event_loop, is);
		if (0)
		{
			pthread_join(pid, NULL);
		}
		else
		{
#if USE_GL
			glutInit(&argc, argv);
			glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
			glutInitWindowSize(g_video_width, g_video_height);
			glutInitWindowPosition(0, 0);
			
			win = glutCreateWindow(TITLE);
			createMenu();

			glutDisplayFunc(display);
			glutReshapeFunc(reshape);
			glutMouseFunc(mouse);
			glutMotionFunc(motion);
			glutIdleFunc(idle);

			if (initEnvironment())
			{
				glutMainLoop();
			}
#else
			printf(">>>>>>Please input command (exit, quit, save):<<<<<<\n");
			char string[256];
			char* ret;
			while(1)
			{
				ret = gets(string);
				if (strcmp(string, "exit") == 0 || 
				    strcmp(string, "quit") == 0 ||
				    strcmp(string, "e") == 0 ||
				    strcmp(string, "q") == 0
				    )
				{
					break;
				}
				else if (strcmp(string, "save") == 0 ||
					strcmp(string, "s") == 0
				    )
				{
					save_bmp();
					printf("save_bmp() finish.\n");
				}
				else
				{
					printf("Please input command (exit, quit, save):\n");
				}
			}
#endif
		}
	}
#else
	event_loop(is);
#endif

    return 0;
}

#else

#define TEST_DELAY 1000

static uint32_t test_timer(uint32_t interval, void *opaque) {
	printf("test_timer: %s\n", (const char*)opaque);
	return 0;
}

int main(int argc, char *argv[]) {
#ifdef __MINGW32__
	ptw32_processInitialize();
	//ptw32_processTerminate();
#endif

	printf("test SDLMOD_TIMER\n");
	if (SDLMOD_TimerInit() != 0)
	{
		fprintf(stderr, "SDLMOD_TimerInit failed\n");
		return 0;
	}
	SDLMOD_AddTimer(TEST_DELAY, test_timer, "hello");
	
	while(1)
	{
		SDLMOD_Delay(100);
	}
	
	SDLMOD_TimerQuit();
	return 0;
}
#endif
