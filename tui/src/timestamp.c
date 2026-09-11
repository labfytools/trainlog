#include "timestamp.h"

#include <limits.h>

static bool parse_digits(const char *value, size_t count, int *output)
{
    size_t index;
    int result = 0;
    for (index = 0U; index < count; ++index) {
        if (value[index] < '0' || value[index] > '9') return false;
        result = result * 10 + (value[index] - '0');
    }
    *output = result;
    return true;
}

static bool leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const int DAYS[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return month == 2 && leap_year(year) ? 29 : DAYS[month - 1];
}

/* Proleptic Gregorian day number relative to 0001-01-01. */
static int64_t day_number(int year, int month, int day)
{
    int prior_year = year - 1;
    int64_t days = (int64_t)prior_year * 365 + prior_year / 4 -
        prior_year / 100 + prior_year / 400;
    int current_month;
    for (current_month = 1; current_month < month; ++current_month)
        days += days_in_month(year, current_month);
    return days + day - 1;
}

bool trainlog_timestamp_parse(
    const char *value, size_t length, TrainlogTimestampKey *output)
{
    size_t zone;
    int year, month, day, hour, minute, second = 0;
    int offset_hour = 0, offset_minute = 0, offset_sign = 0;
    int64_t local_second;

    if (value == NULL || output == NULL || length < 17U ||
            !parse_digits(value, 4U, &year) || value[4] != '-' ||
            !parse_digits(value + 5, 2U, &month) || value[7] != '-' ||
            !parse_digits(value + 8, 2U, &day) ||
            (value[10] != 'T' && value[10] != 't') ||
            !parse_digits(value + 11, 2U, &hour) || value[13] != ':' ||
            !parse_digits(value + 14, 2U, &minute)) return false;
    if (year < 1 || month < 1 || month > 12 || day < 1 ||
            day > days_in_month(year, month) || hour > 23 || minute > 59)
        return false;

    zone = 16U;
    output->fraction = NULL;
    output->fraction_length = 0U;
    if (zone < length && value[zone] == ':') {
        if (zone + 3U > length || !parse_digits(value + zone + 1U, 2U, &second) || second > 59)
            return false;
        zone += 3U;
        if (zone < length && value[zone] == '.') {
            size_t fraction_start = ++zone;
            while (zone < length && value[zone] >= '0' && value[zone] <= '9') ++zone;
            if (zone == fraction_start) return false;
            output->fraction = value + fraction_start;
            output->fraction_length = zone - fraction_start;
        }
    }

    if (zone + 1U == length && (value[zone] == 'Z' || value[zone] == 'z')) {
        offset_sign = 0;
    } else {
        if (zone + 6U != length || (value[zone] != '+' && value[zone] != '-') ||
                value[zone + 3U] != ':' ||
                !parse_digits(value + zone + 1U, 2U, &offset_hour) ||
                !parse_digits(value + zone + 4U, 2U, &offset_minute) ||
                offset_hour > 23 || offset_minute > 59) return false;
        offset_sign = value[zone] == '+' ? 1 : -1;
    }

    output->local_day = day_number(year, month, day);
    local_second = output->local_day * INT64_C(86400) +
        (int64_t)hour * INT64_C(3600) + (int64_t)minute * INT64_C(60) + second;
    output->utc_second = local_second - offset_sign *
        ((int64_t)offset_hour * INT64_C(3600) + (int64_t)offset_minute * INT64_C(60));
    return true;
}

int trainlog_timestamp_compare(
    const TrainlogTimestampKey *left, const TrainlogTimestampKey *right)
{
    size_t index;
    size_t count;
    if (left->utc_second != right->utc_second)
        return left->utc_second < right->utc_second ? -1 : 1;
    count = left->fraction_length > right->fraction_length
        ? left->fraction_length : right->fraction_length;
    for (index = 0U; index < count; ++index) {
        char left_digit = index < left->fraction_length ? left->fraction[index] : '0';
        char right_digit = index < right->fraction_length ? right->fraction[index] : '0';
        if (left_digit != right_digit) return left_digit < right_digit ? -1 : 1;
    }
    return 0;
}
