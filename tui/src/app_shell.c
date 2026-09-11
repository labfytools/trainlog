/**
 * @file app_shell.c
 * @brief Pure state machinery shared by the Notcurses application shell.
 */

#include "trainlog/app_shell.h"

#include <stdio.h>
#include <string.h>

#include <utf8proc.h>

#include "trainlog/terminal.h"

static void copy_id(char output[TRAINLOG_SHELL_STABLE_ID_CAPACITY],
                    const char *value)
{
    (void)snprintf(output, TRAINLOG_SHELL_STABLE_ID_CAPACITY, "%s",
                   value != NULL ? value : "");
}

void trainlog_shell_layout_compute(int columns, int rows,
                                   TrainlogShellLayout *layout)
{
    int content_x;
    if (layout == NULL) return;
    (void)memset(layout, 0, sizeof(*layout));
    layout->rows = rows;
    layout->columns = columns;
    layout->usable = columns >= TRAINLOG_SHELL_MIN_COLUMNS &&
        rows >= TRAINLOG_SHELL_MIN_ROWS;
    if (!layout->usable) return;

    /* CONTRACT: chrome owns four rows on every valid geometry. Content and
     * overlays can therefore never clip or overwrite the footer. */
    layout->header = (TrainlogRect){0, 0, 2, columns};
    layout->footer = (TrainlogRect){rows - 2, 0, 2, columns};
    layout->sidebar_visible = columns >= 100 && rows >= 26;
    layout->sidebar_expanded = columns >= 120 && rows >= 32;
    content_x = layout->sidebar_visible ? 23 : 0;
    if (layout->sidebar_visible) {
        layout->sidebar = (TrainlogRect){2, 0, rows - 4,
            TRAINLOG_SHELL_SIDEBAR_COLUMNS};
        layout->separator = (TrainlogRect){2, 22, rows - 4, 1};
    }
    layout->content = (TrainlogRect){2, content_x, rows - 4,
        columns - content_x};
}

TrainlogRect trainlog_shell_overlay_rect(const TrainlogShellLayout *layout,
                                         int preferred_width,
                                         int preferred_height)
{
    TrainlogRect result = {0, 0, 0, 0};
    int maximum_width;
    int maximum_height;
    if (layout == NULL || !layout->usable) return result;
    maximum_width = layout->columns >= 100 ? 80 : layout->columns;
    if (layout->columns == 100 && maximum_width > 74) maximum_width = 74;
    if (layout->columns == 80 && maximum_width > 78) maximum_width = 78;
    maximum_height = layout->content.height;
    if (layout->columns >= 120 && maximum_height > 29) maximum_height = 29;
    if (layout->columns == 100 && maximum_height > 24) maximum_height = 24;
    result.width = preferred_width > 0 && preferred_width < maximum_width
        ? preferred_width : maximum_width;
    result.height = preferred_height > 0 && preferred_height < maximum_height
        ? preferred_height : maximum_height;
    result.x = (layout->columns - result.width) / 2;
    result.y = layout->content.y +
        (layout->content.height - result.height) / 2;
    return result;
}

bool trainlog_shell_rect_contains(const TrainlogRect *outer,
                                  const TrainlogRect *inner)
{
    return outer != NULL && inner != NULL && inner->height >= 0 &&
        inner->width >= 0 && inner->y >= outer->y && inner->x >= outer->x &&
        inner->y + inner->height <= outer->y + outer->height &&
        inner->x + inner->width <= outer->x + outer->width;
}

void trainlog_navigation_init(TrainlogNavigationState *navigation)
{
    if (navigation == NULL) return;
    (void)memset(navigation, 0, sizeof(*navigation));
    navigation->current.route = TRAINLOG_ROUTE_HOME;
}

bool trainlog_navigation_open(TrainlogNavigationState *navigation,
                              TrainlogAppRoute route,
                              const char *stable_id)
{
    if (navigation == NULL) return false;
    if (navigation->current.route == route &&
        strcmp(navigation->current.stable_id, stable_id != NULL ? stable_id : "") == 0)
        return true;
    if (navigation->back_count == TRAINLOG_SHELL_ROUTE_DEPTH) {
        (void)memmove(&navigation->back[0], &navigation->back[1],
            (TRAINLOG_SHELL_ROUTE_DEPTH - 1U) * sizeof(navigation->back[0]));
        --navigation->back_count;
    }
    navigation->back[navigation->back_count++] = navigation->current;
    navigation->current.route = route;
    copy_id(navigation->current.stable_id, stable_id);
    return true;
}

bool trainlog_navigation_back(TrainlogNavigationState *navigation)
{
    if (navigation == NULL || navigation->back_count == 0U) return false;
    navigation->current = navigation->back[--navigation->back_count];
    return true;
}

void trainlog_actions_clear(TrainlogActionModel *model)
{
    if (model != NULL) (void)memset(model, 0, sizeof(*model));
}

bool trainlog_actions_add(TrainlogActionModel *model,
                          const TrainlogAction *action)
{
    size_t index;
    if (model == NULL || action == NULL || action->identifier == NULL ||
        action->label == NULL || model->count >= TRAINLOG_SHELL_ACTION_LIMIT)
        return false;
    /* INVARIANT: identifier is the action's stable identity. Keeping it unique
     * prevents the footer and F7 registry from advertising the same semantic
     * action twice when contextual and shell-wide contributors overlap. */
    for (index = 0U; index < model->count; ++index)
        if (strcmp(model->items[index].identifier, action->identifier) == 0)
            return false;
    model->items[model->count++] = *action;
    return true;
}

const TrainlogAction *trainlog_actions_find_key(const TrainlogActionModel *model,
                                                int key)
{
    size_t index;
    if (model == NULL) return NULL;
    for (index = 0U; index < model->count; ++index) {
        if (model->items[index].enabled && model->items[index].key == key)
            return &model->items[index];
    }
    return NULL;
}

const TrainlogAction *trainlog_actions_at_priority(const TrainlogActionModel *model,
                                                   size_t position)
{
    const TrainlogAction *result = NULL;
    size_t rank;
    size_t index;
    if (model == NULL) return NULL;
    for (rank = 0U; rank <= position; ++rank) {
        result = NULL;
        for (index = 0U; index < model->count; ++index) {
            const TrainlogAction *candidate = &model->items[index];
            size_t prior = 0U;
            size_t other;
            if (!candidate->enabled) continue;
            for (other = 0U; other < model->count; ++other) {
                if (!model->items[other].enabled) continue;
                if (model->items[other].priority < candidate->priority ||
                    (model->items[other].priority == candidate->priority &&
                     other < index)) ++prior;
            }
            if (prior == rank) { result = candidate; break; }
        }
        if (result == NULL || rank == position) break;
    }
    return result;
}

void trainlog_overlays_init(TrainlogOverlayStack *stack)
{
    if (stack != NULL) (void)memset(stack, 0, sizeof(*stack));
}

bool trainlog_overlays_push(TrainlogOverlayStack *stack,
                            TrainlogOverlayType type,
                            TrainlogFocusTarget restore_focus,
                            const char *restore_stable_id)
{
    TrainlogOverlay *overlay;
    if (stack == NULL || stack->count >= TRAINLOG_SHELL_OVERLAY_LIMIT) return false;
    overlay = &stack->items[stack->count++];
    (void)memset(overlay, 0, sizeof(*overlay));
    overlay->type = type;
    overlay->restore_focus = restore_focus;
    copy_id(overlay->restore_stable_id, restore_stable_id);
    return true;
}

const TrainlogOverlay *trainlog_overlays_top(const TrainlogOverlayStack *stack)
{
    return stack != NULL && stack->count > 0U
        ? &stack->items[stack->count - 1U] : NULL;
}

bool trainlog_overlays_pop(TrainlogOverlayStack *stack,
                           TrainlogFocusTarget *restore_focus,
                           char restore_stable_id[TRAINLOG_SHELL_STABLE_ID_CAPACITY])
{
    TrainlogOverlay *overlay;
    if (stack == NULL || stack->count == 0U) return false;
    overlay = &stack->items[--stack->count];
    if (restore_focus != NULL) *restore_focus = overlay->restore_focus;
    if (restore_stable_id != NULL)
        copy_id(restore_stable_id, overlay->restore_stable_id);
    (void)memset(overlay, 0, sizeof(*overlay));
    return true;
}

void trainlog_list_init(TrainlogListState *list)
{
    if (list != NULL) (void)memset(list, 0, sizeof(*list));
}

void trainlog_list_set_items(TrainlogListState *list,
                             const char *const *stable_ids,
                             size_t count,
                             size_t visible_rows,
                             bool total_known,
                             bool more_available)
{
    size_t index;
    size_t selected = 0U;
    bool found = false;
    if (list == NULL) return;
    for (index = 0U; index < count && stable_ids != NULL; ++index) {
        if (strcmp(list->selected_id, stable_ids[index]) == 0) {
            selected = index; found = true; break;
        }
    }
    if (!found && count > 0U) {
        selected = list->selected_index < count ? list->selected_index : count - 1U;
        copy_id(list->selected_id, stable_ids[selected]);
    } else if (count == 0U) {
        list->selected_id[0] = '\0'; selected = 0U;
    }
    list->selected_index = selected;
    list->item_count = count;
    list->visible_rows = visible_rows > 0U ? visible_rows : 1U;
    list->total_known = total_known;
    list->more_available = more_available;
    if (selected < list->viewport_start) list->viewport_start = selected;
    if (selected >= list->viewport_start + list->visible_rows)
        list->viewport_start = selected - list->visible_rows + 1U;
    if (count <= list->visible_rows) list->viewport_start = 0U;
    else if (list->viewport_start > count - list->visible_rows)
        list->viewport_start = count - list->visible_rows;
}

void trainlog_list_move(TrainlogListState *list,
                        const char *const *stable_ids,
                        int delta)
{
    size_t next;
    if (list == NULL || stable_ids == NULL || list->item_count == 0U) return;
    if (delta < 0) {
        size_t amount = (size_t)(-(long long)delta);
        next = amount > list->selected_index ? 0U : list->selected_index - amount;
    } else {
        size_t amount = (size_t)delta;
        next = amount > list->item_count - 1U - list->selected_index
            ? list->item_count - 1U : list->selected_index + amount;
    }
    list->selected_index = next;
    copy_id(list->selected_id, stable_ids[next]);
    if (next < list->viewport_start) list->viewport_start = next;
    if (next >= list->viewport_start + list->visible_rows)
        list->viewport_start = next - list->visible_rows + 1U;
}

void trainlog_shell_format_list_label(const char *value,
                                      int maximum_cells,
                                      char *output,
                                      size_t output_capacity)
{
    const char *input = value != NULL ? value : "";
    size_t input_bytes = 0U;
    size_t output_bytes = 0U;
    int cells = 0;
    bool truncated = false;

    if (output == NULL || output_capacity == 0U) return;
    output[0] = '\0';
    if (maximum_cells <= 0) return;

    /* INVARIANT: reserve one display cell for the truncation marker before
     * copying a code point.  This keeps labels valid UTF-8 and makes clipped
     * stable-ID-backed rows visibly distinct from complete names. */
    while (input[input_bytes] != '\0') {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t parsed = utf8proc_iterate(
            (const utf8proc_uint8_t *)input + input_bytes, -1, &codepoint);
        int width;
        if (parsed <= 0) { parsed = 1; codepoint = (unsigned char)input[input_bytes]; }
        width = utf8proc_charwidth(codepoint);
        if (width < 0) width = 1;
        if (cells + width > maximum_cells ||
            output_bytes + (size_t)parsed >= output_capacity) {
            truncated = true;
            break;
        }
        (void)memcpy(output + output_bytes, input + input_bytes, (size_t)parsed);
        output_bytes += (size_t)parsed;
        input_bytes += (size_t)parsed;
        cells += width;
    }
    if (input[input_bytes] != '\0') truncated = true;
    if (truncated) {
        static const char ellipsis[] = "…";
        while (output_bytes > 0U && cells >= maximum_cells) {
            utf8proc_int32_t codepoint;
            size_t start = output_bytes - 1U;
            while (start > 0U && ((unsigned char)output[start] & 0xc0U) == 0x80U)
                --start;
            if (utf8proc_iterate((const utf8proc_uint8_t *)output + start,
                    (utf8proc_ssize_t)(output_bytes - start), &codepoint) > 0) {
                int width = utf8proc_charwidth(codepoint);
                cells -= width > 0 ? width : 1;
            }
            output_bytes = start;
        }
        if (output_bytes + sizeof(ellipsis) <= output_capacity) {
            (void)memcpy(output + output_bytes, ellipsis, sizeof(ellipsis));
            return;
        }
    }
    output[output_bytes] = '\0';
}

void trainlog_search_init(TrainlogSearchState *search)
{
    if (search == NULL) return;
    (void)memset(search, 0, sizeof(*search));
}

bool trainlog_search_insert(TrainlogSearchState *search,
                            const char *utf8,
                            size_t length)
{
    utf8proc_int32_t codepoint;
    utf8proc_ssize_t parsed;
    if (search == NULL || utf8 == NULL || length == 0U ||
        search->bytes + length >= TRAINLOG_SHELL_SEARCH_CAPACITY) {
        if (search != NULL) search->capacity_error = true;
        return false;
    }
    parsed = utf8proc_iterate((const utf8proc_uint8_t *)utf8,
                              (utf8proc_ssize_t)length, &codepoint);
    if (parsed <= 0 || (size_t)parsed != length || codepoint < 0x20 ||
        codepoint == 0x7f) return false;
    (void)memmove(search->text + search->cursor + length,
        search->text + search->cursor, search->bytes - search->cursor + 1U);
    (void)memcpy(search->text + search->cursor, utf8, length);
    search->cursor += length;
    search->bytes += length;
    search->capacity_error = false;
    return true;
}

static size_t next_grapheme_boundary(const char *text, size_t bytes, size_t start)
{
    utf8proc_int32_t previous;
    utf8proc_ssize_t parsed;
    size_t cursor = start;
    utf8proc_int32_t state = 0;
    if (text == NULL || start >= bytes) return bytes;
    parsed = utf8proc_iterate((const utf8proc_uint8_t *)text + cursor,
        (utf8proc_ssize_t)(bytes - cursor), &previous);
    if (parsed <= 0) return start + 1U;
    cursor += (size_t)parsed;
    while (cursor < bytes) {
        utf8proc_int32_t current;
        parsed = utf8proc_iterate((const utf8proc_uint8_t *)text + cursor,
            (utf8proc_ssize_t)(bytes - cursor), &current);
        if (parsed <= 0 || utf8proc_grapheme_break_stateful(previous, current,
            &state) != 0) break;
        previous = current;
        cursor += (size_t)parsed;
    }
    return cursor;
}

static size_t previous_grapheme_boundary(const char *text, size_t bytes,
                                         size_t cursor)
{
    size_t previous = 0U;
    size_t next = 0U;
    while (next < cursor && next < bytes) {
        previous = next;
        next = next_grapheme_boundary(text, bytes, next);
    }
    return previous;
}

bool trainlog_search_backspace(TrainlogSearchState *search)
{
    size_t start;
    if (search == NULL || search->cursor == 0U) return false;
    start = previous_grapheme_boundary(search->text, search->bytes,
        search->cursor);
    (void)memmove(search->text + start, search->text + search->cursor,
        search->bytes - search->cursor + 1U);
    search->bytes -= search->cursor - start;
    search->cursor = start;
    search->capacity_error = false;
    return true;
}

void trainlog_search_home(TrainlogSearchState *search)
{
    if (search != NULL) search->cursor = 0U;
}

void trainlog_search_end(TrainlogSearchState *search)
{
    if (search != NULL) search->cursor = search->bytes;
}

void trainlog_search_left(TrainlogSearchState *search)
{
    if (search == NULL || search->cursor == 0U) return;
    search->cursor = previous_grapheme_boundary(search->text, search->bytes,
        search->cursor);
}

void trainlog_search_right(TrainlogSearchState *search)
{
    if (search == NULL || search->cursor >= search->bytes) return;
    search->cursor = next_grapheme_boundary(search->text, search->bytes,
        search->cursor);
}

bool trainlog_search_escape(TrainlogSearchState *search)
{
    if (search == NULL) return false;
    if (search->bytes > 0U) {
        search->text[0] = '\0'; search->bytes = 0U; search->cursor = 0U;
        search->capacity_error = false; return false;
    }
    search->open = false; search->focused = false; return true;
}

void trainlog_form_init(TrainlogFormField *field, const char *initial_value)
{
    size_t length;
    if (field == NULL) return;
    (void)memset(field, 0, sizeof(*field));
    length = initial_value != NULL ? strlen(initial_value) : 0U;
    if (length > TRAINLOG_SHELL_SEARCH_CAPACITY - 1U)
        length = TRAINLOG_SHELL_SEARCH_CAPACITY - 1U;
    if (length > 0U) (void)memcpy(field->text, initial_value, length);
    field->text[length] = '\0';
    field->bytes = length;
    field->cursor = length;
    field->active = true;
}

TrainlogFormResult trainlog_form_handle(TrainlogFormField *field, int key)
{
    TrainlogSearchState editor;
    bool changed = false;
    if (field == NULL || !field->active) return TRAINLOG_FORM_IGNORED;
    if (key == TRAINLOG_KEY_ENTER || key == '\n') return TRAINLOG_FORM_SUBMIT;
    if (key == TRAINLOG_KEY_ESCAPE || key == 27) return TRAINLOG_FORM_CANCEL;
    if (key == TRAINLOG_KEY_TAB) return TRAINLOG_FORM_NEXT;
    if (key == TRAINLOG_KEY_SHIFT_TAB) return TRAINLOG_FORM_PREVIOUS;
    if (key == TRAINLOG_KEY_F6) return TRAINLOG_FORM_OPEN_NAVIGATION;
    if (key == TRAINLOG_KEY_F7) return TRAINLOG_FORM_OPEN_ACTIONS;
    (void)memset(&editor, 0, sizeof(editor));
    (void)snprintf(editor.text, sizeof(editor.text), "%s", field->text);
    editor.bytes = field->bytes;
    editor.cursor = field->cursor;
    editor.open = true;
    editor.focused = true;
    if (key == TRAINLOG_KEY_HOME) trainlog_search_home(&editor);
    else if (key == TRAINLOG_KEY_END) trainlog_search_end(&editor);
    else if (key == TRAINLOG_KEY_LEFT) trainlog_search_left(&editor);
    else if (key == TRAINLOG_KEY_RIGHT) trainlog_search_right(&editor);
    else if (key == TRAINLOG_KEY_BACKSPACE || key == TRAINLOG_KEY_DELETE)
        changed = trainlog_search_backspace(&editor);
    else if (key >= 0x20 && key <= 0x10ffff) {
        char encoded[5] = "";
        utf8proc_ssize_t bytes = utf8proc_encode_char((utf8proc_int32_t)key,
            (utf8proc_uint8_t *)encoded);
        if (bytes > 0) changed = trainlog_search_insert(&editor, encoded,
            (size_t)bytes);
    } else return TRAINLOG_FORM_IGNORED;
    (void)snprintf(field->text, sizeof(field->text), "%s", editor.text);
    field->bytes = editor.bytes;
    field->cursor = editor.cursor;
    field->capacity_error = editor.capacity_error;
    return changed ? TRAINLOG_FORM_EDITED : TRAINLOG_FORM_IGNORED;
}

const char *trainlog_route_title(TrainlogAppRoute route)
{
    switch (route) {
    case TRAINLOG_ROUTE_HOME: return "Accueil";
    case TRAINLOG_ROUTE_SESSIONS: return "Séances";
    case TRAINLOG_ROUTE_SESSION_CURRENT: return "Séances / Séance en cours";
    case TRAINLOG_ROUTE_SESSION_GENERATOR: return "Séances / Programmer";
    case TRAINLOG_ROUTE_SESSION_MANUAL: return "Séances / Nouvelle séance";
    case TRAINLOG_ROUTE_SESSIONS_COMPLETED: return "Séances / Effectuées";
    case TRAINLOG_ROUTE_SESSION_DETAIL: return "Séances / Détail";
    case TRAINLOG_ROUTE_EXERCISES: return "Exercices / Catalogue";
    case TRAINLOG_ROUTE_EXERCISE_DETAIL: return "Exercices / Fiche";
    case TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE: return "Exercices / Connaissances";
    case TRAINLOG_ROUTE_EXERCISE_PERFORMANCE: return "Statistiques / Performance";
    case TRAINLOG_ROUTE_EXERCISE_MAX: return "Statistiques / MAX mesuré";
    case TRAINLOG_ROUTE_EQUIPMENT: return "Équipements / Catalogue";
    case TRAINLOG_ROUTE_EQUIPMENT_DETAIL: return "Équipements / Fiche";
    case TRAINLOG_ROUTE_STATS: return "Statistiques";
    case TRAINLOG_ROUTE_STATS_EXERCISE: return "Statistiques / Par exercice";
    case TRAINLOG_ROUTE_BODY: return "Statistiques / Mensurations";
    case TRAINLOG_ROUTE_BODY_DETAIL: return "Mensurations / Relevé";
    case TRAINLOG_ROUTE_BODY_METRIC: return "Mensurations / Historique";
    case TRAINLOG_ROUTE_BODY_TRENDS: return "Mensurations / 12 mois";
    case TRAINLOG_ROUTE_BODY_GLOBAL: return "Mensurations / Vue globale";
    case TRAINLOG_ROUTE_BODY_ANALYTICS: return "Mensurations / Analyse";
    case TRAINLOG_ROUTE_MAX: return "Statistiques / Capacités MAX";
    case TRAINLOG_ROUTE_SYNC: return "Synchronisation";
    case TRAINLOG_ROUTE_SETTINGS: return "Paramètres";
    default: return "Trainlog";
    }
}

TrainlogAppRoute trainlog_route_section(TrainlogAppRoute route)
{
    if (route >= TRAINLOG_ROUTE_SESSIONS && route <= TRAINLOG_ROUTE_SESSION_DETAIL)
        return TRAINLOG_ROUTE_SESSIONS;
    if (route == TRAINLOG_ROUTE_EXERCISE_DETAIL ||
        route == TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE) return TRAINLOG_ROUTE_EXERCISES;
    if (route == TRAINLOG_ROUTE_EXERCISE_PERFORMANCE ||
        route == TRAINLOG_ROUTE_EXERCISE_MAX) return TRAINLOG_ROUTE_STATS;
    if (route == TRAINLOG_ROUTE_EQUIPMENT_DETAIL) return TRAINLOG_ROUTE_EQUIPMENT;
    if (route == TRAINLOG_ROUTE_STATS_EXERCISE ||
        (route >= TRAINLOG_ROUTE_BODY && route <= TRAINLOG_ROUTE_BODY_ANALYTICS) ||
        route == TRAINLOG_ROUTE_MAX) return TRAINLOG_ROUTE_STATS;
    return route;
}

TrainlogLeaveDecision trainlog_shell_leave_decision(
    const TrainlogDurabilityState *state,
    bool explicit_discard)
{
    if (state == NULL) return TRAINLOG_LEAVE_ALLOW;
    if (explicit_discard) return state->session_draft || state->session_dirty ||
        state->generator_configuration_dirty || state->generator_preview ||
        state->generator_preview_dirty || state->transient_form_dirty
            ? TRAINLOG_LEAVE_CONFIRM_DISCARD : TRAINLOG_LEAVE_ALLOW;
    if (state->generator_preview || state->generator_preview_dirty ||
        state->generator_configuration_dirty || state->transient_form_dirty)
        return TRAINLOG_LEAVE_CONFIRM_KEEP;
    /* A session draft is owned in memory by the run and ordinary navigation
     * suspends it. Only q/discard asks to erase it. */
    return TRAINLOG_LEAVE_ALLOW;
}

void trainlog_shell_discard_transient(TrainlogDurabilityState *state)
{
    if (state != NULL) (void)memset(state, 0, sizeof(*state));
}
