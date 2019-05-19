// videoDecoder.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"
}


#define REFRESH_EVENT (SDL_USEREVENT + 1)
#define BREAK_EVENT (SDL_USEREVENT + 2)

int thread_exit = 0;

int refresh_event(void *opaque){
	thread_exit = 0;
	while (!thread_exit)
	{
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(40);
	}
	thread_exit = 0;
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	int screen_w, screen_h;
	SDL_Window *screen;
	SDL_Texture *textTure;
	SDL_Rect video_rect;
	SDL_Renderer *renderer;
	SDL_Thread *video_thread;
	SDL_Event event;

	AVFormatContext *pAvFormatContext;
	int i, videoIndex;
	AVCodecContext *pAcCodecContext;
	AVCodec *pAvCodec;
	AVPacket *avPacket;
	AVFrame *pFrame, *pFrameYuv;
	uint8_t *out_buffer;
	int y_size;
	int ret, got_picture;
	struct SwsContext *swsContext;
	char filePath[] = "屌丝男士.mov";

	//FILE* file_yuv = fopen("test.yuv", "wb+");

	int frame_cnt;
	av_register_all();
	avformat_network_init();
	pAvFormatContext = avformat_alloc_context();

	if (avformat_open_input(&pAvFormatContext, filePath, NULL, NULL) != 0)
	{
		printf("Couldn't open input stream.\n");
		return -1;
	}

	if (avformat_find_stream_info(pAvFormatContext, NULL) < 0)
	{
		printf("Couldn't find stream information.\n");
		return -1;
	}

	videoIndex = -1;
	for (size_t i = 0; i < pAvFormatContext->nb_streams; i++)
	{
		if (pAvFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoIndex = i;
			break;
		}
	}

	if (videoIndex == -1){
		printf("Didn't find a video stream.\n");
		return -1;
	}

	pAcCodecContext = pAvFormatContext->streams[videoIndex]->codec;
	pAvCodec = avcodec_find_decoder(pAcCodecContext->codec_id);

	if (pAvCodec == NULL){
		printf("Codec not found.\n");
		return -1;
	}

	if (avcodec_open2(pAcCodecContext, pAvCodec, NULL) < 0)
	{
		printf("Could not open codec.\n");
		return -1;
	}

	pFrame = av_frame_alloc();
	pFrameYuv = av_frame_alloc();

	out_buffer = (uint8_t*)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pAcCodecContext->width, pAcCodecContext->height));
	//avpicture_fill((AVPicture*)pFrameYuv, out_buffer, PIX_FMT_YUV420P, pAcCodecContext->width, pAcCodecContext->height);
	//讲一段缓存空间out_buffer关联到pFrameYuv（使其有存放的空间）
	avpicture_fill((AVPicture*)pFrameYuv, out_buffer, PIX_FMT_YUV420P, pAcCodecContext->width, pAcCodecContext->height);


	//输出视频文件格式信息
	av_dump_format(pAvFormatContext, 0, filePath, 0);

	swsContext = sws_getContext(pAcCodecContext->width, pAcCodecContext->height, pAcCodecContext->pix_fmt
		, pAcCodecContext->width, pAcCodecContext->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
	{
		printf("could not init SDL %s", SDL_GetError);
		return -1;
	}

	screen_w = pAcCodecContext->width;
	screen_h = pAcCodecContext->height;
	screen = SDL_CreateWindow("SDL window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w
		, screen_h, SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	renderer = SDL_CreateRenderer(screen, -1, 0);//0代表什么意思?
	textTure = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pAcCodecContext->width
		, pAcCodecContext->height);
	video_rect.x = 0;
	video_rect.y = 0;
	video_rect.w = screen_w;
	video_rect.h = screen_h;


	avPacket = (AVPacket*)av_malloc(sizeof(AVPacket));

	video_thread = SDL_CreateThread(refresh_event, NULL, NULL);

	frame_cnt = 0;

	for (;;){
		SDL_WaitEvent(&event);

		//	printf("Event type:%d\n", event.type);

		if (event.type == REFRESH_EVENT){
			//以下循环是确保每次REFRESH_EVENT事件到来都可以处理一帧视频帧
			while (1)
			{
				if (av_read_frame(pAvFormatContext, avPacket) < 0){
					thread_exit = 1;
				}

				//只有当当前帧为视频帧的时候才往下走，使得不会被音频帧占用延时
				if (avPacket->stream_index == videoIndex)
				{
					break;
				}
			}


			printf("av_read_frame \n");

			if (avPacket->stream_index == videoIndex)
			{
				ret = avcodec_decode_video2(pAcCodecContext, pFrame, &got_picture, avPacket);
				if (ret < 0){
					printf("Decode Error.\n");
					return -1;
				}

				if (got_picture)
				{
					sws_scale(swsContext, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pAcCodecContext->height, pFrameYuv->data
						, pFrameYuv->linesize);

					printf("Decoded frame index: %d\n", frame_cnt);

					//	fwrite(pFrameYuv->data[0], 1, pAcCodecContext->width*pAcCodecContext->height, file_yuv);

					SDL_UpdateTexture(textTure, NULL, pFrameYuv->data[0], pFrameYuv->linesize[0]);
					SDL_RenderClear(renderer);
					SDL_RenderCopy(renderer, textTure, NULL, NULL);
					SDL_RenderPresent(renderer);

					//fwrite(pFrame->data[0], 1, pAcCodecContext->width*pAcCodecContext->height, file_yuv);

					/*
					* 在此处添加输出YUV的代码
					* 取自于pFrameYUV，使用fwrite()
					*/

					frame_cnt++;
				}


				av_free_packet(avPacket);
			}
			else{
				//Exit Thread
				thread_exit = 1;
			}
		}
		else if (event.type == SDL_QUIT)
		{
			thread_exit = 1;
		}
		else if (event.type == BREAK_EVENT){
			break;
		}

	}

	sws_freeContext(swsContext);
	av_frame_free(&pFrame);
	av_frame_free(&pFrameYuv);

	avcodec_close(pAcCodecContext);
	avformat_close_input(&pAvFormatContext);

	return 0;
}

