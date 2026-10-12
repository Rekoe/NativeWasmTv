/* Exercise the production temporal wrapper with real AVFrames. Test-only SDL
 * pthread adapters let this run without an Android JVM, not in the APK. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libavutil/frame.h"
#include "libavutil/pixdesc.h"
#include "libavutil/mathematics.h"
#include "libavutil/error.h"
#include "ijksdl/ijksdl_thread.h"
#include "ijksdl/ijksdl_mutex.h"
SDL_mutex *SDL_CreateMutex(void) { SDL_mutex *m=calloc(1,sizeof(*m)); if(m)pthread_mutex_init(&m->id,NULL);return m; }
void SDL_DestroyMutex(SDL_mutex *m) {pthread_mutex_destroy(&m->id);free(m);}
int SDL_LockMutex(SDL_mutex *m) {return pthread_mutex_lock(&m->id);}
int SDL_UnlockMutex(SDL_mutex *m) {return pthread_mutex_unlock(&m->id);}
SDL_cond *SDL_CreateCond(void) {SDL_cond *c=calloc(1,sizeof(*c));if(c)pthread_cond_init(&c->id,NULL);return c;}
void SDL_DestroyCond(SDL_cond *c) {pthread_cond_destroy(&c->id);free(c);}
int SDL_CondWait(SDL_cond *c,SDL_mutex *m) {return pthread_cond_wait(&c->id,&m->id);}
int SDL_CondSignal(SDL_cond *c) {return pthread_cond_signal(&c->id);}
int SDL_CondBroadcast(SDL_cond *c) {return pthread_cond_broadcast(&c->id);}
static void *run(void *p) {SDL_Thread *t=p;t->retval=t->func(t->data);return NULL;}
SDL_Thread *SDL_CreateThreadEx(SDL_Thread *t,int (*fn)(void *),void *arg,const char *name) {
    t->func=fn;t->data=arg;return pthread_create(&t->id,NULL,run,t) ? NULL : t;
}
void SDL_WaitThread(SDL_Thread *t,int *result) {pthread_join(t->id,NULL);if(result)*result=t->retval;}
typedef struct {struct {int pkt_serial;} viddec; void *video_st;} VideoState;
typedef struct {VideoState *is;uint32_t ntv_video_description;} FFPlayer;
#define NTV_DESCRIPTION_FRAME (1U<<25)
static uint32_t ntv_describe_video(void *stream,void *codec,AVFrame *f) {return 0;}
/* Only these two integrations are stubbed; pixel processing, references,
 * threading, flushing and ownership run through ntv_deinterlace.h unchanged. */
static FILE *output;
static int count,expectedSerial,progressive;
static int queue_picture(FFPlayer *ffp,AVFrame *f,double pts,double duration,int64_t pos,int serial) {
    assert(!f->interlaced_frame && serial==expectedSerial);
    assert(f->color_primaries==AVCOL_PRI_BT2020 && f->color_trc==AVCOL_TRC_ARIB_STD_B67);
    assert(f->colorspace==AVCOL_SPC_BT2020_NCL && f->color_range==AVCOL_RANGE_MPEG);
    assert(fabs(pts-count*(progressive ? 0.04 : 0.02))<0.000001);
    assert(fabs(duration-(progressive ? 0.04 : 0.02))<0.000001);
    const AVPixFmtDescriptor *d=av_pix_fmt_desc_get(f->format);
    int bytes=d->comp[0].depth>8 ? 2 : 1;
    if(output)for(int p=0;p<3;++p) {
        int w=AV_CEIL_RSHIFT(f->width,p?d->log2_chroma_w:0),h=AV_CEIL_RSHIFT(f->height,p?d->log2_chroma_h:0);
        for(int y=0;y<h;++y)assert(fwrite(f->data[p]+y*f->linesize[p],1,w*bytes,output)==w*bytes);
    }
    ++count;return 0;
}
/* viddec.avctx is unused by the metadata stub. */
#define ntv_describe_video(stream, codec, f) 0U
#include "ntv_deinterlace.h"

static AVFrame *frame(int w,int h,int depth,int index,int tff,int interlaced) {
    AVFrame *f=av_frame_alloc();assert(f);
    f->format=depth==8?AV_PIX_FMT_YUV420P:AV_PIX_FMT_YUV420P10LE;
    f->width=w;f->height=h;assert(av_frame_get_buffer(f,32)==0);
    f->pts=index;f->interlaced_frame=interlaced;f->top_field_first=tff;
    f->color_primaries=AVCOL_PRI_BT2020;f->color_trc=AVCOL_TRC_ARIB_STD_B67;
    f->colorspace=AVCOL_SPC_BT2020_NCL;f->color_range=AVCOL_RANGE_MPEG;return f;
}
static int read_frame(FILE *in,AVFrame *f,int depth) {
    for(int p=0;p<3;++p) {
        int w=AV_CEIL_RSHIFT(f->width,p?1:0)*(depth==8?1:2),h=AV_CEIL_RSHIFT(f->height,p?1:0);
        for(int y=0;y<h;++y) if(fread(f->data[p]+y*f->linesize[p],1,w,in)!=w)return 0;
    }
    return 1;
}
int main(int argc,char **argv) {
    assert(argc==9);
    int w=atoi(argv[2]),h=atoi(argv[3]),depth=atoi(argv[4]),tff=atoi(argv[5]);progressive=atoi(argv[6]);
    int expected=atoi(argv[8]);FILE *in=fopen(argv[1],"rb");output=fopen(argv[7],"wb");assert(in&&output);
    VideoState is={0};FFPlayer ffp={.is=&is};NtvDeinterlace state={0};is.viddec.pkt_serial=expectedSerial=3;
    int frames=0;
    for(;;) {
        AVFrame *f=frame(w,h,depth,frames,tff,!progressive);
        if(!read_frame(in,f,depth)) {av_frame_free(&f);break;}
        int used=ntv_deinterlace_push(&ffp,&state,f,(AVRational){1,25},(AVRational){25,1});assert(used>=0);
        if(!used) {assert(progressive);queue_picture(&ffp,f,frames*.04,.04,0,expectedSerial);}
        else assert(!f->buf[0]);
        av_frame_free(&f);++frames;
    }
    assert(ntv_deinterlace_flush(&ffp,&state,(AVRational){1,25},(AVRational){25,1})==0);
    assert(count==expected && count==frames*(progressive?1:2));
    assert(!state.cur && !state.prev && !state.next);
    printf("PASS frames=%d output=%d threads=%d depth=%d tff=%d progressive=%d\n",frames,count,state.threads,depth,tff,progressive);
    ntv_deinterlace_destroy(&state);fclose(in);fclose(output);output=NULL;
    /* A seek must discard the delayed old frame and all temporal references. */
    memset(&state,0,sizeof(state));count=0;progressive=0;
    AVFrame *a=frame(64,32,8,0,1,1);assert(ntv_deinterlace_push(&ffp,&state,a,(AVRational){1,25},(AVRational){25,1})==1);av_frame_free(&a);
    ++is.viddec.pkt_serial;expectedSerial=is.viddec.pkt_serial;
    a=frame(64,32,8,0,1,1);assert(ntv_deinterlace_push(&ffp,&state,a,(AVRational){1,25},(AVRational){25,1})==1);av_frame_free(&a);
    assert(!count && state.serial==expectedSerial);
    assert(ntv_deinterlace_flush(&ffp,&state,(AVRational){1,25},(AVRational){25,1})==0 && count==2);
    ntv_deinterlace_destroy(&state);puts("PASS seek/flush/timestamps/colour metadata/ownership");return 0;
}
