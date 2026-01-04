#include "webview_cef_texture_renderer.h"
#include "webview_cef_texture.h"
WebviewTextureRenderer::WebviewTextureRenderer(FlTextureRegistrar* texture_register) :
        register_(texture_register) {
    texture_ = webview_cef_texture_new();
    fl_texture_registrar_register_texture(register_, FL_TEXTURE(texture_));
    textureId = (int64_t)texture_;
}

WebviewTextureRenderer::~WebviewTextureRenderer() {
    fl_texture_registrar_unregister_texture(register_, FL_TEXTURE(texture_));
    register_ = nullptr;
    if (texture_ != NULL) {
        g_object_unref(texture_);
        texture_ = NULL;
    }
}

void WebviewTextureRenderer::onFrame(const void* buffer, int32_t width, int32_t height) {
    if (width == 0 || height == 0) {
        printf("WebviewTextureRenderer onFrame size: w=%d,h=%d", width, height);
        return;
    }

    webview_cef_texture_set_buffer(texture_, buffer, width, height);
    fl_texture_registrar_mark_texture_frame_available(register_, FL_TEXTURE(texture_));
}