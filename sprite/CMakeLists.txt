add_library(sprite INTERFACE)

target_sources(sprite INTERFACE
        sprite.c
        sprite.h
        sprite.S
        sprite_dma.c
        sprite_dma.h
        dma_buffer.h
        scanline_rendering.c
        scanline_rendering_mono.c
        scanline_rendering.h
)

target_include_directories(sprite INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(sprite INTERFACE pico_base_headers hardware_interp)
