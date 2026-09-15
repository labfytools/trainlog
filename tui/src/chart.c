/**
 * @file chart.c
 * @brief Time-scaled high-resolution Braille chart renderer.
 */

#include "trainlog/chart.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHART_LABEL_WIDTH 12
#define CHART_MIN_WIDTH 24
#define CHART_MIN_HEIGHT 6

static int clamp_coordinate(int value, int limit)
{
    if (limit <= 1) return 0;
    if (value < 0) return 0;
    if (value >= limit) return limit - 1;
    return value;
}

bool trainlog_chart_scale(const TrainlogChartPoint *points, size_t count,
                          TrainlogChartScale *scale)
{
    return trainlog_chart_scale_with_options(points, count, NULL, scale);
}

bool trainlog_chart_scale_with_options(const TrainlogChartPoint *points,
    size_t count, const TrainlogChartOptions *options,
    TrainlogChartScale *scale)
{
    size_t index;
    double minimum;
    double maximum;
    double span;
    double margin;
    if (scale == NULL || (count > 0U && points == NULL) || count == 0U)
        return false;
    minimum = points[0].value;
    maximum = minimum;
    if (!isfinite(minimum)) return false;
    for (index = 1U; index < count; ++index) {
        if (!isfinite(points[index].value) ||
            points[index].timestamp < points[index - 1U].timestamp)
            return false;
        if (points[index].value < minimum) minimum = points[index].value;
        if (points[index].value > maximum) maximum = points[index].value;
    }
    span = maximum - minimum;
    /* WHY: a modest 8% headroom keeps observations off the frame while the
     * labelled scale prevents a visually amplified small change from hiding
     * its actual magnitude. Constant series use 2% of their value (at least
     * one unit total) solely to make their horizontal position defined. */
    if (span > 0.0) margin = span * 0.08;
    else {
        margin = fabs(minimum) * 0.02;
        if (margin < 0.5) margin = 0.5;
    }
    if (!isfinite(margin) || minimum < -DBL_MAX + margin ||
        maximum > DBL_MAX - margin) return false;
    scale->minimum_timestamp = options != NULL && options->has_time_domain
        ? options->minimum_timestamp : points[0].timestamp;
    scale->maximum_timestamp = options != NULL && options->has_time_domain
        ? options->maximum_timestamp : points[count - 1U].timestamp;
    if (scale->maximum_timestamp < scale->minimum_timestamp) return false;
    scale->minimum_value = options != NULL && options->zero_baseline
        ? 0.0 : minimum - margin;
    scale->maximum_value = maximum + margin;
    if (options != NULL && options->non_negative && scale->minimum_value < 0.0)
        scale->minimum_value = 0.0;
    if (scale->maximum_value <= scale->minimum_value)
        scale->maximum_value = scale->minimum_value + (maximum > 0.0 ? maximum * 0.08 : 1.0);
    return true;
}

int trainlog_chart_map_x(const TrainlogChartScale *scale, int64_t timestamp,
                         int logical_width)
{
    long double ratio;
    if (scale == NULL || logical_width <= 1 ||
        scale->maximum_timestamp <= scale->minimum_timestamp)
        return logical_width > 1 ? (logical_width - 1) / 2 : 0;
    ratio = ((long double)timestamp - (long double)scale->minimum_timestamp) /
        ((long double)scale->maximum_timestamp -
         (long double)scale->minimum_timestamp);
    return clamp_coordinate((int)llroundl(ratio * (logical_width - 1)),
                            logical_width);
}

int trainlog_chart_map_y(const TrainlogChartScale *scale, double value,
                         int logical_height)
{
    double ratio;
    if (scale == NULL || logical_height <= 1 ||
        scale->maximum_value <= scale->minimum_value)
        return logical_height > 1 ? (logical_height - 1) / 2 : 0;
    ratio = (scale->maximum_value - value) /
        (scale->maximum_value - scale->minimum_value);
    return clamp_coordinate((int)llround(ratio * (logical_height - 1)),
                            logical_height);
}

uint8_t trainlog_chart_braille_dot(uint8_t cell, int subpixel_x,
                                   int subpixel_y)
{
    static const uint8_t dots[4][2] = {
        {0x01U, 0x08U}, {0x02U, 0x10U}, {0x04U, 0x20U}, {0x40U, 0x80U}
    };
    if (subpixel_x < 0 || subpixel_x > 1 ||
        subpixel_y < 0 || subpixel_y > 3) return cell;
    return (uint8_t)(cell | dots[subpixel_y][subpixel_x]);
}

uint32_t trainlog_chart_braille_codepoint(uint8_t cell)
{
    return 0x2800U + (uint32_t)cell;
}

static void raster_dot(uint8_t *cells, int cell_width, int cell_height,
                       int x, int y)
{
    int cell_x;
    int cell_y;
    if (cells == NULL || x < 0 || y < 0 || x >= cell_width * 2 ||
        y >= cell_height * 4) return;
    cell_x = x / 2;
    cell_y = y / 4;
    cells[(size_t)cell_y * (size_t)cell_width + (size_t)cell_x] =
        trainlog_chart_braille_dot(
            cells[(size_t)cell_y * (size_t)cell_width + (size_t)cell_x],
            x % 2, y % 4);
}

static void raster_line(uint8_t *cells, int cell_width, int cell_height,
                        int x0, int y0, int x1, int y1)
{
    int64_t dx = llabs((int64_t)x1 - (int64_t)x0);
    int sx = x0 < x1 ? 1 : -1;
    int64_t dy = -llabs((int64_t)y1 - (int64_t)y0);
    int sy = y0 < y1 ? 1 : -1;
    int64_t error = dx + dy;
    for (;;) {
        raster_dot(cells, cell_width, cell_height, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        {
            int64_t doubled = error * INT64_C(2);
            if (doubled >= dy) { error += dy; x0 += sx; }
            if (doubled <= dx) { error += dx; y0 += sy; }
        }
    }
}

static void short_date(const TrainlogChartPoint *point, char output[6])
{
    time_t seconds;
    struct tm value;
    if (point != NULL && point->timestamp_label != NULL &&
        strlen(point->timestamp_label) >= 10U &&
        point->timestamp_label[4] == '-' && point->timestamp_label[7] == '-') {
        output[0] = point->timestamp_label[8];
        output[1] = point->timestamp_label[9];
        output[2] = '/';
        output[3] = point->timestamp_label[5];
        output[4] = point->timestamp_label[6];
        output[5] = '\0';
        return;
    }
    seconds = point != NULL ? (time_t)point->timestamp : (time_t)-1;
    if (point == NULL || (int64_t)seconds != point->timestamp ||
        gmtime_r(&seconds, &value) == NULL ||
        strftime(output, 6U, "%d/%m", &value) != 5U)
        (void)snprintf(output, 6U, "--/--");
}

static void render_empty(TrainlogSurface *surface, TrainlogChartRect rect,
                         const char *title)
{
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_BOLD);
    trainlog_surface_printf(surface, rect.top, rect.left, "%s",
        title != NULL ? title : "ÉVOLUTION DANS LE TEMPS");
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    if (rect.height > 2)
        trainlog_surface_printf(surface, rect.top + 2, rect.left,
            "Aucune donnée disponible.");
}

void trainlog_chart_render(TrainlogSurface *surface, TrainlogChartRect rect,
                           const TrainlogChartPoint *points, size_t count,
                           const char *unit, const char *title)
{
    trainlog_chart_render_line(surface, rect, points, count, unit, title, NULL);
}

void trainlog_chart_render_line(TrainlogSurface *surface, TrainlogChartRect rect,
                           const TrainlogChartPoint *points, size_t count,
                           const char *unit, const char *title,
                           const TrainlogChartOptions *options)
{
    TrainlogChartScale scale;
    uint8_t *cells;
    size_t cell_count;
    int plot_left;
    int plot_top;
    int plot_width;
    int plot_height;
    int logical_width;
    int logical_height;
    size_t index;
    char first_date[6];
    char last_date[6];
    const char *safe_unit = unit != NULL ? unit : "";
    if (surface == NULL || rect.top < 0 || rect.left < 0 ||
        rect.height <= 0 || rect.width <= 0 ||
        rect.top > INT_MAX - rect.height ||
        rect.left > INT_MAX - rect.width) return;
    if (count == 0U) { render_empty(surface, rect, title); return; }
    if (!trainlog_chart_scale_with_options(points, count, options, &scale)) {
        render_empty(surface, rect, title);
        return;
    }
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_BOLD);
    trainlog_surface_printf(surface, rect.top, rect.left, "%s",
        title != NULL ? title : "ÉVOLUTION DANS LE TEMPS");
    if (rect.width < CHART_MIN_WIDTH || rect.height < CHART_MIN_HEIGHT) {
        trainlog_surface_set_role(surface, TRAINLOG_COLOR_ACCENT,
            TRAINLOG_RGB_BASE, TRAINLOG_TEXT_BOLD);
        if (count == 1U)
            trainlog_surface_printf(surface,
                rect.top + (rect.height > 2 ? 2 : 1), rect.left,
                "◆ %.2f %s", points[0].value, safe_unit);
        else
            trainlog_surface_printf(surface,
                rect.top + (rect.height > 2 ? 2 : 1), rect.left,
                "◆ %zu relevés · %.2f %s", count,
                points[count - 1U].value, safe_unit);
        return;
    }
    plot_left = rect.left + CHART_LABEL_WIDTH;
    plot_top = rect.top + 2;
    plot_width = rect.width - CHART_LABEL_WIDTH - 1;
    plot_height = rect.height - 5;
    if (plot_width <= 0 || plot_height <= 0 ||
        plot_width > INT_MAX / 2 || plot_height > INT_MAX / 4 ||
        (size_t)plot_width > SIZE_MAX / (size_t)plot_height) return;
    cell_count = (size_t)plot_width * (size_t)plot_height;
    cells = calloc(cell_count, sizeof(*cells));
    if (cells == NULL) return;
    logical_width = plot_width * 2;
    logical_height = plot_height * 4;
    for (index = 1U; index < count; ++index) {
        raster_line(cells, plot_width, plot_height,
            trainlog_chart_map_x(&scale, points[index - 1U].timestamp,
                                 logical_width),
            trainlog_chart_map_y(&scale, points[index - 1U].value,
                                 logical_height),
            trainlog_chart_map_x(&scale, points[index].timestamp, logical_width),
            trainlog_chart_map_y(&scale, points[index].value, logical_height));
    }
    if (count == 1U)
        raster_dot(cells, plot_width, plot_height,
            trainlog_chart_map_x(&scale, points[0].timestamp, logical_width),
            trainlog_chart_map_y(&scale, points[0].value, logical_height));

    trainlog_surface_set_role(surface, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    trainlog_surface_printf(surface, plot_top, rect.left, "%8.2f %s",
        scale.maximum_value, safe_unit);
    trainlog_surface_printf(surface, plot_top + plot_height - 1, rect.left,
        "%8.2f %s", scale.minimum_value, safe_unit);
    for (int row = 0; row < plot_height; ++row)
        trainlog_surface_draw(surface, plot_top + row, plot_left - 1, 0x2502U);
    trainlog_surface_draw(surface, plot_top + plot_height, plot_left - 1, 0x2514U);
    for (int column = 0; column < plot_width; ++column)
        trainlog_surface_draw(surface, plot_top + plot_height,
                              plot_left + column, 0x2500U);

    trainlog_surface_set_role(surface, TRAINLOG_COLOR_GRAPH,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    for (int row = 0; row < plot_height; ++row)
        for (int column = 0; column < plot_width; ++column) {
            uint8_t cell = cells[(size_t)row * (size_t)plot_width +
                                 (size_t)column];
            if (cell != 0U)
                trainlog_surface_draw(surface, plot_top + row,
                    plot_left + column, trainlog_chart_braille_codepoint(cell));
        }
    free(cells);

    /* CONTRACT: diamonds identify stored observations, while Braille dots
     * between them are interpolation only. Several observations may map to one
     * cell after clipping; this never changes or fabricates source values. */
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_ACCENT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_BOLD);
    for (index = 0U; index < count; ++index) {
        int logical_x = trainlog_chart_map_x(&scale, points[index].timestamp,
                                             logical_width);
        int logical_y = trainlog_chart_map_y(&scale, points[index].value,
                                             logical_height);
        trainlog_surface_draw(surface, plot_top + logical_y / 4,
                              plot_left + logical_x / 2, 0x25c6U);
    }
    {
        TrainlogChartPoint first_domain = {scale.minimum_timestamp, 0.0, NULL};
        TrainlogChartPoint last_domain = {scale.maximum_timestamp, 0.0, NULL};
        short_date(options != NULL && options->has_time_domain
            ? &first_domain : &points[0], first_date);
        short_date(options != NULL && options->has_time_domain
            ? &last_domain : &points[count - 1U], last_date);
    }
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    if (count == 1U) {
        trainlog_surface_printf(surface, plot_top + plot_height + 1,
            plot_left + (plot_width - 5) / 2, "%s", first_date);
    } else {
        trainlog_surface_printf(surface, plot_top + plot_height + 1,
                                plot_left, "%s", first_date);
        trainlog_surface_printf(surface, plot_top + plot_height + 1,
            plot_left + plot_width - 5, "%s", last_date);
    }
    if (count > 2U && plot_width >= 34) {
        size_t middle = count / 2U;
        int column = plot_left + trainlog_chart_map_x(&scale,
            points[middle].timestamp, logical_width) / 2;
        short_date(&points[middle], first_date);
        if (column > plot_left + 6 && column < plot_left + plot_width - 10) {
            trainlog_surface_draw(surface, plot_top + plot_height,
                                  column, 0x252cU);
            trainlog_surface_printf(surface, plot_top + plot_height + 1,
                                    column - 2, "%s", first_date);
        }
    }
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
}

void trainlog_chart_render_bars(TrainlogSurface *surface, TrainlogChartRect rect,
    const TrainlogChartPoint *points, size_t count, const char *unit,
    const char *title, const TrainlogChartOptions *options)
{
    TrainlogChartScale scale;
    int plot_left, plot_top, plot_width, plot_height;
    size_t index;
    const char *safe_unit = unit != NULL ? unit : "";
    if (surface == NULL || rect.height <= 0 || rect.width <= 0) return;
    if (count == 0U) { render_empty(surface, rect, title); return; }
    if (!trainlog_chart_scale_with_options(points, count, options, &scale)) {
        render_empty(surface, rect, title); return;
    }
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_BOLD);
    trainlog_surface_printf(surface, rect.top, rect.left, "%s",
        title != NULL ? title : "TOTAUX PAR PÉRIODE");
    if (rect.width < CHART_MIN_WIDTH || rect.height < CHART_MIN_HEIGHT) {
        trainlog_surface_printf(surface, rect.top + 2, rect.left,
            "%zu périodes · %.2f %s", count, points[count - 1U].value, safe_unit);
        return;
    }
    plot_left = rect.left + CHART_LABEL_WIDTH;
    plot_top = rect.top + 2;
    plot_width = rect.width - CHART_LABEL_WIDTH - 1;
    plot_height = rect.height - 5;
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    trainlog_surface_printf(surface, plot_top, rect.left, "%8.2f %s",
        scale.maximum_value, safe_unit);
    trainlog_surface_printf(surface, plot_top + plot_height - 1, rect.left,
        "%8.2f %s", scale.minimum_value, safe_unit);
    for (int row = 0; row < plot_height; ++row)
        trainlog_surface_draw(surface, plot_top + row, plot_left - 1, 0x2502U);
    for (int column = 0; column < plot_width; ++column)
        trainlog_surface_draw(surface, plot_top + plot_height,
            plot_left + column, 0x2500U);
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_GRAPH,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    for (index = 0U; index < count; ++index) {
        int center = trainlog_chart_map_x(&scale, points[index].timestamp, plot_width);
        int top_row = trainlog_chart_map_y(&scale, points[index].value, plot_height);
        int half = count > 0U ? plot_width / (int)(count * 3U) : 0;
        if (points[index].value <= scale.minimum_value) continue;
        if (half < 0) half = 0;
        if (half > 2) half = 2;
        for (int row = top_row; row < plot_height; ++row)
            for (int column = center - half; column <= center + half; ++column)
                if (column >= 0 && column < plot_width)
                    trainlog_surface_draw(surface, plot_top + row,
                        plot_left + column, 0x2588U);
    }
    {
        char first[6], last[6];
        TrainlogChartPoint first_domain = {scale.minimum_timestamp, 0.0, NULL};
        TrainlogChartPoint last_domain = {scale.maximum_timestamp, 0.0, NULL};
        trainlog_surface_set_role(surface, TRAINLOG_COLOR_MUTED,
            TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
        if (count <= 8U && plot_width / (int)count >= 12) {
            for (index = 0U; index < count; ++index) {
                int center = trainlog_chart_map_x(&scale,
                    points[index].timestamp, plot_width);
                int column = plot_left + center - 5;
                const char *label = points[index].timestamp_label;
                if (column < plot_left) column = plot_left;
                if (column > plot_left + plot_width - 11)
                    column = plot_left + plot_width - 11;
                if (label != NULL)
                    trainlog_surface_printf(surface,
                        plot_top + plot_height + 1, column, "%.11s", label);
            }
        } else {
            short_date(&first_domain, first); short_date(&last_domain, last);
            trainlog_surface_printf(surface, plot_top + plot_height + 1,
                plot_left, "%s", first);
            trainlog_surface_printf(surface, plot_top + plot_height + 1,
                plot_left + plot_width - 5, "%s", last);
        }
    }
}
