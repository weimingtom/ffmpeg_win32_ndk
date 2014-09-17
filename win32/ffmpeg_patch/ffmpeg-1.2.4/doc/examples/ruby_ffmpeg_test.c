#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <string.h>
#include <stdio.h>

#define reader_READ_BUFFER_SIZE		8192		// Buffer size for reading

typedef struct {
	struct ReSampleContext *	context;			// FFMPEG: Audio resampler context

	int 						src_channels;		// Channels for input frame
	int 						src_rate;			// Sample rate for input frame
	enum AVSampleFormat			src_format;			// Sample format for input frame

	int 						dst_channels;		// Channels for output frame
	int 						dst_rate;			// Sample rate for output frame
	enum AVSampleFormat			dst_format;			// Sample format for output frame
} AudioResamplerInternal;

typedef struct AudioFrameInternal{
	uint8_t * 					data;					// Raw sample data

	int							channels;				// FFMPEG: Audio channel count
	enum AVSampleFormat			format;					// FFMPEG: Format of the picture data
	int							samples;				// FFMPEG: Sample count
	int							rate;					// FFMPEG: Sample rate

	float						timestamp;				// Ruby: timestamp for this image (in seconds), or Qnil if not available
	float						duration;				// Ruby: duration of this image (in seconds), or Qnil if not available
} AudioFrameInternal;

struct ReaderInternal;
typedef struct StreamInternal {
	AVStream *			stream;					// FFMPEG: Actual stream

	struct ReaderInternal *	reader;				// Ruby: Pointer back to reader
	AVDictionary *     metadata;				// Ruby: Array of metadata
} StreamInternal;

typedef struct ReaderInternal {
	AVFormatContext *	format;					// FFMPEG: Format context
	AVIOContext *		protocol;				// FFMPEG: IO context

	FILE *				io;						// Ruby: IO class to read from
	//VALUE				streams;				// Ruby: Streams wrapped in Ruby objects
	AVDictionary *      metadata;				// Ruby: Array of metadata
} ReaderInternal;

extern AudioResamplerInternal *audio_resampler_new(
	int src_channels, int src_rate, enum AVSampleFormat src_format, 
	int dst_channels, int dst_rate, enum AVSampleFormat dst_format);
extern void audio_resampler_free(AudioResamplerInternal * internal);
extern AudioFrameInternal *audio_resampler_resample(AudioResamplerInternal * internal, AudioFrameInternal * internal_frame);

extern AudioFrameInternal *audio_frame_new2(uint8_t * data, int channels, enum AVSampleFormat format, int samples, int rate, float timestamp, float duration);
extern AudioFrameInternal *audio_frame_new(AVFrame * frame, AVCodecContext * codec);
extern void audio_frame_free(AudioFrameInternal * internal);
extern uint8_t *audio_frame_data(AudioFrameInternal * internal, int *size);

extern StreamInternal * stream_new(ReaderInternal * reader, AVStream * stream);
extern void stream_free(StreamInternal * internal);
extern void stream_print_av_dictionary(StreamInternal *internal);
extern enum AVMediaType stream_type(StreamInternal * internal);

extern AudioFrameInternal *audio_stream_decode(StreamInternal * internal);
extern AudioResamplerInternal *audio_stream_resampler(StreamInternal * internal,
	int dst_channels, int dst_rate, enum AVSampleFormat dst_format);

extern ReaderInternal *reader_new(FILE *file);
extern void reader_free(ReaderInternal *internal);
extern int reader_get_streams_size(ReaderInternal *internal);
extern StreamInternal *reader_get_streams_by_id(ReaderInternal *internal, int id);
extern void reader_print_av_dictionary(ReaderInternal *internal);
extern int reader_find_next_stream_packet(ReaderInternal * internal, AVPacket * packet, int stream_index);

extern void get_VERSION(char* str, int str_size);
extern const char *get_CONFIGURATION();
extern const char *get_LICENSE();

//--------------------------------------------------------

static AudioResamplerInternal * audio_resampler_alloc(void) 
{
	AudioResamplerInternal * internal = (AudioResamplerInternal *)av_mallocz(sizeof(AudioResamplerInternal));
	if (!internal) 
	{
		fprintf(stderr, "Failed to allocate internal structure\n");
		exit(0);
	}
	return internal;
}
static AudioResamplerInternal *audio_resampler_initialize(AudioResamplerInternal * internal,
	int src_channels, int src_rate, enum AVSampleFormat src_format, 
	int dst_channels, int dst_rate, enum AVSampleFormat dst_format) 
{
	internal->src_channels 	= src_channels;
	internal->src_rate		= src_rate;
	internal->src_format	= src_format;

	internal->dst_channels	= dst_channels;
	internal->dst_rate		= dst_rate;
	internal->dst_format	= dst_format;

	if (internal->src_format == AV_SAMPLE_FMT_NONE) 
	{
		fprintf(stderr, "Unknown input sample format\n");
		exit(0);
	}
	if (internal->dst_format == AV_SAMPLE_FMT_NONE)
	{
		fprintf(stderr, "Unknown output sample format\n");
		exit(0);
	}
	// Create audio resampler
	internal->context = av_audio_resample_init(
		internal->dst_channels, internal->src_channels,
		internal->dst_rate, internal->src_rate,
		internal->dst_format, internal->src_format,
		0, 0, 1, 0.0);
	if (!internal->context)
	{
		fprintf(stderr, "Failed to create resampling context\n");
		exit(0);
	}
	return internal;
}
AudioResamplerInternal *audio_resampler_new(
	int src_channels, int src_rate, enum AVSampleFormat src_format, 
	int dst_channels, int dst_rate, enum AVSampleFormat dst_format) 
{
	return audio_resampler_initialize(audio_resampler_alloc(), 
		src_channels, src_rate, src_format, 
		dst_channels, dst_rate, dst_format);
}
void audio_resampler_free(AudioResamplerInternal * internal) 
{
	if (internal) 
	{
		if (internal->context)
		{
			audio_resample_close(internal->context);
		}
		av_free(internal);
	}
}
AudioFrameInternal *audio_resampler_resample(AudioResamplerInternal * internal, AudioFrameInternal * internal_frame) 
{
	// Allocate enough memory for output samples
	// (we have to use the max channels, as there seems to be a bug in FFMPEG of overshooting the buffer when downsampling)
	int max_channels = (internal->src_channels > internal->dst_channels) ? internal->src_channels : internal->dst_channels;
	int bytes_per_sample = av_get_bytes_per_sample(internal->dst_format) * max_channels;

	uint8_t * dst_data = (uint8_t *)av_malloc((internal_frame->samples * internal->dst_rate / internal->src_rate + 32) * bytes_per_sample);
	if (!dst_data)
	{
		fprintf(stderr, "Failed to allocate new sample buffer\n");
		exit(0);
	}
	// Resample
	int dst_samples = audio_resample(internal->context,
									 (short *)dst_data,
									 (short *)internal_frame->data,
									 internal_frame->samples);

	// Wrap into Ruby object
	return audio_frame_new2(dst_data,
		internal->dst_channels, internal->dst_format, dst_samples,
		internal->dst_rate, internal_frame->timestamp, internal_frame->duration);
}

//--------------------------------------------------------

static AudioFrameInternal *audio_frame_alloc(void) 
{
	AudioFrameInternal * internal = (AudioFrameInternal *)av_mallocz(sizeof(AudioFrameInternal));
	if (!internal) 
	{
		fprintf(stderr, "Failed to allocate internal structure\n");
		exit(0);
	}
	return internal;
}
AudioFrameInternal *audio_frame_new2(uint8_t * data, int channels, enum AVSampleFormat format, int samples, int rate, float timestamp, float duration) 
{
	AudioFrameInternal * internal = audio_frame_alloc();

	internal->data = data;

	internal->channels = channels;
	internal->format = format;
	internal->samples = samples;
	internal->rate = rate;

	internal->timestamp = timestamp;
	internal->duration = duration;

	return internal;
}
AudioFrameInternal *audio_frame_new(AVFrame * frame, AVCodecContext * codec) 
{
	// Time stamp: start of with presentation timestamp of frame
	int64_t timestamp = frame->pts;
	if (timestamp == (int64_t)AV_NOPTS_VALUE) 
	{
		// Fall back to presentation timestamp of packet
		timestamp = frame->pkt_pts;
		if (timestamp == (int64_t)AV_NOPTS_VALUE) 
		{
			// Fall back to decompression timestamp of packet
			timestamp = frame->pkt_dts;
		}
	}
	// Copy data into new sample buffer
	int plane_size = 0;
	//FIXME:
	int data_size = av_samples_get_buffer_size(&plane_size, codec->channels, frame->nb_samples/*codec->frame_size*/, codec->sample_fmt, 1);
	//fprintf(stderr, "data_size = %d, %d, %d, %d, %d\n", data_size, plane_size, codec->channels, frame->nb_samples/*codec->frame_size*/, codec->sample_fmt);
	int planes = av_sample_fmt_is_planar(codec->sample_fmt) ? codec->channels : 1;
	uint8_t * buffer = av_malloc(data_size);
	if (!buffer) 
	{
		fprintf(stderr, "Failed to allocate sample buffer %d, %d\n", data_size, planes);
		exit(0);
	}
	int i;
	for (i = 0; i < planes; ++i) 
	{
		memcpy(buffer + i * plane_size, frame->extended_data[i], plane_size);
	}
	// Clean up
	av_free(frame);
	// Call main init method
	return audio_frame_new2(buffer, codec->channels, codec->sample_fmt, frame->nb_samples/*codec->frame_size*/, codec->sample_rate,
		(timestamp != (int64_t)AV_NOPTS_VALUE) ? (float)(timestamp * av_q2d(codec->time_base)) : -1.0f,
		(float)(frame->nb_samples/*codec->frame_size*/ / (codec->sample_rate * 1000.0)));
}
void audio_frame_free(AudioFrameInternal * internal) 
{
	if (internal) 
	{
		if (internal->data)
		{
			av_free(internal->data);
		}
		av_free(internal);
	}
}
uint8_t *audio_frame_data(AudioFrameInternal * internal, int *size) 
{
	int bytes_per_sample = av_get_bytes_per_sample(internal->format);
	//FIXME: int ? long ?
	*size = bytes_per_sample * internal->channels * internal->samples;
	//printf("audio_frame_data %d, %d, %d\n", bytes_per_sample, internal->channels, internal->samples);
	return internal->data;
}

//--------------------------------------------------------

static StreamInternal * stream_alloc(void) 
{
	StreamInternal *internal = (StreamInternal *)av_mallocz(sizeof(StreamInternal));
	if (!internal) 
	{
		fprintf(stderr, "Failed to allocate internal structure");
		exit(0);
	}
	return internal;
}

StreamInternal * stream_new(ReaderInternal * reader, AVStream * stream) 
{
	StreamInternal *internal = stream_alloc();
	internal->stream = stream;
	internal->reader = reader;
	internal->metadata = internal->stream->metadata;

	return internal;
}

void stream_free(StreamInternal * internal) 
{
	if (internal) 
	{
		av_free(internal);
	}
}

AudioFrameInternal *audio_stream_decode(StreamInternal * internal) 
{
	// Prepare codec
	if (!avcodec_is_open(internal->stream->codec)) 
	{
		const AVCodec * codec = internal->stream->codec->codec;
		if (!codec) 
		{
			codec = avcodec_find_decoder(internal->stream->codec->codec_id);
		}
		avcodec_open2(internal->stream->codec, codec, NULL);
	}
	// Find and decode next audio frame
	AVFrame * frame = avcodec_alloc_frame();
	for (;;) 
	{
		// Find next packet for this stream
		AVPacket packet;
		int found = reader_find_next_stream_packet(internal->reader, &packet, internal->stream->index);
		if (!found) 
		{
			// No more packets
			av_free(frame);
			return NULL;
		}
		// Decode audio frame
		int decoded = 0;
	    int err = avcodec_decode_audio4(internal->stream->codec, frame, &decoded, &packet);
		if (err < 0) 
		{
			fprintf(stderr, "audio_stream_decode Load Error %d\n", err);
			exit(0);
		}
		if (decoded) 
		{
			return audio_frame_new(frame, internal->stream->codec);
		}
	}
	return NULL;
}
AudioResamplerInternal *audio_stream_resampler(StreamInternal * internal,
	int dst_channels, int dst_rate, enum AVSampleFormat dst_format) 
{
	return audio_resampler_new(
		internal->stream->codec->channels, 
		internal->stream->codec->sample_rate, 
		internal->stream->codec->sample_fmt,
		dst_channels, dst_rate, dst_format);
}

//enum AVMediaType codec_type; /* see AVMEDIA_TYPE_xxx */
enum AVMediaType stream_type(StreamInternal * internal) 
{
	//AVMEDIA_TYPE_VIDEO //video
	//AVMEDIA_TYPE_AUDIO //audio
	//AVMEDIA_TYPE_DATA //data
	//AVMEDIA_TYPE_SUBTITLE //subtitle
	//AVMEDIA_TYPE_ATTACHMENT //attachment
	//default
	return internal->stream->codec->codec_type;
}

//--------------------------------------------------------

int reader_get_streams_size(ReaderInternal *internal) 
{
	AVFormatContext *format = internal->format;
	return format->nb_streams;
}

StreamInternal *reader_get_streams_by_id(ReaderInternal *internal, int id) 
{
	ReaderInternal *reader = internal;
	AVFormatContext *format = internal->format;
	if (id < format->nb_streams) 
	{
		//*type = format->streams[id]->codec->codec_type;
		switch (format->streams[id]->codec->codec_type) 
		{
		case AVMEDIA_TYPE_VIDEO: 
			// Video stream
			return stream_new(reader, format->streams[id]);
			break;
			
		case AVMEDIA_TYPE_AUDIO: 
			return stream_new(reader, format->streams[id]);
			break;
		
		default: 
			// All other streams
			return stream_new(reader, format->streams[id]);
			break;
		}
	}
	return NULL;
}

void reader_print_av_dictionary(ReaderInternal *internal) 
{
	AVDictionary *dict = internal->metadata;
	AVDictionaryEntry *temp = NULL;
	printf("===reader_print_av_dictionary start\n");
	while ((temp = av_dict_get(dict, "", temp, AV_DICT_IGNORE_SUFFIX)) != NULL) 
	{
		printf("<key> %s, <value> %s\n", temp->key, temp->value);
	}
	printf("===reader_print_av_dictionary end\n");
}

void stream_print_av_dictionary(StreamInternal *internal)
{
	AVDictionary *dict = internal->metadata;
	AVDictionaryEntry *temp = NULL;
	while ((temp = av_dict_get(dict, "", temp, AV_DICT_IGNORE_SUFFIX)) != NULL) 
	{
		printf("key == %s, value == %s\n", temp->key, temp->value);
	}
}

//--------------------------------------------------------

static int read_packet(void * opaque, uint8_t * buffer, int buffer_size) 
{
	ReaderInternal *internal = (ReaderInternal *)opaque;
	return fread(buffer, 1, buffer_size, internal->io);
}
static ReaderInternal *reader_alloc(void) 
{
	ReaderInternal * internal = (ReaderInternal *)av_mallocz(sizeof(ReaderInternal));
	if (!internal) 
	{
		fprintf(stderr, "Failed to allocate internal structure\n");
		exit(0);
	}
	internal->format = avformat_alloc_context();
	if (!internal->format) 
	{
		fprintf(stderr, "Failed to allocate FFMPEG format context\n");
		exit(0);
	}
	internal->protocol = avio_alloc_context(
		av_malloc(reader_READ_BUFFER_SIZE), 
		reader_READ_BUFFER_SIZE, 
		0, internal, read_packet, 
		NULL, NULL);
	if (!internal->protocol) 
	{
		fprintf(stderr, "Failed to allocate FFMPEG IO context\n");
		exit(0);
	}
	internal->protocol->seekable = 0;
	internal->format->pb = internal->protocol;
	return internal;
}
static ReaderInternal *reader_initialize(ReaderInternal *internal, FILE *file) 
{
	internal->io = file;
	// Open file via Ruby stream
	int err = avformat_open_input(&internal->format, "unnamed", NULL, NULL);
	if (err)
	{
		fprintf(stderr, "reader_initialize Load Error %d", err);
		exit(0);
	}
	// Read in stream information
	avformat_find_stream_info(internal->format, NULL);
	// Extract properties
	internal->metadata = internal->format->metadata;
	return internal;
}

ReaderInternal *reader_new(FILE *file)
{
	return reader_initialize(reader_alloc(), file);
}

void reader_free(ReaderInternal *internal) 
{
	if (internal) 
	{
		if (internal->format)
		{
			avformat_free_context(internal->format);
		}
		if (internal->protocol)
		{
			av_free(internal->protocol);
		}
		av_free(internal);
	}
}

// Find the next packet for the stream
int reader_find_next_stream_packet(ReaderInternal * internal, AVPacket * packet, int stream_index) 
{
	for (;;) 
	{
		int err = av_read_frame(internal->format, packet);
		if (err < 0) 
		{
			return 0;
		}
		if (packet->stream_index == stream_index) 
		{
			return 1;
		}
	}
}

void get_VERSION(char* str, int str_size)
{
	snprintf(str, str_size, "%d.%d.%d", 
		(avformat_version() >> 16) & 0xffff,
		(avformat_version() >>  8) & 0x00ff,
		(avformat_version()      ) & 0x00ff);
}

const char *get_CONFIGURATION()
{
	return avformat_configuration();
}

const char *get_LICENSE()
{
	return avformat_license();
}

int main(int argc, char **argv)
{
	// 8192, 4096, 2, 1024, 8
	//"cai.mp3"; // "123go_2ch.wav";//"test-2.mp4";
	const char* filename_input = "123go.wav";
	const char* filename_output = "test-2.raw";
	char str[256];
	FILE *fin = NULL, *fout = NULL;
	ReaderInternal *reader = NULL;
	int streams_size = 0;
	StreamInternal *stream = NULL, *audio_stream = NULL;
	AudioFrameInternal *audio_frame = NULL, *audio_frame2 = NULL;
	uint8_t * data;
	int data_size;
	int i, audio_decode_times = 0;
	AudioResamplerInternal *resampler = NULL;
	
	av_register_all();
	av_log_set_level(AV_LOG_QUIET);
	
	get_VERSION(str, sizeof(str));
	printf("get_VERSION() == %s\n", str);
	printf("get_CONFIGURATION() == %s\n", get_CONFIGURATION());
	printf("get_LICENSE() == %s\n", get_LICENSE());
	
	fin = fopen(filename_input, "rb");
	fout = fopen(filename_output, "wb+");
	if (fin && fout)
	{
		reader = reader_new(fin);
		if (reader)
		{
			reader_print_av_dictionary(reader);
			streams_size = reader_get_streams_size(reader);
			
			for (i = 0; i < streams_size; ++i)
			{
				stream = reader_get_streams_by_id(reader, i);
				if (stream)
				{
					if (stream_type(stream) == AVMEDIA_TYPE_AUDIO)
					{
						audio_stream = stream;
						break;
					}
				}
			}
			if (audio_stream)
			{
				printf("get audio stream success\n");
				
				resampler = audio_stream_resampler(audio_stream, 2, 41000, AV_SAMPLE_FMT_S16);
				if (resampler)
				{
					for (audio_decode_times = 0;; ++audio_decode_times)
					{
						audio_frame = audio_stream_decode(audio_stream);
						if (audio_frame)
						{
							audio_frame2 = audio_resampler_resample(resampler, audio_frame);
							if (audio_frame2)
							{
								data = audio_frame_data(audio_frame2, &data_size);
								printf("fwrite data_size %d\n", data_size);
								fwrite(data, 1, data_size, fout);
								
								audio_frame_free(audio_frame2);
								audio_frame2 = NULL;
							}
							audio_frame_free(audio_frame);
							audio_frame = NULL;
						}
						else
						{
							break;
						}
					}
					audio_resampler_free(resampler);
					resampler = NULL;
				}
				
				printf("audio_decode_times == %d\n", audio_decode_times);
				
				stream_free(audio_stream);
				audio_stream = NULL;
			}
			
			reader_free(reader);
			reader = NULL;
		}
		fclose(fout);
		fout = NULL;
		fclose(fin);
		fin = NULL;
	}
	else
	{
		fprintf(stderr, "open input file failed\n");
	}
	
	return 0;
}