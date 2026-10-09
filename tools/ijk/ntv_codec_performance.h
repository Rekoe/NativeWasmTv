/* LGPL-2.1-or-later. Optional API 23+ hint; configuration retries without it. */
#ifndef NTV_CODEC_PERFORMANCE_H
#define NTV_CODEC_PERFORMANCE_H
#include "ijksdl/android/ijksdl_codec_android_mediaformat_java.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include "j4a/j4a_base.h"
static jclass ntv_codec_performance_class;
static jmethodID ntv_codec_performance_configure;

void ntv_codec_jni_init(JNIEnv *env)
{
    if (J4A_GetSystemAndroidApiLevel(env) < 23) return;
    jclass local = (*env)->FindClass(env, "xiao/bu/tv/IjkCodecPerformance");
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
    if (!local) return;
    ntv_codec_performance_class = (*env)->NewGlobalRef(env, local);
    (*env)->DeleteLocalRef(env, local);
    ntv_codec_performance_configure = (*env)->GetStaticMethodID(env, ntv_codec_performance_class,
            "configure", "(Landroid/media/MediaFormat;Ljava/lang/String;Ljava/lang/String;IIF)F");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        (*env)->DeleteGlobalRef(env, ntv_codec_performance_class);
        ntv_codec_performance_class = NULL;
    }
}

static float ntv_codec_hint(JNIEnv *env, SDL_AMediaFormat *format,
        const char *name, const char *mime, int width, int height, float fps)
{
    if (!ntv_codec_performance_class || fps < 45) return 0;
    jstring jname = (*env)->NewStringUTF(env, name);
    jstring jmime = (*env)->NewStringUTF(env, mime);
    float result = 0;
    if (jname && jmime && !(*env)->ExceptionCheck(env))
        result = (*env)->CallStaticFloatMethod(env, ntv_codec_performance_class,
                ntv_codec_performance_configure, SDL_AMediaFormatJava_getObject(env, format),
                jname, jmime, width, height, fps);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); result = 0; }
    if (jname) (*env)->DeleteLocalRef(env, jname);
    if (jmime) (*env)->DeleteLocalRef(env, jmime);
    return result;
}
#endif
