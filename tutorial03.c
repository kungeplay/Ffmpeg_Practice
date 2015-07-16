#include "SDL.h"
#include "SDL_thread.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h" 
#include "libswresample/swresample.h"
#include "libavutil/samplefmt.h"
#define SDL_AUDIO_BUFFER_SIZE 1024

typedef struct PacketQueue
{
	AVPacketList *first_pkt,*last_pkt;
	int nb_packets;
	int size;
	SDL_mutex * mutex;
	SDL_cond * cond;
}PacketQueue;

int quit=0;
PacketQueue audioq;
void packet_queue_init(PacketQueue *q)
{
	memset(q,0,sizeof(PacketQueue));
	q->mutex=SDL_CreateMutex();
	q->cond=SDL_CreateCond();
}
int packet_queue_put(PacketQueue *q,AVPacket *pkt)
{
	AVPacketList *pktl;
	if (av_dup_packet(pkt)<0)
	{
		return -1;
	}
	pktl=av_malloc(sizeof(AVPacketList));
	if (!pktl)
	{
		return -1;
	}
	pktl->pkt=*pkt;
	pktl->next=NULL;
	SDL_LockMutex(q->mutex);
	if (!q->last_pkt)
	{
		q->first_pkt=pktl;
	}
	else
	{
		q->last_pkt->next=pktl;
	}
	q->last_pkt=pktl;
	++q->nb_packets;
	q->size+=pktl->pkt.size;
	SDL_CondSignal(q->cond);
	SDL_UnlockMutex(q->mutex);
	return 0;
}
static int packet_queue_get(PacketQueue *q,AVPacket *pkt,int block)
{
	AVPacketList *pktl;
	int ret;
	SDL_LockMutex(q->mutex);
	for (; ; )
	{
		if (quit)
		{
			ret=-1;
			break;
		}
		pktl=q->first_pkt;
		if (pktl)
		{
			q->first_pkt=pktl->next;
			if (!q->first_pkt)
			{
				q->last_pkt=NULL;
			}
			--q->nb_packets;
			q->size-=pktl->pkt.size;
			*pkt=pktl->pkt;
			av_free(pktl);
			ret=1;
			break;
		}
		else
		{
			SDL_CondWait(q->cond,q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) 
{
	static AVPacket pkt;
	static audio_pkt_size=0;
	static uint8_t *audio_pkt_data=NULL;
	//uint8_t *out[] = { audio_buf };
	int len1, data_size=0;
	static AVFrame pAudioFrame ;//= av_frame_alloc();  
	//av_frame_unref(pAudioFrame);

	for(;;) 
	{
		while(audio_pkt_size > 0)
		{
			int got_frame = 0;
			len1 = avcodec_decode_audio4(aCodecCtx, &pAudioFrame, &got_frame, 
						&pkt);
			if(len1 < 0)
			{
				/* if error, skip frame */
				audio_pkt_size = 0;
				break;
			}
			audio_pkt_data+=len1;
			//pkt.data+=len1;
			audio_pkt_size -= len1;
			if(got_frame == 0) 
			{
				/* No data yet, get more frames */
				continue;
			}
			else
			{
				/*SwrContext *swrContext = swr_alloc();
				swr_alloc_set_opts(swrContext, aCodecCtx->channel_layout, AV_SAMPLE_FMT_S16,
					aCodecCtx->sample_rate, aCodecCtx->channel_layout, aCodecCtx->sample_fmt,
					aCodecCtx->sample_rate, 0, NULL);  
				swr_init(swrContext); 
				printf("--------------------%d\n",pAudioFrame->format );
				//int m=(AVSampleFormat)pAudioFrame->format;
				swr_convert(swrContext, out, buf_size/aCodecCtx->channels/av_get_bytes_per_sample(AV_SAMPLE_FMT_S16),  
					(const uint8_t **)pAudioFrame->data, 
					pAudioFrame->linesize[0] / aCodecCtx->channels / av_get_bytes_per_sample(AV_SAMPLE_FMT_S32));  
					*/
				data_size = av_samples_get_buffer_size(NULL, aCodecCtx->channels, pAudioFrame.nb_samples, 
					AV_SAMPLE_FMT_S16, 0);
				memcpy(audio_buf, pAudioFrame.data[0], data_size);
				//av_free(&pAudioFrame);  
				av_free_packet(&pkt);
				//swr_free(&swrContext);
			}
			/* We have data, return it and come back for more later */
			return data_size;
		}
		if(pkt.data)
		{
			av_free_packet(&pkt);
		}

		if(quit) 
		{
			return -1;
		}

		if(packet_queue_get(&audioq, &pkt, 1) < 0) 
		{
			return -1;
		}
		audio_pkt_size=pkt.size;
		audio_pkt_data=pkt.data;
	}
}
void audio_callback(void *userdata,uint8_t *stream,int len)
{
	AVCodecContext *aCodecCtx=(AVCodecContext *)userdata;
	int len1,audio_size;
	static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE*3)/2];
	static unsigned int audio_buf_size=0;
	static unsigned int audio_buf_index=0;
	AVPacket *pkt=av_mallocz(sizeof(AVPacket));
	AVPacket *pkt_temp=av_mallocz(sizeof(AVPacket));
	AVFrame *frame=NULL;
	while(len>0)
	{
		if(audio_buf_index>=audio_buf_size)
		{
			/*We have already sent all our data; get more**/
			//audio_size=audio_decode_frame(aCodecCtx,pkt,pkt_temp,frame,audio_buf);
			audio_size=audio_decode_frame(aCodecCtx,audio_buf,sizeof(audio_buf));
			if(audio_size<0)
			{
				/*If error,output silence*/
				audio_buf_size=1024;
				memset(audio_buf,0,audio_buf_size);
			}
			else
			{
				audio_buf_size=audio_size;
			}
			audio_buf_index=0;
		}
		len1=audio_buf_size-audio_buf_index;
		if (len1>len)
		{
			len1=len;
		}
		memcpy(stream,(uint8_t*)audio_buf+audio_buf_index,len1);
		len-=len1;
		stream+=len1;
		audio_buf_index+=len1;
	}
}
int main(int argc,char ** argv)
{
	av_register_all();
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER))
	{
		fprintf(stderr, "Cloud not initialize SDL-%s\n",SDL_GetError());
		exit(1);
	}

	AVFormatContext *pFormatCtx;
	pFormatCtx=avformat_alloc_context();
	//Open video file
	#ifdef _FFMPEG_0_6_
		if(av_open_input_file(&pFormatCtx,argv[1],NULL,0,NULL))
	#else
		if (avformat_open_input(&pFormatCtx,argv[1],NULL,NULL)!=0)
	#endif
			return -1;//Couldn't open file
	//Retrieve stream information
	#ifdef _FFMPEG_0_6_
		if (av_find_stream_info(pFormatCtx)<0)
	#else
		if(avformat_find_stream_info(pFormatCtx,NULL)<0)
	#endif
			return -1;//Couldn't find stream information
	//Dump information about file onto standard error
	av_dump_format(pFormatCtx,0,argv[1],0);
	int i;
	AVCodecContext *pCodecCtx;
	AVCodecContext *aCodecCtx;
	//Find the first video stream
	int videoStream=-1;
	int audioStream=-1;
	for (i = 0; i < pFormatCtx->nb_streams; ++i)
	{
		if (pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO&&videoStream<0)
		{
			videoStream=i;
		}
		if (pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO&&audioStream<0)
		{
			audioStream=i;
		}
	}

	if (videoStream==-1)
	{   
		printf("Don't find a video stream\n");
		return -1;//Didn't find a video stream
	}
	if(audioStream==-1)
	{
		printf("Don't find a video stream\n");
		return -1;
	}	

	//Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;
	aCodecCtx=pFormatCtx->streams[audioStream]->codec;

	SDL_AudioSpec wanted_spec,spec;
	wanted_spec.freq=aCodecCtx->sample_rate;
	wanted_spec.format=AUDIO_S16SYS;
	wanted_spec.channels=aCodecCtx->channels;
	wanted_spec.silence=0;
	wanted_spec.samples=SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.callback=audio_callback;
	wanted_spec.userdata=aCodecCtx;

	if (SDL_OpenAudio(&wanted_spec,&spec)<0)
	{
		fprintf(stderr, "SDL_Openaudio:%s\n",SDL_GetError());
		return -1;
	}

	AVCodec *pCodec;
	AVCodec *aCodec;
	//Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	aCodec=avcodec_find_decoder(aCodecCtx->codec_id);
	if (pCodec==NULL)
	{
		fprintf(stderr,"Unsupported codec!\n");
		return -1;
	}
	printf("video decoder:%s\n", pCodec->name);
	if (!aCodec)
	{
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}
	printf("audio decoder:%s\n",aCodec->name );
	//Open video codec
	#ifdef _FFMPEG_0_6_
		if(avcodec_open(pCodecCtx,pCodec)<0)
	#else
		if(avcodec_open2(pCodecCtx,pCodec,NULL)<0)
	#endif
		return -1;//Could not open audio codec

	//Open audio codec
	#ifdef _FFMPEG_0_6_
		if(avcodec_open(aCodecCtx,aCodec)<0)
	#else
		if(avcodec_open2(aCodecCtx,aCodec,NULL)<0)
	#endif
		return -1;//Could not open audio codec

	packet_queue_init(&audioq);
	SDL_PauseAudio(0);

	AVFrame *pFrame;
	//Allocate video frame
	pFrame=avcodec_alloc_frame();

	SDL_Surface *screen;
	screen=SDL_SetVideoMode(pCodecCtx->width,pCodecCtx->height,0,0);
	if (!screen)
	{
		fprintf(stderr, "SDL:Could not set video mode-exiting\n");
	}
	SDL_Overlay *bmp;
	bmp=SDL_CreateYUVOverlay(pCodecCtx->width,pCodecCtx->height,SDL_YV12_OVERLAY,screen);;
	SDL_Rect rect;
	SDL_Event event;

	int frameFinished;
	AVPacket packet;
	i=0;
	av_init_packet(&packet);//////////////
	while(av_read_frame(pFormatCtx,&packet)>=0)
	{
		//printf("packet.stream_index=%d, packet.size=%d\n", packet.stream_index,packet.size);
		//Is this a packet from the video stream?
		if (packet.stream_index==videoStream)
		{
			//Decode video frame
			//avcodec_decode_video(pCodecCtx,pFrame,&frameFinished,packet.data,packet.size);
			avcodec_decode_video2(pCodecCtx,pFrame,&frameFinished,&packet);		
			//Did we get a video frame?
			if (frameFinished)
			{
				SDL_LockYUVOverlay(bmp);
				AVPicture pict;
				pict.data[0]=bmp->pixels[0];
				pict.data[1]=bmp->pixels[2];
				pict.data[2]=bmp->pixels[1];
				pict.linesize[0]=bmp->pitches[0];
				pict.linesize[1]=bmp->pitches[2];
				pict.linesize[2]=bmp->pitches[1];
				//Convert the image into YUV format that SDL uses
				static struct SwsContext *img_convert_ctx;
				img_convert_ctx=sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height,PIX_FMT_YUV420P,SWS_BICUBIC,NULL,NULL,NULL);
				if(img_convert_ctx==NULL)
				{
					fprintf(stderr,"Can not initialize the conversion context!\n");
					exit(1);
				}
				sws_scale(img_convert_ctx,(uint8_t const * const *)pFrame->data,pFrame->linesize,0,pCodecCtx->height,pict.data,pict.linesize);
				SDL_UnlockYUVOverlay(bmp);
				rect.x=0;
				rect.y=0;
				rect.w=pCodecCtx->width;
				rect.h=pCodecCtx->height;
				SDL_DisplayYUVOverlay(bmp,&rect);
			}
		}
		else if(packet.stream_index==audioStream)
		{
			packet_queue_put(&audioq,&packet);
		}
		else
		{
			av_free_packet(&packet);
		}

		//Free the packet that was allocated by av_read_frame
		//av_free_packet(&packet);
		SDL_PollEvent(&event);
		switch(event.type)
		{
			case SDL_QUIT:
				SDL_Quit();
				quit=1;
				exit(0);
				break;
			default:
				break;
		}
	}
	SDL_FreeYUVOverlay(bmp);
	//Free the RGB image
	//av_free(buffer);
	//av_free(pFrameRGB);
	//Free the YUV frame
	av_free(pFrame);
	//Close the codec
	avcodec_close(pCodecCtx);
	//Close the video file
	#ifdef _FFMPEG_0_6_
		av_close_input_file(pFormatCtx);
	#else
		avformat_close_input(&pFormatCtx);
	#endif
	avformat_free_context(pFormatCtx);
	return 0;
}
