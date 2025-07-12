#include <mbgl/map/map.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/util/image.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/gfx/backend.hpp>
#include <mbgl/gfx/headless_frontend.hpp>
#include <mbgl/style/style.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <cstring>
#include <mutex>
#include <vector>

// Platform-specific export macro
#ifdef _WIN32
    #define MAPLIBRE_EXPORT __declspec(dllexport)
#else
    #define MAPLIBRE_EXPORT __attribute__((visibility("default")))
#endif

// Global mutex for RunLoop initialization
static std::mutex g_init_mutex;
static bool g_runloop_initialized = false;
static std::unique_ptr<mbgl::util::RunLoop> g_runloop;

// Renderer handle structure
struct RendererHandle {
    std::unique_ptr<mbgl::HeadlessFrontend> frontend;
    std::unique_ptr<mbgl::Map> map;
    std::vector<uint8_t> lastRenderedPng;
    uint32_t width;
    uint32_t height;
    double pixelRatio;
    bool isValid;
    
    RendererHandle() : width(0), height(0), pixelRatio(1.0), isValid(false) {}
};

extern "C" {

MAPLIBRE_EXPORT RendererHandle* maplibre_create_renderer(
    const char* style_json, 
    int width, 
    int height,
    double pixel_ratio) {
    
    if (!style_json || width <= 0 || height <= 0) {
        return nullptr;
    }
    
    try {
        using namespace mbgl;
        
        // Initialize RunLoop once globally
        {
            std::lock_guard<std::mutex> lock(g_init_mutex);
            if (!g_runloop_initialized) {
                g_runloop = std::make_unique<util::RunLoop>();
                g_runloop_initialized = true;
            }
        }
        
        // Create renderer handle
        auto handle = std::make_unique<RendererHandle>();
        handle->width = static_cast<uint32_t>(width);
        handle->height = static_cast<uint32_t>(height);
        handle->pixelRatio = pixel_ratio;
        
        // Create frontend
        handle->frontend = std::make_unique<HeadlessFrontend>(
            Size{handle->width, handle->height}, 
            static_cast<float>(pixel_ratio)
        );
        
        // Configure MapTiler
        auto mapTilerConfiguration = TileServerOptions::MapTilerConfiguration();
        
        // Create map
        handle->map = std::make_unique<Map>(
            *handle->frontend,
            MapObserver::nullObserver(),
            MapOptions()
                .withMapMode(MapMode::Static)
                .withSize(handle->frontend->getSize())
                .withPixelRatio(static_cast<float>(pixel_ratio)),
            ResourceOptions()
                //.withCachePath(cache_file ? cache_file : "cache.sqlite")
                //.withAssetPath(asset_root ? asset_root : ".")
                //.withApiKey(api_key ? api_key : "")
                .withTileServerOptions(mapTilerConfiguration)
        );
        
        // Load style
        // std::string style(style_path);
        // if (style.find("://") == std::string::npos) {
        //     style = std::string("file://") + style;
        // }
        // handle->map->getStyle().loadURL(style);
        handle->map->getStyle().loadJSON(style_json);

        handle->isValid = true;
        return handle.release();
        
    } catch (const std::exception& e) {
        std::cerr << "maplibre_create_renderer error: " << e.what() << std::endl;
        return nullptr;
    } catch (...) {
        std::cerr << "maplibre_create_renderer unknown error" << std::endl;
        return nullptr;
    }
}

MAPLIBRE_EXPORT bool maplibre_render_png(
    RendererHandle* handle, 
    double lon, 
    double lat, 
    double zoom,
    double bearing,
    double pitch) {
    
    if (!handle || !handle->isValid || !handle->map || !handle->frontend) {
        return false;
    }
    
    try {
        using namespace mbgl;
        
        // Set camera position
        handle->map->jumpTo(CameraOptions()
            .withCenter(LatLng{lat, lon})
            .withZoom(zoom)
            .withBearing(bearing)
            .withPitch(pitch));
        
        // Render the map
        auto result = handle->frontend->render(*handle->map);
        
        // Encode to PNG and store in handle
        auto pngString = encodePNG(result.image);
        handle->lastRenderedPng.assign(pngString.begin(), pngString.end());
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "maplibre_render_png error: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "maplibre_render_png unknown error" << std::endl;
        return false;
    }
}

MAPLIBRE_EXPORT const uint8_t* maplibre_get_image(RendererHandle* handle, int* buffer_size) {
    if (!handle || !handle->isValid || handle->lastRenderedPng.empty()) {
        if (buffer_size) *buffer_size = 0;
        return nullptr;
    }
    
    if (buffer_size) {
        *buffer_size = static_cast<int>(handle->lastRenderedPng.size());
    }
    
    return handle->lastRenderedPng.data();
}

MAPLIBRE_EXPORT void maplibre_destroy_renderer(RendererHandle* handle) {
    if (handle) {
        // Explicit cleanup in correct order
        handle->lastRenderedPng.clear();
        handle->map.reset();
        handle->frontend.reset();
        handle->isValid = false;
        
        delete handle;
    }
}

} // extern "C"