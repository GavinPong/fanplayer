// 包含头文件
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// for jni
JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);

// 内部常量定义
#define DEF_WIN_PIX_FMT   WINDOW_FORMAT_RGBX_8888

// 内部类型定义
typedef struct {
    // common members
    VDEV_COMMON_MEMBERS

    ANativeWindow *wincur;
    ANativeWindow *winnew;
} VDEVCTXT;

// 内部函数实现
inline int android_pixfmt_to_ffmpeg_pixfmt(int srcfmt)
{
    // dst fmt
    int dst_fmt = 0;
    switch (srcfmt) {
    case WINDOW_FORMAT_RGB_565:   dst_fmt = AV_PIX_FMT_RGB565; break;
    case WINDOW_FORMAT_RGBX_8888: dst_fmt = AV_PIX_FMT_BGR32;  break;
    }
    return dst_fmt;
}

static void vdev_android_lock(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;

    if (c->wincur != c->winnew) {
        if (c->wincur) ANativeWindow_release(c->wincur);
        if (c->winnew) ANativeWindow_setBuffersGeometry(c->winnew, c->sw, c->sh, DEF_WIN_PIX_FMT);
        c->wincur = c->winnew;
    }

    if (c->wincur) {
        ANativeWindow_Buffer winbuf;
        ANativeWindow_lock(c->wincur, &winbuf, NULL);
        if (buffer  ) buffer  [0] = (uint8_t*)winbuf.bits;
        if (linesize) linesize[0] = winbuf.stride * 4;
    }
}

static void vdev_android_unlock(void *ctxt, int64_t pts)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    if (c->wincur) ANativeWindow_unlockAndPost(c->wincur);
    c->cmninfos->vpts = pts;
    vdev_avsync_and_complete(c);
}

static void vdev_android_destroy(void *ctxt)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    if (c->wincur) ANativeWindow_release(c->wincur);
    free(ctxt);
}

// 接口函数实现
void* vdev_android_create(void *surface, int bufnum, int w, int h)
{
    int ret, i;
    VDEVCTXT *ctxt = (VDEVCTXT*)calloc(1, sizeof(VDEVCTXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate vdev context !\n");
        exit(0);
    }

    // init vdev context
    ctxt->pixfmt    = android_pixfmt_to_ffmpeg_pixfmt(DEF_WIN_PIX_FMT);
    ctxt->surface   = surface;
    ctxt->w         = w > 1 ? w : 1;
    ctxt->h         = h > 1 ? h : 1;
    ctxt->sw        = w > 1 ? w : 1;
    ctxt->sh        = h > 1 ? h : 1;
    ctxt->lock      = vdev_android_lock;
    ctxt->unlock    = vdev_android_unlock;
    ctxt->destroy   = vdev_android_destroy;
    return ctxt;
}

void vdev_android_setwindow(void *ctxt, jobject surface)
{
    if (!ctxt) return;
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    JNIEnv *env = get_jni_env();
    c->winnew   = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
}
