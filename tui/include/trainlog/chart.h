/*
 * Trainlog chart interface.
 *
 * Declares the module boundary and ownership contract; implementation and persistence remain in their owning modules.
 */
#ifndef TRAINLOG_CHART_H
#define TRAINLOG_CHART_H

/**
 * @file chart.h
 * @brief Reusable, time-scaled Braille chart for Trainlog Notcurses surfaces.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/terminal.h"

typedef struct TrainlogChartRect {
    int top;
    int left;
    int height;
    int width;
} TrainlogChartRect;

typedef struct TrainlogChartPoint {
    int64_t timestamp;
    double value;
    /* Optional borrowed RFC3339 text used only for its YYYY-MM-DD label. */
    const char *timestamp_label;
} TrainlogChartPoint;

typedef struct TrainlogChartScale {
    int64_t minimum_timestamp;
    int64_t maximum_timestamp;
    double minimum_value;
    double maximum_value;
} TrainlogChartScale;

typedef struct TrainlogChartOptions {
    bool has_time_domain;
    int64_t minimum_timestamp;
    int64_t maximum_timestamp;
    bool non_negative;
    bool zero_baseline;
} TrainlogChartOptions;

/**
 * Build the visible data scale, including a deterministic Y margin.
 *
 * CONTRACT: points are ordered by nondecreasing timestamp and values are
 * finite. Zero points is valid and returns false because no scale exists.
 * INVARIANT: a successful scale always has a positive Y span; a one-point
 * time span is represented without inventing another observation.
 */
bool trainlog_chart_scale(const TrainlogChartPoint *points, size_t count,
                          TrainlogChartScale *scale);
bool trainlog_chart_scale_with_options(const TrainlogChartPoint *points,
    size_t count, const TrainlogChartOptions *options,
    TrainlogChartScale *scale);

/* Pure coordinate mappings used by the renderer and terminal-free tests. */
int trainlog_chart_map_x(const TrainlogChartScale *scale, int64_t timestamp,
                         int logical_width);
int trainlog_chart_map_y(const TrainlogChartScale *scale, double value,
                         int logical_height);

/* Braille cells encode a 2x4 logical raster. Out-of-cell coordinates reject. */
uint8_t trainlog_chart_braille_dot(uint8_t cell, int subpixel_x,
                                   int subpixel_y);
uint32_t trainlog_chart_braille_codepoint(uint8_t cell);

/**
 * Render one complete chart inside a caller-owned Notcurses surface rectangle.
 *
 * WHY: statistics screens share graph semantics without owning terminal-backend
 * details or duplicating scaling/rasterization. CONTRACT: title and unit are
 * borrowed for this call; the function creates no persistent terminal state.
 * INVARIANT: every write is clipped to rect and the current ncplane geometry.
 */
void trainlog_chart_render(TrainlogSurface *surface, TrainlogChartRect rect,
                           const TrainlogChartPoint *points, size_t count,
                           const char *unit, const char *title);

void trainlog_chart_render_line(TrainlogSurface *surface, TrainlogChartRect rect,
    const TrainlogChartPoint *points, size_t count, const char *unit,
    const char *title, const TrainlogChartOptions *options);

/** Render discrete time buckets with one shared Y scale and no interpolation. */
void trainlog_chart_render_bars(TrainlogSurface *surface, TrainlogChartRect rect,
    const TrainlogChartPoint *points, size_t count, const char *unit,
    const char *title, const TrainlogChartOptions *options);

#endif
