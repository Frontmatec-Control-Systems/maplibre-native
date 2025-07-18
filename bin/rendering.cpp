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
#include <string>
#include <chrono>

// Platform-specific export macro
#ifdef _WIN32
    #define MAPLIBRE_EXPORT __declspec(dllexport)
#else
    #define MAPLIBRE_EXPORT __attribute__((visibility("default")))
#endif

// Configuration constants
static const int CACHE_TIMEOUT_SECONDS = 20;

// Renderer handle with timeout-based caching
struct RendererHandle {
    std::unique_ptr<mbgl::util::RunLoop> runloop;
    std::unique_ptr<mbgl::HeadlessFrontend> frontend;
    std::unique_ptr<mbgl::Map> map;
    std::string styleJson;
    std::vector<uint8_t> lastRenderedPng;
    uint32_t width;
    uint32_t height;
    double pixelRatio;
    bool isValid;
    std::chrono::steady_clock::time_point lastUsed;
    
    RendererHandle() : width(0), height(0), pixelRatio(1.0), isValid(false), 
                       lastUsed(std::chrono::steady_clock::now()) {}
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
        auto handle = std::make_unique<RendererHandle>();
        
        // Store parameters
        handle->styleJson = style_json;
        handle->width = static_cast<uint32_t>(width);
        handle->height = static_cast<uint32_t>(height);
        handle->pixelRatio = pixel_ratio;
        handle->isValid = true;
        
        // No MapLibre objects created yet - will be created on first render
        
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
    
    if (!handle || !handle->isValid) {
        return false;
    }
    
    try {
        using namespace mbgl;
        
        // Clear previous result
        handle->lastRenderedPng.clear();
        
        auto now = std::chrono::steady_clock::now();
        
        // Check if objects need to be recreated (don't exist or older than 20 seconds)
        bool needsRecreate = false;
        
        if (!handle->runloop || !handle->frontend || !handle->map) {
            needsRecreate = true;
        } else {
            auto timeSinceLastUse = std::chrono::duration_cast<std::chrono::seconds>(now - handle->lastUsed).count();
            if (timeSinceLastUse > CACHE_TIMEOUT_SECONDS) {
                needsRecreate = true;
            }
        }
        
        if (needsRecreate) {
            // Clean up existing objects
            handle->map.reset();
            handle->frontend.reset();
            handle->runloop.reset();
            
            // Create fresh objects
            handle->runloop = std::make_unique<mbgl::util::RunLoop>();
            
            handle->frontend = std::make_unique<HeadlessFrontend>(
                Size{handle->width, handle->height}, 
                static_cast<float>(handle->pixelRatio)
            );
            
            auto mapTilerConfiguration = TileServerOptions::MapTilerConfiguration();
            
            handle->map = std::make_unique<Map>(
                *handle->frontend,
                MapObserver::nullObserver(),
                MapOptions()
                    .withMapMode(MapMode::Tile)
                    .withSize(handle->frontend->getSize())
                    .withPixelRatio(static_cast<float>(handle->pixelRatio)),
                ResourceOptions()
                    .withTileServerOptions(mapTilerConfiguration)
            );
            
            // Load style
            handle->map->getStyle().loadJSON(handle->styleJson);
        }
        
        // Update last used time
        handle->lastUsed = now;
        
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
        handle->lastRenderedPng.clear();
        handle->map.reset();
        handle->frontend.reset();
        handle->runloop.reset();
        handle->isValid = false;
        
        delete handle;
    }
}

} // extern "C"