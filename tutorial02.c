#include "SDL.h"
#include "SDL_thread.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h" 
int main(int argc,char ** argv)
{
	av_register_all();
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
	//Find the first video stream
	int videoStream=-1;
	for (i = 0; i < pFormatCtx->nb_streams; ++i)
	{
		if (pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoStream=i;
			break;
		}
	}
	if (videoStream==-1)
		return -1;//Didn't find a video stream
	//Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;
	AVCodec *pCodec;
	//Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec==NULL)
	{
		fprintf(stderr,"Unsupported codec!\n");
		return -1;
	}
	//Open codec
	#ifdef _FFMPEG_0_6_
		if(avcodec_open(pCodecCtx,pCodec)<0)
	#else
		if(avcodec_open2(pCodecCtx,pCodec,NULL)<0)
	#endif
		return -1;//Could not open codec
	AVFrame *pFrame;
	//Allocate video frame
	pFrame=avcodec_alloc_frame();
	//Allocate an AVFrame structure
	//AVFrame *pFrameRGB;
	//pFrameRGB=avcodec_alloc_frame();
	//if (pFrameRGB==NULL)
	//	return -1;
	//uint8_t *buffer;
	//int numBytes;
	//Determine required buffer size and allocate buffer
	//numBytes=avpicture_get_size(PIX_FMT_RGB24,pCodecCtx->width,pCodecCtx->height);
	//buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
	//Assign appropriate parts of buffer to image planes in pFrameRGB
	//Note that pFrameRGB is an AVFrame,but AVFrame is a superset
	//of AVPicture
	//avpicture_fill((AVPicture *)pFrameRGB,buffer,PIX_FMT_RGB24,pCodecCtx->width,pCodecCtx->height);

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER))
	{
		fprintf(stderr, "Cloud not initialize SDL-%s\n",SDL_GetError());
		exit(1);
	}
	SDL_Surface *screen;
	screen=SDL_SetVideoMode(pCodecCtx->width,pCodecCtx->height,0,0);
	if (!screen)
	{
		fprintf(stderr, "SDL:Could not set video mode-exiting\n");
	}
	SDL_Overlay *bmp;
	bmp=SDL_CreateYUVOverlay(pCodecCtx->width,pCodecCtx->height,SDL_YV12_OVERLAY,screen);
	printf("*bmp->pixels[0]=%d, *bmp->pixels[1]=%d,bmp->pitches[0]=%d,\n",*bmp->pixels[0],*bmp->pixels[1],bmp->pitches[0]);
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
				printf("*bmp->pixels[0]=%d, bmp->pitches[0]=%d\n",*bmp->pixels[0],bmp->pitches[0] );
				//Convert the image into YUV format that SDL uses
				static struct SwsContext *img_convert_ctx;
				img_convert_ctx=sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height,PIX_FMT_YUV420P,SWS_BICUBIC,NULL,NULL,NULL);
				if(img_convert_ctx==NULL)
				{
					fprintf(stderr,"Can not initialize the conversion context!\n");
					exit(1);
				}
				sws_scale(img_convert_ctx,(const uint8_t * const *)pFrame->data,pFrame->linesize,0,pCodecCtx->height,pict.data,pict.linesize);
				printf("-----*bmp->pixels[0]=%d, bmp->pitches[0]=%d\n",*bmp->pixels[0],bmp->pitches[0] );
				SDL_UnlockYUVOverlay(bmp);
				rect.x=0;
				rect.y=0;
				rect.w=pCodecCtx->width;
				rect.h=pCodecCtx->height;
				SDL_DisplayYUVOverlay(bmp,&rect);
			}
		}

		//Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
		SDL_PollEvent(&event);
		switch(event.type)
		{
			case SDL_QUIT:
				SDL_Quit();
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
