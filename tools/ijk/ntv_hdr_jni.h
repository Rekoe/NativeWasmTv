/* LGPL-2.1-or-later. Java SurfaceTexture bridge owned by the codec instance. */
#ifndef NTV_HDR_JNI_H
#define NTV_HDR_JNI_H
static jclass ntv_hdr_class;
static jmethodID ntv_hdr_create, ntv_hdr_surface, ntv_hdr_release, ntv_hdr_output, ntv_hdr_verify;
static jmethodID ntv_hdr_prepare;
void ntv_hdr_jni_init(JNIEnv *env)
{
    if (J4A_GetSystemAndroidApiLevel(env) < 17) return;
    jclass local = (*env)->FindClass(env,"xiao/bu/tv/NativeHdrOutput");
    if (J4A_ExceptionCheck__catchAll(env) || !local) return;
    ntv_hdr_class = (*env)->NewGlobalRef(env,local);
    (*env)->DeleteLocalRef(env,local);
    ntv_hdr_create = (*env)->GetStaticMethodID(env,ntv_hdr_class,"create",
            "(Landroid/media/MediaFormat;Landroid/view/Surface;)Lxiao/bu/tv/NativeHdrOutput;");
    ntv_hdr_surface = (*env)->GetMethodID(env,ntv_hdr_class,"getSurface","()Landroid/view/Surface;");
    ntv_hdr_release = (*env)->GetMethodID(env,ntv_hdr_class,"release","()V");
    ntv_hdr_output = (*env)->GetMethodID(env,ntv_hdr_class,"outputFormat","(Landroid/media/MediaFormat;)V");
    ntv_hdr_verify = (*env)->GetStaticMethodID(env,ntv_hdr_class,"verifyToneMapping",
            "(Landroid/media/MediaCodec;Landroid/media/MediaFormat;)V");
    ntv_hdr_prepare = (*env)->GetStaticMethodID(env,ntv_hdr_class,"prepareSoftwareOutput","()V");
    if (J4A_ExceptionCheck__catchAll(env)) {
        (*env)->DeleteGlobalRef(env,ntv_hdr_class); ntv_hdr_class = NULL;
    }
}
int ntv_hdr_prepare_software(void)
{
    if (!ntv_hdr_class) return 0;
    JNIEnv *env = NULL;
    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) return -1;
    (*env)->CallStaticVoidMethod(env,ntv_hdr_class,ntv_hdr_prepare);
    return J4A_ExceptionCheck__catchAll(env) ? -1 : 0;
}
static void ntv_hdr_close(JNIEnv *env,jobject *bridge)
{
    if (*bridge) {
        (*env)->CallVoidMethod(env,*bridge,ntv_hdr_release);
        J4A_ExceptionCheck__catchAll(env);
        SDL_JNI_DeleteGlobalRefP(env,bridge);
    }
}
#endif
