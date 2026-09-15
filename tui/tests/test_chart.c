/**
 * @file test_chart.c
 * @brief Terminal-free geometry and Braille chart regressions.
 */

#include "trainlog/chart.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

struct TrainlogSurface {
    TrainlogChartRect bounds;
    bool overflow;
    size_t draw_count;
    size_t axis_count;
    size_t marker_count;
    size_t braille_count;
    size_t block_count;
    size_t print_count;
};

#define CHECK(condition) do { if (!(condition)) {                          \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                 \
        __FILE__, __LINE__, #condition); return false; } } while (0)

/* Renderer symbols capture the production component's writes without creating
 * an interactive terminal or depending on a particular terminal emulator. */
void trainlog_surface_set_role(TrainlogSurface *surface,
    TrainlogColorRole foreground, unsigned background_rgb,
    TrainlogTextStyle style)
{ (void)surface; (void)foreground; (void)background_rgb; (void)style; }
void trainlog_surface_printf(TrainlogSurface *surface, int row, int column,
    const char *format, ...)
{
    (void)format;
    if (row < surface->bounds.top || column < surface->bounds.left ||
        row >= surface->bounds.top + surface->bounds.height ||
        column >= surface->bounds.left + surface->bounds.width)
        surface->overflow = true;
    ++surface->print_count;
}
void trainlog_surface_draw(TrainlogSurface *surface, int row, int column,
    uint32_t codepoint)
{
    if (row < surface->bounds.top || column < surface->bounds.left ||
        row >= surface->bounds.top + surface->bounds.height ||
        column >= surface->bounds.left + surface->bounds.width)
        surface->overflow = true;
    ++surface->draw_count;
    if (codepoint == 0x2500U || codepoint == 0x2502U || codepoint == 0x2514U)
        ++surface->axis_count;
    if (codepoint == 0x25c6U) ++surface->marker_count;
    if (codepoint >= 0x2801U && codepoint <= 0x28ffU)
        ++surface->braille_count;
    if (codepoint == 0x2588U) ++surface->block_count;
}

static bool test_scales_and_mapping(void)
{
    TrainlogChartScale scale;
    TrainlogChartPoint growing[] = {
        {0, 83.7, NULL}, {86400, 84.0, NULL}, {691200, 85.1, NULL}};
    TrainlogChartPoint falling[] = {{0, 10.0, NULL}, {10, 5.0, NULL}};
    TrainlogChartPoint constant[] = {{0, 42.0, NULL}, {10, 42.0, NULL}};
    TrainlogChartPoint one[] = {{50, 7.0, NULL}};
    CHECK(!trainlog_chart_scale(NULL, 0U, &scale));
    CHECK(trainlog_chart_scale(one, 1U, &scale));
    CHECK(scale.minimum_value < 7.0 && scale.maximum_value > 7.0);
    CHECK(trainlog_chart_map_x(&scale, 50, 20) == 9);
    CHECK(trainlog_chart_scale(constant, 2U, &scale));
    CHECK(scale.maximum_value > scale.minimum_value);
    CHECK(trainlog_chart_map_y(&scale, 42.0, 17) == 8);
    CHECK(trainlog_chart_scale(growing, 3U, &scale));
    CHECK(trainlog_chart_map_x(&scale, 0, 101) == 0);
    CHECK(trainlog_chart_map_x(&scale, 86400, 101) == 13);
    CHECK(trainlog_chart_map_x(&scale, 691200, 101) == 100);
    CHECK(trainlog_chart_map_x(&scale, -100, 101) == 0);
    CHECK(trainlog_chart_map_x(&scale, 900000, 101) == 100);
    CHECK(trainlog_chart_map_y(&scale, 85.1, 100) <
          trainlog_chart_map_y(&scale, 83.7, 100));
    CHECK(trainlog_chart_scale(falling, 2U, &scale));
    CHECK(trainlog_chart_map_y(&scale, 10.0, 20) <
          trainlog_chart_map_y(&scale, 5.0, 20));
    return true;
}

static bool test_invalid_and_small_dimensions(void)
{
    TrainlogChartScale scale = {0, 10, 0.0, 10.0};
    TrainlogChartPoint unordered[] = {{10, 1.0, NULL}, {0, 2.0, NULL}};
    TrainlogChartPoint invalid[] = {{0, NAN, NULL}};
    CHECK(!trainlog_chart_scale(unordered, 2U, &scale));
    CHECK(!trainlog_chart_scale(invalid, 1U, &scale));
    CHECK(trainlog_chart_map_x(&scale, 5, 0) == 0);
    CHECK(trainlog_chart_map_y(&scale, 5.0, 1) == 0);
    return true;
}

static bool test_explicit_domains_and_non_negative_scale(void)
{
    TrainlogChartPoint points[] = {{800, 1.0, NULL}, {900, 2.0, NULL}};
    TrainlogChartOptions seven = {true, 300, 1000, true, true};
    TrainlogChartOptions thirty = {true, -2000, 1000, true, true};
    TrainlogChartScale scale7;
    TrainlogChartScale scale30;
    CHECK(trainlog_chart_scale_with_options(points, 2U, &seven, &scale7));
    CHECK(trainlog_chart_scale_with_options(points, 2U, &thirty, &scale30));
    CHECK(scale7.minimum_timestamp == 300 && scale30.minimum_timestamp == -2000);
    CHECK(trainlog_chart_map_x(&scale7, 800, 100) !=
          trainlog_chart_map_x(&scale30, 800, 100));
    CHECK(scale7.minimum_value == 0.0 && scale30.minimum_value == 0.0);
    return true;
}

static bool test_braille_encoding(void)
{
    uint8_t cell = 0U;
    cell = trainlog_chart_braille_dot(cell, 0, 0);
    cell = trainlog_chart_braille_dot(cell, 1, 3);
    CHECK(cell == 0x81U);
    CHECK(trainlog_chart_braille_codepoint(cell) == 0x2881U);
    CHECK(trainlog_chart_braille_dot(0U, 1, 0) == 0x08U);
    CHECK(trainlog_chart_braille_dot(0U, 0, 3) == 0x40U);
    CHECK(trainlog_chart_braille_dot(0x01U, 2, 0) == 0x01U);
    return true;
}

static bool test_rendering_is_bounded(void)
{
    TrainlogSurface surface;
    TrainlogChartRect rect = {3, 4, 9, 52};
    TrainlogChartPoint points[] = {
        {1788739200, 83.7, "2026-09-07T00:00:00+02:00"},
        {1788998400, 85.1, "2026-09-10T00:00:00+02:00"},
        {1789430400, 84.2, "2026-09-15T00:00:00+02:00"}
    };
    (void)memset(&surface, 0, sizeof(surface)); surface.bounds = rect;
    trainlog_chart_render(&surface, rect, NULL, 0U, "kg", "Poids");
    CHECK(!surface.overflow && surface.draw_count == 0U &&
          surface.print_count >= 2U);

    (void)memset(&surface, 0, sizeof(surface)); surface.bounds = rect;
    trainlog_chart_render(&surface, rect, points, 1U, "kg", "Poids");
    CHECK(!surface.overflow && surface.marker_count == 1U &&
          surface.axis_count > 0U && surface.braille_count > 0U);

    (void)memset(&surface, 0, sizeof(surface)); surface.bounds = rect;
    trainlog_chart_render(&surface, rect, points, 2U, "kg", "Poids");
    CHECK(!surface.overflow && surface.marker_count == 2U &&
          surface.braille_count > 0U);

    /* The compact fallback still stays inside an arbitrarily placed small
     * rectangle; it cannot leak into sibling planes or the footer. */
    rect = (TrainlogChartRect){7, 9, 3, 12};
    (void)memset(&surface, 0, sizeof(surface)); surface.bounds = rect;
    trainlog_chart_render(&surface, rect, points, 3U, "kg", "Poids");
    CHECK(!surface.overflow && surface.draw_count == 0U &&
          surface.print_count >= 2U);

    rect = (TrainlogChartRect){3, 4, 9, 52};
    (void)memset(&surface, 0, sizeof(surface)); surface.bounds = rect;
    {
        TrainlogChartOptions options = {true, 1788134400, 1789430400, true, true};
        trainlog_chart_render_bars(&surface, rect, points, 2U, "kg",
            "Volume hebdomadaire", &options);
    }
    CHECK(!surface.overflow && surface.block_count > 0U &&
          surface.braille_count == 0U && surface.marker_count == 0U);
    return true;
}

int main(void)
{
    if (!test_scales_and_mapping() || !test_invalid_and_small_dimensions() ||
        !test_explicit_domains_and_non_negative_scale() ||
        !test_braille_encoding() || !test_rendering_is_bounded()) return 1;
    (void)printf("PASS chart\n");
    return 0;
}
