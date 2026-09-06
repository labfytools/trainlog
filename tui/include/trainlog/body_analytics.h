#ifndef TRAINLOG_BODY_ANALYTICS_H
#define TRAINLOG_BODY_ANALYTICS_H

/**
 * @file body_analytics.h
 * @brief Derived body-composition and proportion analytics.
 *
 * Analytics are display-time estimates derived from real body observations.
 * No estimated value is persisted as if it were a direct measurement.
 */

#include <stdbool.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

typedef enum TrainlogBodyAnalyticsFormula {
    TRAINLOG_BODY_ANALYTICS_FORMULA_MALE = 0,
    TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
} TrainlogBodyAnalyticsFormula;

typedef struct TrainlogBodyAnalyticsProfile {
    TrainlogBodyAnalyticsFormula formula;
    double height_cm;
} TrainlogBodyAnalyticsProfile;

typedef struct TrainlogBodyAnalyticsResult {
    bool has_body_fat_estimate;
    double body_fat_percent;

    bool has_fat_mass_estimate;
    double fat_mass_kg;

    bool has_lean_mass_estimate;
    double lean_mass_kg;

    bool has_waist_hip_ratio;
    double waist_hip_ratio;

    bool has_shoulder_waist_ratio;
    double shoulder_waist_ratio;

    bool has_chest_waist_ratio;
    double chest_waist_ratio;

    bool has_arm_asymmetry;
    double arm_asymmetry_percent;

    bool has_forearm_asymmetry;
    double forearm_asymmetry_percent;

    bool has_thigh_asymmetry;
    double thigh_asymmetry_percent;

    bool has_calf_asymmetry;
    double calf_asymmetry_percent;
} TrainlogBodyAnalyticsResult;

bool trainlog_body_analytics_profile_valid(
    const TrainlogBodyAnalyticsProfile *profile
);

/**
 * @brief Calculate derived analytics from one real observation.
 *
 * If profile is NULL, profile-independent ratios and asymmetries are still
 * calculated. Body-fat estimation uses the circumference-based U.S. Navy
 * equations and is exposed strictly as an estimate.
 */
TrainlogStatus trainlog_body_analytics_calculate(
    const TrainlogBodyAnalyticsProfile *profile,
    const TrainlogBodyObservationRecord *record,
    TrainlogBodyAnalyticsResult *output
);

#endif
