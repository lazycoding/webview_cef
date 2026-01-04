#include "include/webview_cef/webview_cef_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <cstring>
#include <unordered_map>
#include <webview_plugin.h>
#include "webview_cef_keyevent.h"
#include "webview_cef_texture.h"
#include "webview_cef_texture_renderer.h"
#include "gtk_file_dialog_open.h"

#define WEBVIEW_CEF_PLUGIN(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), webview_cef_plugin_get_type(), WebviewCefPlugin))

struct _WebviewCefPlugin {
    GObject parent_instance;
    std::shared_ptr<webview_cef::WebviewPlugin> m_plugin;
    FlTextureRegistrar* m_textureRegister;
    FlView* m_flViewPtr;
};

G_DEFINE_TYPE(WebviewCefPlugin, webview_cef_plugin, g_object_get_type())

struct FlViewPtrCompare {
    bool operator()(FlView* a, FlView* b) const {
        return reinterpret_cast<uintptr_t>(a) == reinterpret_cast<uintptr_t>(b);
    }
};

std::unordered_map<
        FlView*,
        std::shared_ptr<webview_cef::WebviewPlugin>,
        std::hash<FlView*>,
        FlViewPtrCompare>
        webviewPlugins;

static FlValue* encode_wavlue_to_flvalue(WValue* args) {
    WValueType type = webview_value_get_type(args);
    switch (type) {
        case Webview_Value_Type_Bool:
            return fl_value_new_bool(webview_value_get_bool(args));
        case Webview_Value_Type_Int:
            return fl_value_new_int(webview_value_get_int(args));
        case Webview_Value_Type_Float:
            return fl_value_new_float(webview_value_get_float(args));
        case Webview_Value_Type_Double:
            return fl_value_new_float(webview_value_get_double(args));
        case Webview_Value_Type_String:
            return fl_value_new_string(webview_value_get_string(args));
        case Webview_Value_Type_Uint8_List: {
            size_t len = webview_value_get_len(args);
            const uint8_t* val = webview_value_get_uint8_list(args);
            return fl_value_new_uint8_list(val, len);
        }
        case Webview_Value_Type_Int32_List: {
            size_t len = webview_value_get_len(args);
            const int32_t* val = webview_value_get_int32_list(args);
            return fl_value_new_int32_list(val, len);
        }
        case Webview_Value_Type_Int64_List: {
            size_t len = webview_value_get_len(args);
            const int64_t* val = webview_value_get_int64_list(args);
            return fl_value_new_int64_list(val, len);
        }
        case Webview_Value_Type_Float_List: {
            size_t len = webview_value_get_len(args);
            const float* val = webview_value_get_float_list(args);
            return fl_value_new_float32_list(val, len);
        }
        case Webview_Value_Type_Double_List: {
            size_t len = webview_value_get_len(args);
            const double* val = webview_value_get_double_list(args);
            return fl_value_new_float_list(val, len);
        }
        case Webview_Value_Type_List: {
            FlValue* ret = fl_value_new_list();
            size_t len = webview_value_get_len(args);
            for (size_t i = 0; i < len; i++) {
                FlValue* val = encode_wavlue_to_flvalue(webview_value_get_list_value(args, i));
                fl_value_append(ret, val);
                fl_value_unref(val);
            }
            return ret;
        }
        case Webview_Value_Type_Map: {
            FlValue* ret = fl_value_new_map();
            size_t len = webview_value_get_len(args);
            for (size_t i = 0; i < len; i++) {
                FlValue* key = encode_wavlue_to_flvalue(webview_value_get_key(args, i));
                FlValue* val = encode_wavlue_to_flvalue(webview_value_get_value(args, i));
                fl_value_set(ret, key, val);
                fl_value_unref(key);
                fl_value_unref(val);
            }
            return ret;
        }
        default:
            return fl_value_new_null();
    }
}

static WValue* encode_flvalue_to_wvalue(FlValue* args) {
    FlValueType argsType = fl_value_get_type(args);
    switch (argsType) {
        case FL_VALUE_TYPE_BOOL:
            return webview_value_new_bool(fl_value_get_bool(args));
        case FL_VALUE_TYPE_INT:
            return webview_value_new_int(fl_value_get_int(args));
        case FL_VALUE_TYPE_FLOAT:
            return webview_value_new_double(fl_value_get_float(args));
        case FL_VALUE_TYPE_STRING:
            return webview_value_new_string(fl_value_get_string(args));
        case FL_VALUE_TYPE_UINT8_LIST: {
            size_t len = fl_value_get_length(args);
            const uint8_t* val = fl_value_get_uint8_list(args);
            return webview_value_new_uint8_list(val, len);
        }
        case FL_VALUE_TYPE_INT32_LIST: {
            size_t len = fl_value_get_length(args);
            const int32_t* val = fl_value_get_int32_list(args);
            return webview_value_new_int32_list(val, len);
        }
        case FL_VALUE_TYPE_INT64_LIST: {
            size_t len = fl_value_get_length(args);
            const int64_t* val = fl_value_get_int64_list(args);
            return webview_value_new_int64_list(val, len);
        }
        case FL_VALUE_TYPE_FLOAT32_LIST: {
            size_t len = fl_value_get_length(args);
            const float* val = fl_value_get_float32_list(args);
            return webview_value_new_float_list(val, len);
        }
        case FL_VALUE_TYPE_FLOAT_LIST: {
            size_t len = fl_value_get_length(args);
            const double* val = fl_value_get_float_list(args);
            return webview_value_new_double_list(val, len);
        }
        case FL_VALUE_TYPE_LIST: {
            WValue* ret = webview_value_new_list();
            size_t len = fl_value_get_length(args);
            for (size_t i = 0; i < len; i++) {
                WValue* item = encode_flvalue_to_wvalue(fl_value_get_list_value(args, i));
                webview_value_append(ret, item);
                webview_value_unref(item);
            }
            return ret;
        }
        case FL_VALUE_TYPE_MAP: {
            WValue* ret = webview_value_new_map();
            size_t len = fl_value_get_length(args);
            for (size_t i = 0; i < len; i++) {
                WValue* key = encode_flvalue_to_wvalue(fl_value_get_map_key(args, i));
                WValue* val = encode_flvalue_to_wvalue(fl_value_get_map_value(args, i));
                webview_value_set(ret, key, val);
                webview_value_unref(key);
                webview_value_unref(val);
            }
            return ret;
        }
        default:
            return webview_value_new_null();
    }
}

// Called when a method call is received from Flutter.
static void webview_cef_plugin_handle_method_call(
        WebviewCefPlugin* self,
        FlMethodCall* method_call) {
    const gchar* method = fl_method_call_get_name(method_call);
    WValue* encodeArgs = encode_flvalue_to_wvalue(fl_method_call_get_args(method_call));
    g_object_ref(method_call);
    self->m_plugin->HandleMethodCall(method, encodeArgs, [=](int ret, WValue* responseArgs) {
        if (ret > 0) {
            fl_method_call_respond_success(
                    method_call, encode_wavlue_to_flvalue(responseArgs), nullptr);
        } else if (ret < 0) {
            fl_method_call_respond_error(
                    method_call, "error", "error", encode_wavlue_to_flvalue(responseArgs), nullptr);
        } else {
            fl_method_call_respond_not_implemented(method_call, nullptr);
        }
        g_object_unref(method_call);
    });
    webview_value_unref(encodeArgs);
}

static void webview_cef_plugin_dispose(GObject* object) {
    webviewPlugins.erase(WEBVIEW_CEF_PLUGIN(object)->m_flViewPtr);
    WEBVIEW_CEF_PLUGIN(object)->m_plugin = nullptr;
    if (webviewPlugins.empty()) {
        webview_cef::stopCEF();
    }
    G_OBJECT_CLASS(webview_cef_plugin_parent_class)->dispose(object);
}

static void webview_cef_plugin_class_init(WebviewCefPluginClass* klass) {
    G_OBJECT_CLASS(klass)->dispose = webview_cef_plugin_dispose;
}

static void webview_cef_plugin_init(WebviewCefPlugin* self) {
    self->m_plugin = std::make_shared<webview_cef::WebviewPlugin>();
}

static void method_call_cb(
        FlMethodChannel* channel,
        FlMethodCall* method_call,
        gpointer user_data) {
    WebviewCefPlugin* plugin = WEBVIEW_CEF_PLUGIN(user_data);
    webview_cef_plugin_handle_method_call(plugin, method_call);
}

void webview_cef_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
    WebviewCefPlugin* plugin =
            WEBVIEW_CEF_PLUGIN(g_object_new(webview_cef_plugin_get_type(), nullptr));

    plugin->m_flViewPtr = fl_plugin_registrar_get_view(registrar);
    webviewPlugins[plugin->m_flViewPtr] = plugin->m_plugin;  // 可能存在相同的key

    plugin->m_textureRegister = fl_plugin_registrar_get_texture_registrar(registrar);

    g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
    g_autoptr(FlMethodChannel) channel = fl_method_channel_new(
            fl_plugin_registrar_get_messenger(registrar), "webview_cef", FL_METHOD_CODEC(codec));
    fl_method_channel_set_method_call_handler(
            channel, method_call_cb, g_object_ref(plugin), g_object_unref);

    plugin->m_plugin->setInvokeMethodFunc([=](const std::string& method, WValue* arguments) {
        FlValue* args = encode_wavlue_to_flvalue(arguments);
        fl_method_channel_invoke_method(channel, method.c_str(), args, NULL, NULL, NULL);
        fl_value_unref(args);
    });

    plugin->m_plugin->setCreateTextureFunc([=]() {
        std::shared_ptr<WebviewTextureRenderer> renderer =
                std::make_shared<WebviewTextureRenderer>(plugin->m_textureRegister);
        return std::dynamic_pointer_cast<webview_cef::WebviewTexture>(renderer);
    });

    plugin->m_plugin->setHandleDialogFunc([=](CefRefPtr<CefBrowser> browser,
                                              CefDialogHandler::FileDialogMode mode,
                                              const CefString& title,
                                              const CefString& default_file_path,
                                              const std::vector<CefString>& accept_filters,
                                              FileDialogCallback callback) {
        DialogCallbackContext* context = new DialogCallbackContext();
        auto parent = gtk_widget_get_toplevel(GTK_WIDGET(plugin->m_flViewPtr));
        context->parent = parent;
        context->callback = callback;
        context->mode = mode;
        context->title = title.ToString();
        context->default_file_path = default_file_path.ToString();
        context->accept_filters.reserve(accept_filters.size());
        for (const auto& filter : accept_filters) {
            context->accept_filters.push_back(filter.ToString());
        }

        gdk_threads_add_idle(openDialog, context);
        return true;
    });

    g_object_unref(plugin);
}

void initCEFProcesses(int argc, char** argv) {
    webview_cef::initCEFProcesses(argc, argv);
}

gboolean processKeyEventForCEF(GtkWidget* widget, GdkEventKey* event, gpointer data) {
    FlView* flView = reinterpret_cast<FlView*>(widget);
    auto pluginIterator = webviewPlugins.find(flView);

    if (pluginIterator == webviewPlugins.end()) {
        return FALSE;
    }
    auto plugin = pluginIterator->second;
    if (!plugin->getAnyBrowserFocused()) {
        return FALSE;
    }

    CefKeyEvent key_event;
    KeyboardCode windows_key_code = GdkEventToWindowsKeyCode(event);
    key_event.windows_key_code = GetWindowsKeyCodeWithoutLocation(windows_key_code);
    key_event.native_key_code = event->hardware_keycode;

    key_event.modifiers = GetCefStateModifiers(event->state);
    if (event->keyval >= GDK_KP_Space && event->keyval <= GDK_KP_9) {
        key_event.modifiers |= EVENTFLAG_IS_KEY_PAD;
    }
    if (key_event.modifiers & EVENTFLAG_ALT_DOWN) {
        key_event.is_system_key = true;
    }

    if (windows_key_code == VKEY_RETURN) {
        // We need to treat the enter key as a key press of character \r.  This
        // is apparently just how webkit handles it and what it expects.
        key_event.unmodified_character = '\r';
    } else if (
            (windows_key_code == KeyboardCode::VKEY_V) &&
            (key_event.modifiers & EVENTFLAG_CONTROL_DOWN) && (event->type == GDK_KEY_PRESS)) {
        // try to fix copy request freeze process problem(flutter engine will send a copy
        // request when ctrl+v pressed)
        int res = 0;
        if (system("xclip -o -sel clipboard -t $(xclip -o -t TARGETS) | xclip -i -sel "
                   "clipboard -t $(xclip -o -t TARGETS) &>/dev/null") == 0) {
            res = system("xclip -o -sel clipboard | xclip -i &>/dev/null");
        }
    } else {
        // FIXME: fix for non BMP chars
        key_event.unmodified_character = static_cast<int>(gdk_keyval_to_unicode(event->keyval));
    }

    // If ctrl key is pressed down, then control character shall be input.
    if (key_event.modifiers & EVENTFLAG_CONTROL_DOWN) {
        key_event.character =
                GetControlCharacter(windows_key_code, key_event.modifiers & EVENTFLAG_SHIFT_DOWN);
    } else {
        key_event.character = key_event.unmodified_character;
    }
    if (event->type == GDK_KEY_PRESS) {
        key_event.type = KEYEVENT_RAWKEYDOWN;
        plugin->sendKeyEvent(key_event);
        key_event.type = KEYEVENT_CHAR;
    } else {
        key_event.type = KEYEVENT_KEYUP;
    }
    plugin->sendKeyEvent(key_event);

    return TRUE;
}
