/*
 * Trainlog local presentation preferences and translations.
 */
#ifndef TRAINLOG_PRESENTATION_H
#define TRAINLOG_PRESENTATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

typedef enum TrainlogPresentationLanguage {
    TRAINLOG_PRESENTATION_FRENCH = 0,
    TRAINLOG_PRESENTATION_ENGLISH
} TrainlogPresentationLanguage;

typedef enum TrainlogPresentationSaveResult {
    TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED = 0,
    TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABLE,
    TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABILITY_UNCERTAIN
} TrainlogPresentationSaveResult;

/* WHY: language is a local presentation choice, not workout data.
 * CONTRACT: init always selects French. load accepts only the versioned v1
 * file and leaves French selected when it is absent or invalid. Neither
 * function reads or mutates the canonical database.
 * INVARIANT: the only runtime values are French and English. */
void trainlog_presentation_init(void);
bool trainlog_presentation_load(void);
TrainlogPresentationSaveResult trainlog_presentation_save(void);
void trainlog_presentation_resolve_save(
    TrainlogPresentationLanguage previous_language,
    TrainlogPresentationSaveResult result);
TrainlogPresentationLanguage trainlog_presentation_language(void);
bool trainlog_presentation_set_language(TrainlogPresentationLanguage language);
const char *trainlog_presentation_language_code(
    TrainlogPresentationLanguage language);
const char *trainlog_presentation_language_name(
    TrainlogPresentationLanguage language);

/* WHY: stable keys keep screen code independent of wording and language.
 * CONTRACT: returned storage is static and must not be freed. Unknown/NULL
 * keys return a visible deterministic fallback and never user/catalog data.
 * INVARIANT: translations have no side effects. */
const char *trainlog_presentation_text(const char *key);

/* Transitional centralized lookup for presentation format strings. It exists
 * so large legacy renderers can cross one translation boundary without
 * language conditionals; new UI must use stable keys above. Unknown strings
 * are returned byte-for-byte, preserving user and protocol literals. */
const char *trainlog_presentation_source(const char *source);
const char *trainlog_presentation_month_label(const char *source,
    char *output, size_t output_size);

/* Format one translated presentation string into caller-owned storage. This
 * is the serialization-safe adapter used by strict renderer APIs: format
 * placeholders are identical in both catalog columns. */
int trainlog_presentation_vformat(char *output, size_t capacity,
    const char *source, va_list arguments);
int trainlog_presentation_format(char *output, size_t capacity,
    const char *source, ...);
/* Locale-neutral formatting for editable numeric seed text. This is not
 * presentation: both UI languages and every host locale receive ASCII '.'. */
int trainlog_presentation_format_input(char *output, size_t capacity,
    const char *format, ...) __attribute__((format(printf, 3, 4)));
int trainlog_presentation_format_key(char *output, size_t capacity,
    const char *key, ...);

/* Format YYYY-MM-DD as app-owned compact presentation text. Stored timestamp
 * bytes are borrowed and never changed; French uses DD/MM, English MM/DD. */
bool trainlog_presentation_compact_date(const char *iso_date, bool with_year,
    char *output, size_t capacity);

/* Exposed for focused persistence tests; normal callers use load/save. */
bool trainlog_presentation_config_path(char *output, size_t capacity);

#endif
