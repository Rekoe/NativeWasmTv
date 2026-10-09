/* LGPL-2.1-or-later. Per-frame YUV matrix/range and HDR transfer uniforms. */
#ifndef NTV_HDR_RENDER_H
#define NTV_HDR_RENDER_H
static void ntv_hdr_render_uniforms(IJK_GLES2_Renderer *renderer,SDL_VoutOverlay *overlay)
{
    float kr = 0.2126f, kb = 0.0722f;
    if (overlay->ntv_colorspace == 9) { kr = 0.2627f; kb = 0.0593f; }
    else if (overlay->ntv_colorspace == 5 || overlay->ntv_colorspace == 6) { kr = 0.299f; kb = 0.114f; }
    int ten = overlay->format == SDL_FCC_I444P10LE;
    float maximum = ten ? 1023.f : 255.f;
    float y = overlay->ntv_full_range ? 1.f : maximum / (ten ? 876.f : 219.f);
    float uv = overlay->ntv_full_range ? 1.f : maximum / (ten ? 896.f : 224.f);
    float kg = 1.f - kr - kb;
    GLfloat matrix[9] = {y,y,y, 0,-2.f*kb*(1.f-kb)/kg*uv,2.f*(1.f-kb)*uv,
            2.f*(1.f-kr)*uv,-2.f*kr*(1.f-kr)/kg*uv,0};
    glUniformMatrix3fv(renderer->um3_color_conversion,1,GL_FALSE,matrix);
    glUniform3f(renderer->ntv_yuv_offset,overlay->ntv_full_range ? 0.f : (ten ? 64.f : 16.f)/maximum,
            (ten ? 512.f : 128.f)/maximum,(ten ? 512.f : 128.f)/maximum);
    glUniform1f(renderer->ntv_transfer,overlay->ntv_transfer == 16 ? 6.f : overlay->ntv_transfer == 18 ? 7.f : 0.f);
    glUniform1f(renderer->ntv_peak,overlay->ntv_peak > 0 ? overlay->ntv_peak : 1000.f);
}
#endif
