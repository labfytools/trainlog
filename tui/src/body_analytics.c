/**
 * @file body_analytics.c
 * @brief Derived body analytics implementation.
 */

#include "trainlog/body_analytics.h"

#include <math.h>
#include <string.h>

#define CM_PER_INCH 2.54

static bool positive_finite(
    double value
)
{
    return
        isfinite(value) &&
        value > 0.0;
}

static bool body_ratio(
    double numerator,
    double denominator,
    double *output
)
{
    if (
        output == NULL ||
        !positive_finite(numerator) ||
        !positive_finite(denominator)
    ) {
        return false;
    }

    *output =
        numerator /
        denominator;

    return isfinite(*output);
}

static bool body_asymmetry(
    double left,
    double right,
    double *output_percent
)
{
    double average;

    if (
        output_percent == NULL ||
        !positive_finite(left) ||
        !positive_finite(right)
    ) {
        return false;
    }

    average =
        (left + right) /
        2.0;

    if (!positive_finite(average)) {
        return false;
    }

    *output_percent =
        fabs(left - right) /
        average *
        100.0;

    return
        isfinite(
            *output_percent
        );
}

bool trainlog_body_analytics_profile_valid(
    const TrainlogBodyAnalyticsProfile *profile
)
{
    if (profile == NULL) {
        return false;
    }

    if (
        profile->formula !=
            TRAINLOG_BODY_ANALYTICS_FORMULA_MALE &&
        profile->formula !=
            TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
    ) {
        return false;
    }

    return
        positive_finite(
            profile->height_cm
        ) &&
        profile->height_cm >= 100.0 &&
        profile->height_cm <= 250.0;
}

static bool body_fat_estimate(
    const TrainlogBodyAnalyticsProfile *profile,
    const TrainlogBodyObservationRecord *record,
    double *output_percent
)
{
    double height_in;
    double neck_in;
    double waist_in;
    double body_fat;

    if (
        !trainlog_body_analytics_profile_valid(
            profile
        ) ||
        record == NULL ||
        output_percent == NULL ||
        !record->has_neck ||
        !record->has_waist ||
        !positive_finite(
            record->neck_cm
        ) ||
        !positive_finite(
            record->waist_cm
        )
    ) {
        return false;
    }

    height_in =
        profile->height_cm /
        CM_PER_INCH;

    neck_in =
        record->neck_cm /
        CM_PER_INCH;

    waist_in =
        record->waist_cm /
        CM_PER_INCH;

    if (
        profile->formula ==
        TRAINLOG_BODY_ANALYTICS_FORMULA_MALE
    ) {
        double circumference =
            waist_in -
            neck_in;

        if (
            !positive_finite(
                circumference
            )
        ) {
            return false;
        }

        body_fat =
            86.010 *
                log10(
                    circumference
                ) -
            70.041 *
                log10(
                    height_in
                ) +
            36.76;
    } else {
        double hips_in;
        double circumference;

        if (
            !record->has_hips ||
            !positive_finite(
                record->hips_cm
            )
        ) {
            return false;
        }

        hips_in =
            record->hips_cm /
            CM_PER_INCH;

        circumference =
            waist_in +
            hips_in -
            neck_in;

        if (
            !positive_finite(
                circumference
            )
        ) {
            return false;
        }

        body_fat =
            163.205 *
                log10(
                    circumference
                ) -
            97.684 *
                log10(
                    height_in
                ) -
            78.387;
    }

    if (
        !isfinite(body_fat) ||
        body_fat <= 0.0 ||
        body_fat >= 80.0
    ) {
        return false;
    }

    *output_percent =
        body_fat;

    return true;
}

TrainlogStatus trainlog_body_analytics_calculate(
    const TrainlogBodyAnalyticsProfile *profile,
    const TrainlogBodyObservationRecord *record,
    TrainlogBodyAnalyticsResult *output
)
{
    if (
        record == NULL ||
        output == NULL ||
        (
            profile != NULL &&
            !trainlog_body_analytics_profile_valid(
                profile
            )
        )
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(
        output,
        0,
        sizeof(*output)
    );

    if (
        profile != NULL &&
        body_fat_estimate(
            profile,
            record,
            &output->body_fat_percent
        )
    ) {
        output->has_body_fat_estimate =
            true;

        if (
            record->has_body_weight &&
            positive_finite(
                record->body_weight_kg
            )
        ) {
            output->fat_mass_kg =
                record->body_weight_kg *
                output->body_fat_percent /
                100.0;

            output->lean_mass_kg =
                record->body_weight_kg -
                output->fat_mass_kg;

            if (
                isfinite(
                    output->fat_mass_kg
                ) &&
                isfinite(
                    output->lean_mass_kg
                ) &&
                output->fat_mass_kg > 0.0 &&
                output->lean_mass_kg > 0.0
            ) {
                output->has_fat_mass_estimate =
                    true;

                output->has_lean_mass_estimate =
                    true;
            }
        }
    }

    if (
        record->has_waist &&
        record->has_hips &&
        body_ratio(
            record->waist_cm,
            record->hips_cm,
            &output->waist_hip_ratio
        )
    ) {
        output->has_waist_hip_ratio =
            true;
    }

    if (
        record->has_shoulders &&
        record->has_waist &&
        body_ratio(
            record->shoulders_cm,
            record->waist_cm,
            &output->shoulder_waist_ratio
        )
    ) {
        output->has_shoulder_waist_ratio =
            true;
    }

    if (
        record->has_chest &&
        record->has_waist &&
        body_ratio(
            record->chest_cm,
            record->waist_cm,
            &output->chest_waist_ratio
        )
    ) {
        output->has_chest_waist_ratio =
            true;
    }

    if (
        record->has_left_arm &&
        record->has_right_arm &&
        body_asymmetry(
            record->left_arm_cm,
            record->right_arm_cm,
            &output->arm_asymmetry_percent
        )
    ) {
        output->has_arm_asymmetry =
            true;
    }

    if (
        record->has_left_forearm &&
        record->has_right_forearm &&
        body_asymmetry(
            record->left_forearm_cm,
            record->right_forearm_cm,
            &output->forearm_asymmetry_percent
        )
    ) {
        output->has_forearm_asymmetry =
            true;
    }

    if (
        record->has_left_thigh &&
        record->has_right_thigh &&
        body_asymmetry(
            record->left_thigh_cm,
            record->right_thigh_cm,
            &output->thigh_asymmetry_percent
        )
    ) {
        output->has_thigh_asymmetry =
            true;
    }

    if (
        record->has_left_calf &&
        record->has_right_calf &&
        body_asymmetry(
            record->left_calf_cm,
            record->right_calf_cm,
            &output->calf_asymmetry_percent
        )
    ) {
        output->has_calf_asymmetry =
            true;
    }

    return TRAINLOG_STATUS_OK;
}
