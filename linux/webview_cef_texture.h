#ifndef WEBVIEW_CEF_TEXTURE_H_
#define WEBVIEW_CEF_TEXTURE_H_
#include <flutter_linux/flutter_linux.h>
G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(
        WebviewCefTexture,
        webview_cef_texture,
        WEBVIEW_CEF,
        TEXTURE,
        FlPixelBufferTexture);

WebviewCefTexture* webview_cef_texture_new();

void webview_cef_texture_set_buffer(
        WebviewCefTexture* self,
        const void* buffer,
        uint32_t width,
        uint32_t height);

G_END_DECLS
#endif  // WEBVIEW_CEF_TEXTURE_H_
