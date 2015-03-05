#include "AvcodecFrameGrabber.h"

namespace AvCodec
{
	extern "C"
	{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	}
}
 
using namespace AvCodec;

bool NeedStop;

CString timestamp_to_str(int64_t duration,int64_t units) {
	CString s;
	int hours, mins, secs, us;
	secs = duration / units;
	us = duration % units;
	mins = secs / 60;
	secs %= 60;
	hours = mins / 60;
	mins %= 60;

	s.Format(_T("%02d:%02d:%02d.%02d"), hours, mins, secs, (100 * us) / units);
	return s;
}

void memswap(void *p1, void *p2, size_t count) {
	char tmp, *c1, *c2;

	c1 = (char *)p1;
	c2 = (char *)p2;

	while (count--) 
	{
		tmp = *c2;
		*c2++ = *c1;
		*c1++ = tmp;
	}
};

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame,int headerlen,const CString& Title, CSampleGrabberCB * cb, int64_t target_frame) {
	cb->Grab = true;

	int dataSize = width*height * 3;
	uint8_t * data = new uint8_t[dataSize+100000];
	int lineSizeInBytes = pFrame->linesize[0];
	for ( int y=height-1; y>=0; y--) {
		memcpy(data+(height-y-1)*lineSizeInBytes, pFrame->data[0]+y*lineSizeInBytes,lineSizeInBytes );
	}

	cb->BufferCB((double)target_frame, data, dataSize);
	delete[] data;
}

#define INT64_MAX 0x7fffffffffffffffLL
#define INT64_MIN (-INT64_MAX - 1LL)

int av_grab_frames(int numOfFrames, CString fname, CSampleGrabberCB * cb,HWND progressBar,  bool Jump, bool SeekToKeyFrame)
{
	CString msg;
	if(numOfFrames<=0) return -1;

	AVFormatContext *pFormatCtx;
	int             i, videoStream;
	AVCodecContext  *pCodecCtx;
	AVCodec         *pCodec;
	AVFrame         *pFrame;
	AVFrame         *pFrameRGB;
	AVPacket        packet;
	int             frameFinished;
	int             numBytes;
	bool seekByBytes = false;

	uint8_t         *buffer;
	// Register all formats and codecs
	av_register_all();

	// Open video file
	USES_CONVERSION;
	char *fname1 = W2A((LPCTSTR)fname);
	pFormatCtx = avformat_alloc_context();
	if(avformat_open_input(&pFormatCtx,  fname1, NULL, 0/*, NULL*/)!=0) {
		//m_error.sprintf("Couldn't open file \"%s\"\r\n",(const char*)fname.toLocal8Bit());
		return -1; // Couldn't open file
	}

	AVInputFormat *inputFormat = av_find_input_format(W2A((LPCTSTR)fname));
	
	// Retrieve stream information
	if(av_find_stream_info(pFormatCtx)<0) {
		return -1; // Couldn't find stream information
	}
	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, W2A((LPCTSTR)fname), false);
	if ( pFormatCtx->iformat ) {
		seekByBytes= !!(pFormatCtx->iformat->flags & AVFMT_TS_DISCONT);
	}
   
	// Find the first video stream
	videoStream = -1;
	for (i=0; i<pFormatCtx->nb_streams; i++) {
		if ( pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			 videoStream=i;
			 break;
		}
	}

	if(videoStream==-1) {
		return -1; // Didn't find a video stream
	}
    // Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if( pCodec == NULL ){
		return -1; // Codec not found
	}

   // Open codec
   if ( avcodec_open(pCodecCtx, pCodec) < 0 ) {
		return -1; // Could not open codec
	}

    // Hack to correct wrong frame rates that seem to be generated by some codecs
    if(pCodecCtx->time_base.num>1000 && pCodecCtx->time_base.den==1)
		pCodecCtx->time_base.den=1000;

   // Allocate video frame
	pFrame = avcodec_alloc_frame();

   // Allocate an AVFrame structure
   pFrameRGB = avcodec_alloc_frame();
	memset( pFrameRGB->linesize, 0,sizeof(pFrameRGB->linesize));
	if ( pFrameRGB == NULL ){
		return -1;
	}

   // Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height) + 64;
	buffer=/*(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));*/(uint8_t*)malloc(numBytes);
	memset(buffer, 0, numBytes);
   int headerlen = sprintf((char *) buffer, "P6\n%d %d\n255\n", pCodecCtx->width,pCodecCtx->height);


   // Assign appropriate parts of buffer to image planes in pFrameRGB
   avpicture_fill((AVPicture *)pFrameRGB, buffer+headerlen, PIX_FMT_RGB24,
	pCodecCtx->width, pCodecCtx->height);

   // Read frames and save first five frames to disk
   i=0;

   int64_t dur = pFormatCtx->streams[videoStream]->duration;
   int64_t dur2 = pFormatCtx->duration;
   if(dur < 0) dur = dur2;

	cb->Width = pCodecCtx->width;
	cb->Height =  pCodecCtx->height;

    AVRational ss =pFormatCtx->streams[videoStream]->time_base;
    int64_t step = dur/numOfFrames;
	 CString result;//AVSEEK_FLAG_FRAME
	 int64_t sec=dur/ss.den;
	 int64_t min = sec/60;
	 sec = sec%60;
	 int64_t hr = min/60;
	 min=min%60;
	 //QString s;
	 AVFormatContext *ic = pFormatCtx;
	 int64_t full_sec =0; //����� �����, � ��������

	 if (ic->duration != AV_NOPTS_VALUE) {
		 int hours, mins, secs, us;
		 secs = ic->duration / AV_TIME_BASE;
		 full_sec = secs;
		 us = ic->duration % AV_TIME_BASE;
		 mins = secs / 60;
		 secs %= 60;
		 hours = mins / 60;
		 mins %= 60;
		 CString s;
		 s.Format(_T("%02d:%02d:%02d.%02d"), hours, mins, secs,
			 (100 * us) / AV_TIME_BASE);
	 }


	 step = full_sec / numOfFrames;
	

	 AVDictionaryEntry  *tag = NULL;
	while ((tag=av_dict_get (pFormatCtx->streams[videoStream]->metadata,"",tag,AV_DICT_IGNORE_SUFFIX ))) {
	}

	int start_time = 3000;	
	int64_t timestamp;
	timestamp = start_time;
	struct SwsContext *img_convert_ctx=NULL;
	int64_t my_start_time;
	int64_t seek_target;

	 my_start_time = (double)ic->duration / numOfFrames*1.6;
	
	 AVRational rat = {1, AV_TIME_BASE};
	 my_start_time = /*av_rescale_q(my_start_time, rat, pFormatCtx->streams[videoStream]->time_base);*/0;

	 if ( seekByBytes ){
		 uint64_t size=  avio_size(pFormatCtx->pb);
		 seek_target = (double)my_start_time / ic->duration * size;
	 }
	 avformat_seek_file(pFormatCtx, videoStream, 0, my_start_time, seek_target, seekByBytes?AVSEEK_FLAG_BYTE:0) ;
	 avcodec_flush_buffers(pCodecCtx) ;

	 avcodec_flush_buffers(pCodecCtx) ;

	 while(av_read_frame(pFormatCtx, &packet)>=0) {
		 if(packet.stream_index==videoStream) {

			 frameFinished = false;

			 if ( avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet) < 0 ) {

			 }

			 if(frameFinished) {
				 break;
			 }
		 }

		 // Free the packet that was allocated by av_read_frame
		 av_free_packet(&packet);
		 // memset( &packet, 0, sizeof( packet ) );
	 }

	 for ( int i = 0; i < numOfFrames; i++ ) {

		 avpicture_fill((AVPicture *)pFrameRGB, buffer+headerlen, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
		 if(NeedStop) {
			 break;
		 }
		 int64_t target_frame;

		 target_frame = (double)ic->duration / numOfFrames* (i+0.5);
		 //(double)ic->duration / (double)AV_TIME_BASE / (double)numOfFrames /*step*/ * /*AV_TIME_BASE*/(double)ss.den/(double)ss.num * (i+0.5);


		 int64_t display_time= (double)ic->duration / (double)AV_TIME_BASE / (double)numOfFrames* (i+0.5);
		 if(target_frame==0) target_frame = 1;

		 SeekToKeyFrame = true; 

		 int64_t seek_target = target_frame;
		 AVRational rat = {1, AV_TIME_BASE};
		 seek_target = av_rescale_q(seek_target, rat, pFormatCtx->streams[videoStream]->time_base);

		 if ( seekByBytes ) {
			 uint64_t size=  avio_size(pFormatCtx->pb);
			 seek_target = (double)target_frame / ic->duration * size;
		 }
		 int64_t seek_min= 0;
		 int64_t seek_max= /*INT64_MAX*/seek_target;

		 if ( avformat_seek_file(pFormatCtx, videoStream, seek_min, seek_max, seek_target, seekByBytes?AVSEEK_FLAG_BYTE:0) < 0  )  {

		 }

		 avcodec_flush_buffers(pCodecCtx) ;

		 while (av_read_frame(pFormatCtx, &packet)>=0) {
			 // Is this a packet from the video stream?
			 if(packet.stream_index==videoStream) {
				 frameFinished = false;
			
				 if ( avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet) < 0 ) {
					 
				 }

				 // Did we get a video frame?
				 if(frameFinished) {

					 // Convert the image into YUV format that SDL uses
					 if(img_convert_ctx == NULL)
					 {
						 int w = pCodecCtx->width;
						 int h = pCodecCtx->height;

						 img_convert_ctx = sws_getContext(w, h, pCodecCtx->pix_fmt, w, h, PIX_FMT_BGR24, SWS_BICUBIC,
							 NULL, NULL, NULL);
						 if(img_convert_ctx == NULL) {
							 fprintf(stderr, "Cannot initialize the conversion context!\n");
							 exit(1);
						 }
					 }
					 int ret = sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
					 int64_t pts = pFrame->pts;
					 // Save the frame to disk
					 if ( i < numOfFrames ) {
						 SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i, headerlen,timestamp_to_str(target_frame,ss.den),cb, display_time);
					 }
					 break;
				 }
				 if(NeedStop) {
					 i = numOfFrames;
				 }
			 }

			 // Free the packet that was allocated by av_read_frame
			 av_free_packet(&packet);
			 memset( &packet, 0, sizeof( packet ) );
		 }
		 SendMessage(progressBar, PBM_SETPOS, (i + 1)*10, 0);
	 }
finish:
	 // Free the RGB image
	 free(buffer);
	 av_free(pFrameRGB);
	 if ( img_convert_ctx ) {
		 sws_freeContext(img_convert_ctx);
	 }

	 // Free the YUV frame
	 av_free(pFrame);

	 // Close the codec
	 avcodec_close(pCodecCtx);

	 // Close the video file
	 av_close_input_file(pFormatCtx);
	 return true;
}





