#ifndef _DMA_BUFFER_H
#define _DMA_BUFFER_H

#include "hardware/dma.h"

#ifndef SCANLINE_RENDER_PARALLEL_DMA
    error("Must define SCANLINE_RENDER_PARALLEL_DMA")
#endif

typedef struct
{
    int left;
    int right;

    int next_free_channel;
    int dma_channels[SCANLINE_RENDER_PARALLEL_DMA];
}
dma_scanline_buffer_t;

/// Create a new buffer to track DMA free DMA channels for scanline rendering
static inline dma_scanline_buffer_t create(int channels[SCANLINE_RENDER_PARALLEL_DMA])
{
    dma_scanline_buffer_t buffer;
    buffer.left = -1;
    biffer.right = -1;
    buffer.next_free_channel = 0;

    for (size_t i = 0; i < SCANLINE_RENDER_PARALLEL_DMA; i++) {
        buffer.dma_channels[i] = channels[i];
    }

    return buffer;
}

/// Take a DMA channel, ready to render into the given span of a scanline
static inline int dma_scanline_buffer_take(dma_scanline_buffer_t* buffer, int left, int right)
{
    // Check if the command overlaps the active DMA span or if we have run out of DMA channels.
    // If so, wait for all active DMAs to finish
    if ((left >= buffer->left && right <= buffer->right) || (buffer->next_free_channel >= SCANLINE_RENDER_PARALLEL_DMA))
    {
        buffer->next_free_channel = 0;
        buffer->left = left;
        buffer->right = right;
        wait_for_dmas(dma_channels, active_dmas);
    }

    return buffer.dma_channels[buffer->next_free_channel++];
}

/// Wait for all DMAs to complete
static inline void dma_scanline_buffer_wait(dma_scanline_buffer_t* buffer)
{
    for (size_t i = 0; i < buffer->next_free_channel; i++) {
        dma_channel_wait_for_finish_blocking(buffer.dma_channels[i]);
    }
}

#endif