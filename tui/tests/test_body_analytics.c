/**
 * @file test_body_analytics.c
 * @brief Derived body analytics regression tests.
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/body_analytics.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(                                                   \
                stderr,                                                      \
                "CHECK failed at %s:%d: %s\n",                               \
                __FILE__,                                                    \
                __LINE__,                                                    \
                #condition                                                   \
            );                                                               \
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool near(
    double value,
    double expected,
    double tolerance
)
{
    return
        fabs(
            value -
            expected
        ) <= tolerance;
}

static TrainlogBodyObservationRecord
male_record(void)
{
    TrainlogBodyObservationRecord record;

    (void)memset(
        &record,
        0,
        sizeof(record)
    );

    record.has_body_weight = true;
    record.body_weight_kg = 80.0;

    record.has_neck = true;
    record.neck_cm = 40.0;

    record.has_shoulders = true;
    record.shoulders_cm = 120.0;

    record.has_chest = true;
    record.chest_cm = 100.0;

    record.has_waist = true;
    record.waist_cm = 90.0;

    record.has_hips = true;
    record.hips_cm = 100.0;

    record.has_left_arm = true;
    record.left_arm_cm = 35.0;
    record.has_right_arm = true;
    record.right_arm_cm = 36.0;

    record.has_left_forearm = true;
    record.left_forearm_cm = 29.0;
    record.has_right_forearm = true;
    record.right_forearm_cm = 29.5;

    record.has_left_thigh = true;
    record.left_thigh_cm = 58.0;
    record.has_right_thigh = true;
    record.right_thigh_cm = 58.0;

    record.has_left_calf = true;
    record.left_calf_cm = 39.0;
    record.has_right_calf = true;
    record.right_calf_cm = 40.0;

    return record;
}

static bool test_male_estimate(void)
{
    TrainlogBodyAnalyticsProfile profile;
    TrainlogBodyObservationRecord record =
        male_record();

    TrainlogBodyAnalyticsResult result;

    profile.formula =
        TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;

    profile.height_cm = 180.0;

    CHECK(
        trainlog_body_analytics_calculate(
            &profile,
            &record,
            &result
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        result.has_body_fat_estimate
    );

    CHECK(
        near(
            result.body_fat_percent,
            18.4621,
            0.01
        )
    );

    CHECK(
        result.has_fat_mass_estimate
    );

    CHECK(
        near(
            result.fat_mass_kg,
            14.7697,
            0.02
        )
    );

    CHECK(
        result.has_lean_mass_estimate
    );

    CHECK(
        near(
            result.lean_mass_kg,
            65.2303,
            0.02
        )
    );

    CHECK(
        result.has_waist_hip_ratio &&
        near(
            result.waist_hip_ratio,
            0.9,
            0.0001
        )
    );

    CHECK(
        result.has_shoulder_waist_ratio &&
        near(
            result.shoulder_waist_ratio,
            1.333333,
            0.0001
        )
    );

    CHECK(
        result.has_chest_waist_ratio &&
        near(
            result.chest_waist_ratio,
            1.111111,
            0.0001
        )
    );

    CHECK(
        result.has_arm_asymmetry &&
        near(
            result.arm_asymmetry_percent,
            2.8169,
            0.01
        )
    );

    CHECK(
        result.has_thigh_asymmetry &&
        near(
            result.thigh_asymmetry_percent,
            0.0,
            0.0001
        )
    );

    return true;
}

static bool test_female_estimate(void)
{
    TrainlogBodyAnalyticsProfile profile;
    TrainlogBodyObservationRecord record;

    TrainlogBodyAnalyticsResult result;

    (void)memset(
        &record,
        0,
        sizeof(record)
    );

    profile.formula =
        TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE;

    profile.height_cm = 165.0;

    record.has_body_weight = true;
    record.body_weight_kg = 65.0;

    record.has_neck = true;
    record.neck_cm = 34.0;

    record.has_waist = true;
    record.waist_cm = 75.0;

    record.has_hips = true;
    record.hips_cm = 100.0;

    CHECK(
        trainlog_body_analytics_calculate(
            &profile,
            &record,
            &result
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        result.has_body_fat_estimate
    );

    CHECK(
        near(
            result.body_fat_percent,
            29.2385,
            0.01
        )
    );

    return true;
}

static bool test_profile_independent_metrics(void)
{
    TrainlogBodyObservationRecord record =
        male_record();

    TrainlogBodyAnalyticsResult result;

    CHECK(
        trainlog_body_analytics_calculate(
            NULL,
            &record,
            &result
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        !result.has_body_fat_estimate
    );

    CHECK(
        result.has_waist_hip_ratio
    );

    CHECK(
        result.has_arm_asymmetry
    );

    return true;
}

static bool test_missing_required_measurement(void)
{
    TrainlogBodyAnalyticsProfile profile;
    TrainlogBodyObservationRecord record =
        male_record();

    TrainlogBodyAnalyticsResult result;

    profile.formula =
        TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;

    profile.height_cm = 180.0;

    record.has_neck = false;
    record.neck_cm = 0.0;

    CHECK(
        trainlog_body_analytics_calculate(
            &profile,
            &record,
            &result
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        !result.has_body_fat_estimate
    );

    return true;
}

static bool test_invalid_profile(void)
{
    TrainlogBodyAnalyticsProfile profile;
    TrainlogBodyObservationRecord record =
        male_record();

    TrainlogBodyAnalyticsResult result;

    profile.formula =
        TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;

    profile.height_cm = 0.0;

    CHECK(
        trainlog_body_analytics_calculate(
            &profile,
            &record,
            &result
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return true;
}

int main(void)
{
    CHECK(
        test_male_estimate()
    );

    CHECK(
        test_female_estimate()
    );

    CHECK(
        test_profile_independent_metrics()
    );

    CHECK(
        test_missing_required_measurement()
    );

    CHECK(
        test_invalid_profile()
    );

    (void)printf(
        "PASS body_analytics\n"
    );

    return 0;
}
