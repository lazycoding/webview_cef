#ifndef WEBVIEW_CEF_TEXTURE_RENDERER_H
#define WEBVIEW_CEF_TEXTURE_RENDERER_H

#include <flutter_linux/flutter_linux.h>
#include "webview_cef_texture.h"
#include <webview_plugin.h>

class WebviewTextureRenderer : public webview_cef::WebviewTexture {
public:
    WebviewTextureRenderer(FlTextureRegistrar* texture_register);
    virtual ~WebviewTextureRenderer();

    virtual void onFrame(const void* buffer, int32_t width, int32_t height) override;

private:
    FlTextureRegistrar* register_;
    WebviewCefTexture* texture_;
};

#endif  // WEBVIEW_CEF_TEXTURE_RENDERER_H