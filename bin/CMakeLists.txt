add_executable(
    mbgl-cache
    ${PROJECT_SOURCE_DIR}/bin/cache.cpp
)

target_link_libraries(
    mbgl-cache
    PRIVATE
        MapLibreNative::Base
        MapLibreNative::Base::Extras::args
        mbgl-compiler-options
        mbgl-core
)

add_executable(
    mbgl-offline
    ${PROJECT_SOURCE_DIR}/bin/offline.cpp
)

target_link_libraries(
    mbgl-offline
    PRIVATE MapLibreNative::Base::Extras::args mbgl-compiler-options mbgl-core
)

add_executable(
    mbgl-render
    ${PROJECT_SOURCE_DIR}/bin/render.cpp
)

target_link_libraries(
    mbgl-render
    PRIVATE MapLibreNative::Base::Extras::args mbgl-compiler-options mbgl-core
)

if(WIN32)
    find_package(libuv REQUIRED)

    target_link_libraries(
        mbgl-cache PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )

    target_link_libraries(
        mbgl-offline PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )

    target_link_libraries(
        mbgl-render PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )
endif()

install(TARGETS mbgl-offline mbgl-render RUNTIME DESTINATION bin)

# FIXME: CI must have a valid token
#
# add_test(NAME mbgl-offline-tool-test COMMAND mbgl-offline -s maptiler://maps/hybrid --maxZoom=0 WORKING_DIRECTORY
# ${PROJECT_SOURCE_DIR} )
#
# add_test(NAME mbgl-render-tool-test COMMAND mbgl-render WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} )

if(MLN_WITH_OPENGL)
    target_compile_definitions(mbgl-render PRIVATE "MLN_RENDER_BACKEND_OPENGL=1")
endif()







add_library(
    maplibre_rendering SHARED
    ${PROJECT_SOURCE_DIR}/bin/rendering.cpp
)

target_link_libraries(
    maplibre_rendering
    PRIVATE
        MapLibreNative::Base::Extras::args
        mbgl-compiler-options
        mbgl-core
)

# Add the same platform-specific definitions as mbgl-render
if(MLN_WITH_OPENGL)
    target_compile_definitions(maplibre_rendering PRIVATE "MLN_RENDER_BACKEND_OPENGL=1")
endif()

# Add Windows-specific linking (update the existing WIN32 block)
if(WIN32)
    find_package(libuv REQUIRED)
    target_link_libraries(
        mbgl-cache PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )
    target_link_libraries(
        mbgl-offline PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )
    target_link_libraries(
        mbgl-render PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )
    # Add MapLibre.Render to the existing WIN32 block:
    target_link_libraries(
        maplibre_rendering PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
    )
endif()

# Update the install line to include your library
install(TARGETS mbgl-offline mbgl-render maplibre_rendering
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
