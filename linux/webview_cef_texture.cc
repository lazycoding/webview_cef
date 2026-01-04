#include "webview_cef_texture.h"
#include "webview_plugin.h"
struct _WebviewCefTexture {
    FlPixelBufferTexture parent_instance;
    uint8_t* buffer;
    uint32_t width;
    uint32_t height;
};

G_DEFINE_TYPE(WebviewCefTexture, webview_cef_texture, fl_pixel_buffer_texture_get_type())

static gboolean webview_cef_texture_copy_pixels(
        FlPixelBufferTexture* texture,
        const uint8_t** out_buffer,
        uint32_t* width,
        uint32_t* height,
        GError** error) {
    WebviewCefTexture* self = WEBVIEW_CEF_TEXTURE(texture);

    if (self->buffer == nullptr) {
        return FALSE;
    }

    *out_buffer = self->buffer;
    *width = self->width;
    *height = self->height;
    return TRUE;
}

static void webview_cef_texture_dispose(GObject* object) {
    WebviewCefTexture* self = WEBVIEW_CEF_TEXTURE(object);

    if (self->buffer != nullptr) {
        free(self->buffer);
        self->buffer = nullptr;
    }

    G_OBJECT_CLASS(webview_cef_texture_parent_class)->dispose(object);
}

static void webview_cef_texture_class_init(WebviewCefTextureClass* klass) {
    G_OBJECT_CLASS(klass)->dispose = webview_cef_texture_dispose;
    FL_PIXEL_BUFFER_TEXTURE_CLASS(klass)->copy_pixels = webview_cef_texture_copy_pixels;
}

static void webview_cef_texture_init(WebviewCefTexture* self) {
    self->buffer = nullptr;
    self->width = 0;
    self->height = 0;
}

WebviewCefTexture* webview_cef_texture_new() {
    return WEBVIEW_CEF_TEXTURE(g_object_new(webview_cef_texture_get_type(), nullptr));
}

void webview_cef_texture_set_buffer(
        WebviewCefTexture* self,
        const void* buffer,
        uint32_t width,
        uint32_t height) {
    g_return_if_fail(WEBVIEW_CEF_IS_TEXTURE(self));
    if (self->width != (uint32_t)width || self->height != (uint32_t)height) {
        delete[] self->buffer;
        self->width = width;
        self->height = height;
        const auto size = (uint32_t)width * (uint32_t)height * 4;
        self->buffer = new uint8_t[size];
    }

    webview_cef::SwapBufferFromBgraToRgba((void*)self->buffer, buffer, width, height);
}