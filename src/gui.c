#include "gui.h"
#include "config.h"
#include <ctype.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static struct {
    char *input_text;
    TranslationResponse *response;
    GtkWidget *content_box;
    GtkWidget *source_combo;
    GtkWidget *target_combo;
    GtkWidget *text_entry;
    GtkWidget *translate_btn;
    GtkWidget *examples_header;
    GtkWidget *examples_grid;
    GtkWidget *option_buttons[64];
    GtkWidget *window;
    int num_buttons;
    int built;
    int toggling;
    int suppressing;
    int busy;
} ctx;

typedef struct {
    char *text;
    char *source_lang;
    char *target_lang;
    TranslationResponse *response;
} TranslateJob;

typedef struct {
    char *text;
    char *source_lang;
    char *target_lang;
    char *fragment;
    ContextExamples *result;
    int option_idx;
} ContextJob;

static int is_short_text(const char *text) {
    if (!text || !*text) return 0;
    int words = 0, in_word = 0, punct = 0;
    for (const char *p = text; *p; p++) {
        if (isspace((unsigned char)*p)) {
            in_word = 0;
        } else {
            if (!in_word) words++;
            in_word = 1;
            if (ispunct((unsigned char)*p)) punct++;
        }
    }
    return words > 0 && words <= 3 && punct <= 1;
}

static void set_combo_active(GtkComboBox *combo, const char *lang) {
    GtkTreeModel *model = gtk_combo_box_get_model(combo);
    GtkTreeIter iter;
    int idx = 0;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;
    do {
        char *val;
        gtk_tree_model_get(model, &iter, 0, &val, -1);
        if (val && strcasecmp(val, lang) == 0) {
            gtk_combo_box_set_active(combo, idx);
            g_free(val);
            return;
        }
        g_free(val);
        idx++;
    } while (gtk_tree_model_iter_next(model, &iter));
}

static void rebuild_all_examples(void) {
    gtk_label_set_text(GTK_LABEL(ctx.examples_header), "Examples");
    GList *child = gtk_container_get_children(GTK_CONTAINER(ctx.examples_grid));
    for (GList *l = child; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(child);

    int row = 0;
    for (int o = 0; o < ctx.response->num_options; o++) {
        TranslationOption *opt = &ctx.response->options[o];
        for (int i = 0; i < opt->num_examples; i++) {
            char buf[4096];
            snprintf(buf, sizeof(buf), "\xe2\x80\xa2 %s",
                opt->source_examples[i] ? opt->source_examples[i] : "");
            GtkWidget *sl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(sl), buf);
            gtk_label_set_xalign(GTK_LABEL(sl), 0.0);
            gtk_label_set_line_wrap(GTK_LABEL(sl), TRUE);
            gtk_grid_attach(GTK_GRID(ctx.examples_grid), sl, 0, row, 1, 1);

            snprintf(buf, sizeof(buf), "\xe2\x80\xa2 %s",
                opt->target_examples[i] ? opt->target_examples[i] : "");
            GtkWidget *tl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(tl), buf);
            gtk_label_set_xalign(GTK_LABEL(tl), 0.0);
            gtk_label_set_line_wrap(GTK_LABEL(tl), TRUE);
            gtk_grid_attach(GTK_GRID(ctx.examples_grid), tl, 1, row, 1, 1);
            row++;
        }
    }
    gtk_widget_show_all(ctx.examples_grid);
}

static gboolean on_context_fetched(gpointer data) {
    ContextJob *job = data;
    if (job->option_idx < 0 || !ctx.response || job->option_idx >= ctx.response->num_options) {
        free_context_examples(job->result);
        goto cleanup;
    }

    TranslationOption *opt = &ctx.response->options[job->option_idx];
    char header[256];
    snprintf(header, sizeof(header), "Examples for: %s", opt->translation ? opt->translation : "?");
    gtk_label_set_text(GTK_LABEL(ctx.examples_header), header);

    GList *child = gtk_container_get_children(GTK_CONTAINER(ctx.examples_grid));
    for (GList *l = child; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(child);

    ContextExamples *fetched = job->result;
    if (fetched && fetched->num_examples > 0) {
        GtkWidget *src_hdr = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(src_hdr), "<b>Source</b>");
        gtk_label_set_xalign(GTK_LABEL(src_hdr), 0.0);
        gtk_grid_attach(GTK_GRID(ctx.examples_grid), src_hdr, 0, 0, 1, 1);

        GtkWidget *tgt_hdr = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(tgt_hdr), "<b>Translation</b>");
        gtk_label_set_xalign(GTK_LABEL(tgt_hdr), 0.0);
        gtk_grid_attach(GTK_GRID(ctx.examples_grid), tgt_hdr, 1, 0, 1, 1);

        for (int i = 0; i < fetched->num_examples; i++) {
            char buf[4096];
            snprintf(buf, sizeof(buf), "\xe2\x80\xa2 %s",
                fetched->source_examples[i] ? fetched->source_examples[i] : "");
            GtkWidget *sl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(sl), buf);
            gtk_label_set_xalign(GTK_LABEL(sl), 0.0);
            gtk_label_set_line_wrap(GTK_LABEL(sl), TRUE);
            gtk_grid_attach(GTK_GRID(ctx.examples_grid), sl, 0, i + 1, 1, 1);

            snprintf(buf, sizeof(buf), "\xe2\x80\xa2 %s",
                fetched->target_examples[i] ? fetched->target_examples[i] : "");
            GtkWidget *tl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(tl), buf);
            gtk_label_set_xalign(GTK_LABEL(tl), 0.0);
            gtk_label_set_line_wrap(GTK_LABEL(tl), TRUE);
            gtk_grid_attach(GTK_GRID(ctx.examples_grid), tl, 1, i + 1, 1, 1);
        }
    } else {
        const char *err_msg = fetched && fetched->error
                                  ? fetched->error
                                  : "Failed to fetch examples from context.reverso.net";
        char markup[512];
        snprintf(markup, sizeof(markup), "<span color='red'>%s</span>", err_msg);
        GtkWidget *err = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(err), markup);
        gtk_label_set_xalign(GTK_LABEL(err), 0.0);
        gtk_label_set_line_wrap(GTK_LABEL(err), TRUE);
        gtk_grid_attach(GTK_GRID(ctx.examples_grid), err, 0, 0, 2, 1);
    }

    gtk_widget_show_all(ctx.examples_grid);

    free_context_examples(job->result);

cleanup:
    free(job->text);
    free(job->source_lang);
    free(job->target_lang);
    free(job->fragment);
    g_free(job);
    return FALSE;
}

static gpointer context_thread(gpointer data) {
    ContextJob *job = data;
    job->result =
        fetch_context_examples(job->text, job->source_lang, job->target_lang, job->fragment);
    g_idle_add(on_context_fetched, job);
    return NULL;
}

static void rebuild_examples_async(int option_idx) {
    if (!ctx.response || option_idx < 0 || option_idx >= ctx.response->num_options) return;

    TranslationOption *opt = &ctx.response->options[option_idx];
    char header[256];
    snprintf(header, sizeof(header), "Examples for: %s", opt->translation ? opt->translation : "?");
    gtk_label_set_text(GTK_LABEL(ctx.examples_header), header);

    GList *child = gtk_container_get_children(GTK_CONTAINER(ctx.examples_grid));
    for (GList *l = child; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(child);

    char *src_lang = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.source_combo));
    char *tgt_lang = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.target_combo));
    if (!src_lang || !tgt_lang || !ctx.input_text) {
        g_free(src_lang);
        g_free(tgt_lang);
        return;
    }

    ContextJob *job = g_new(ContextJob, 1);
    job->text = strdup(ctx.input_text);
    job->source_lang = strdup(src_lang);
    job->target_lang = strdup(tgt_lang);
    job->fragment = opt->translation ? strdup(opt->translation) : strdup("");
    job->result = NULL;
    job->option_idx = option_idx;
    g_free(src_lang);
    g_free(tgt_lang);

    g_thread_new("context-fetch", context_thread, job);
}

static void on_option_toggled(GtkToggleButton *btn, gpointer user_data) {
    (void)user_data;
    if (ctx.toggling) return;

    if (gtk_toggle_button_get_active(btn)) {
        ctx.toggling = 1;
        int idx = -1;
        for (int i = 0; i < ctx.num_buttons; i++) {
            if (ctx.option_buttons[i] == GTK_WIDGET(btn)) idx = i;
            else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx.option_buttons[i]), FALSE);
        }
        ctx.toggling = 0;
        if (idx >= 0) rebuild_examples_async(idx);
    } else {
        rebuild_all_examples();
    }
}

static void remove_all_children(GtkWidget *box) {
    GList *child = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *l = child; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(child);
}

static void build_ui(void) {
    if (!ctx.response) return;
    ctx.num_buttons = 0;
    remove_all_children(ctx.content_box);

    int short_mode = is_short_text(ctx.input_text);
    int show_options = short_mode && ctx.response->num_options > 1;

    if (show_options) {
        GtkWidget *flowbox = gtk_flow_box_new();
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 6);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flowbox), TRUE);
        gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
        gtk_widget_set_margin_bottom(flowbox, 4);

        for (int i = 0; i < ctx.response->num_options && ctx.num_buttons < 64; i++) {
            TranslationOption *opt = &ctx.response->options[i];
            if (!opt->translation || !*opt->translation) continue;

            char label[128];
            int cnt = opt->frequency > 0 ? opt->frequency : opt->occurrence_count;
            if (cnt > 0) snprintf(label, sizeof(label), "%s [%d]", opt->translation, cnt);
            else snprintf(label, sizeof(label), "%s", opt->translation);

            GtkWidget *btn = gtk_toggle_button_new_with_label(label);
            gtk_widget_set_tooltip_text(btn, "Click to see examples");
            gtk_container_add(GTK_CONTAINER(flowbox), btn);
            ctx.option_buttons[ctx.num_buttons++] = btn;
            g_signal_connect(btn, "toggled", G_CALLBACK(on_option_toggled), NULL);
        }

        gtk_box_pack_start(GTK_BOX(ctx.content_box), flowbox, FALSE, FALSE, 0);
    } else {
        char *text = NULL;
        if (ctx.response->num_translations > 0) {
            size_t len = 1;
            for (int i = 0; i < ctx.response->num_translations && i < 3; i++)
                len += strlen(ctx.response->translations[i]) + 4;
            text = calloc(1, len);
            if (text) {
                for (int i = 0; i < ctx.response->num_translations && i < 3; i++) {
                    if (i > 0) strcat(text, "\n");
                    strcat(text, "\xe2\x80\xa2 ");
                    strcat(text, ctx.response->translations[i]);
                }
            }
        }
        GtkWidget *trans_lbl = gtk_label_new(text ? text : "No translation");
        free(text);
        gtk_label_set_xalign(GTK_LABEL(trans_lbl), 0.0);
        gtk_label_set_line_wrap(GTK_LABEL(trans_lbl), TRUE);
        gtk_widget_set_margin_bottom(trans_lbl, 4);
        gtk_box_pack_start(GTK_BOX(ctx.content_box), trans_lbl, FALSE, FALSE, 0);
    }

    ctx.examples_header = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ctx.examples_header), 0.0);
    gtk_widget_set_margin_top(ctx.examples_header, 2);
    gtk_box_pack_start(GTK_BOX(ctx.content_box), ctx.examples_header, FALSE, FALSE, 0);

    ctx.examples_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(ctx.examples_grid), 24);
    gtk_grid_set_row_spacing(GTK_GRID(ctx.examples_grid), 4);

    GtkWidget *ex_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(ex_sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(ex_sw), ctx.examples_grid);
    gtk_box_pack_start(GTK_BOX(ctx.content_box), ex_sw, TRUE, TRUE, 0);

    if (show_options && ctx.num_buttons > 0) rebuild_all_examples();
    else if (!show_options && ctx.response->num_options > 0) rebuild_examples_async(0);

    gtk_widget_show_all(ctx.content_box);
}

static gboolean on_translate_complete(gpointer data) {
    TranslateJob *job = data;
    ctx.busy = 0;

    if (job->response) {
        free_translation_response(ctx.response);
        ctx.response = job->response;

        save_config_target_lang(job->target_lang);
        save_config_last_source_lang(job->source_lang);

        if (ctx.response->num_options == 0 && ctx.response->num_translations > 0) {
            if (fill_bst_options(ctx.response, ctx.input_text, ctx.response->source_lang,
                    ctx.response->target_lang) != 0) {
                ctx.response->options = calloc(1, sizeof(TranslationOption));
                ctx.response->options[0].translation = strdup(ctx.response->translations[0]);
                ctx.response->num_options = 1;
            }
        }
        build_ui();
    } else {
        gtk_label_set_text(GTK_LABEL(ctx.examples_header), "Translation failed");
        gtk_widget_show_all(ctx.content_box);
    }

    free(job->text);
    free(job->source_lang);
    free(job->target_lang);
    g_free(job);
    return FALSE;
}

static gpointer translate_thread(gpointer data) {
    TranslateJob *job = data;
    job->response = translate_text(job->text, job->source_lang, job->target_lang);
    g_idle_add(on_translate_complete, job);
    return NULL;
}

static void do_translate_async(const char *src, const char *tgt) {
    if (ctx.busy) return;
    ctx.busy = 1;

    remove_all_children(ctx.content_box);
    gtk_label_set_text(GTK_LABEL(ctx.examples_header), "Translating...");
    gtk_widget_show_all(ctx.content_box);

    TranslateJob *job = g_new(TranslateJob, 1);
    job->text = strdup(ctx.input_text);
    job->source_lang = strdup(src);
    job->target_lang = strdup(tgt);
    job->response = NULL;
    g_thread_new("translate", translate_thread, job);
}

static char *get_selected_option_text(void) {
    for (int i = 0; i < ctx.num_buttons; i++) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctx.option_buttons[i]))) {
            const char *label = gtk_button_get_label(GTK_BUTTON(ctx.option_buttons[i]));
            if (!label) return NULL;
            char *text = strdup(label);
            char *bracket = strrchr(text, '[');
            if (bracket && bracket > text && bracket[-1] == ' ') {
                char *end = NULL;
                strtol(bracket + 1, &end, 10);
                if (end && *end == ']') bracket[-1] = '\0';
            }
            return text;
        }
    }
    return NULL;
}

static const char *find_swap_lang(const char *exclude) {
    for (int i = 0; SUPPORTED_LANGUAGES[i]; i++)
        if (strcasecmp(SUPPORTED_LANGUAGES[i], exclude) != 0) return SUPPORTED_LANGUAGES[i];
    return "english";
}

static void retranslate(void) {
    if (ctx.toggling) return;
    if (ctx.suppressing) return;
    if (ctx.busy) return;

    char *src = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.source_combo));
    char *tgt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.target_combo));
    if (!src || !tgt) {
        g_free(src);
        g_free(tgt);
        return;
    }

    if (!lang_to_code(src) || !lang_to_code(tgt)) {
        g_free(src);
        g_free(tgt);
        return;
    }

    if (strcasecmp(src, tgt) == 0) {
        char *saved_tgt = load_config_target_lang();
        char *saved_src = load_config_last_source_lang();
        const char *new_src;
        const char *new_tgt;

        if (saved_tgt && saved_src && strcasecmp(saved_tgt, saved_src) != 0 &&
            strcasecmp(saved_tgt, src) != 0) {
            new_src = saved_tgt;
            new_tgt = saved_src;
        } else {
            new_src = find_swap_lang(src);
            new_tgt = src;
        }

        char *selected_text = get_selected_option_text();
        if (!selected_text && ctx.response && ctx.response->num_options > 0 &&
            ctx.response->options[0].translation)
            selected_text = strdup(ctx.response->options[0].translation);

        ctx.suppressing = 1;
        set_combo_active(GTK_COMBO_BOX(ctx.source_combo), new_src);
        set_combo_active(GTK_COMBO_BOX(ctx.target_combo), new_tgt);
        ctx.suppressing = 0;

        if (selected_text) {
            free(ctx.input_text);
            ctx.input_text = selected_text;
            gtk_entry_set_text(GTK_ENTRY(ctx.text_entry), ctx.input_text);
        }

        g_free(src);
        g_free(tgt);
        do_translate_async(new_src, new_tgt);
        g_free(saved_tgt);
        g_free(saved_src);
        return;
    }

    do_translate_async(src, tgt);
    g_free(src);
    g_free(tgt);
}

static void on_lang_changed(GtkComboBox *combo, gpointer data) {
    (void)combo;
    (void)data;
    if (ctx.suppressing) return;
    if (!ctx.built) return;
    retranslate();
}

static void on_swap_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    (void)data;
    if (ctx.busy) return;

    char *src = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.source_combo));
    char *tgt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.target_combo));
    if (!src || !tgt) {
        g_free(src);
        g_free(tgt);
        return;
    }

    char *selected_text = get_selected_option_text();
    if (!selected_text && ctx.response && ctx.response->num_options > 0 &&
        ctx.response->options[0].translation)
        selected_text = strdup(ctx.response->options[0].translation);

    ctx.suppressing = 1;
    set_combo_active(GTK_COMBO_BOX(ctx.source_combo), tgt);
    set_combo_active(GTK_COMBO_BOX(ctx.target_combo), src);
    ctx.suppressing = 0;

    if (selected_text) {
        free(ctx.input_text);
        ctx.input_text = selected_text;
        gtk_entry_set_text(GTK_ENTRY(ctx.text_entry), ctx.input_text);
    }

    g_free(src);
    g_free(tgt);

    retranslate();
}

static void on_destroy(void) {
    gtk_main_quit();
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

static GtkWidget *make_lang_combo(GtkBox *parent, const char *label_text) {
    GtkWidget *lbl = gtk_label_new(NULL);
    char *m = g_markup_printf_escaped("<b>%s</b>", label_text);
    gtk_label_set_markup(GTK_LABEL(lbl), m);
    g_free(m);
    gtk_box_pack_start(parent, lbl, FALSE, FALSE, 0);

    GtkWidget *combo = gtk_combo_box_text_new();
    for (int i = 0; SUPPORTED_LANGUAGES[i]; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), SUPPORTED_LANGUAGES[i]);
    gtk_box_pack_start(parent, combo, FALSE, FALSE, 0);
    return combo;
}

static void on_translate_clicked(void) {
    if (ctx.busy) return;

    const char *new_text = gtk_entry_get_text(GTK_ENTRY(ctx.text_entry));
    if (!new_text || !*new_text) return;

    char *src = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.source_combo));
    char *tgt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctx.target_combo));
    if (!src || !tgt) {
        g_free(src);
        g_free(tgt);
        return;
    }

    free(ctx.input_text);
    ctx.input_text = strdup(new_text);

    do_translate_async(src, tgt);
    g_free(src);
    g_free(tgt);
}

static gboolean on_initial_translate_complete(gpointer data) {
    TranslateJob *job = data;
    ctx.busy = 0;

    if (job->response) {
        ctx.response = job->response;

        save_config_target_lang(job->target_lang);
        save_config_last_source_lang(job->source_lang);

        if (ctx.response->num_options == 0 && ctx.response->num_translations > 0) {
            if (fill_bst_options(ctx.response, ctx.input_text, ctx.response->source_lang,
                    ctx.response->target_lang) != 0) {
                ctx.response->options = calloc(1, sizeof(TranslationOption));
                ctx.response->options[0].translation = strdup(ctx.response->translations[0]);
                ctx.response->num_options = 1;
            }
        }
        build_ui();
    } else {
        remove_all_children(ctx.content_box);
        GtkWidget *err = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(err), "<span color='red'>Translation failed</span>");
        gtk_label_set_line_wrap(GTK_LABEL(err), TRUE);
        gtk_box_pack_start(GTK_BOX(ctx.content_box), err, FALSE, FALSE, 0);
        gtk_widget_show_all(ctx.content_box);
    }

    free(job->text);
    free(job->source_lang);
    free(job->target_lang);
    g_free(job);
    return FALSE;
}

static gpointer initial_translate_thread(gpointer data) {
    TranslateJob *job = data;
    job->response = translate_text(job->text, job->source_lang, job->target_lang);
    g_idle_add(on_initial_translate_complete, job);
    return NULL;
}

void show_translation_gui(const char *text, const char *source_lang, const char *target_lang) {
    memset(&ctx, 0, sizeof(ctx));
    ctx.input_text = strdup(text);
    ctx.built = 0;

    ctx.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx.window), "Reverso Linux");
    gtk_window_set_default_size(GTK_WINDOW(ctx.window), 640, 640);
    gtk_window_set_resizable(GTK_WINDOW(ctx.window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(ctx.window), TRUE);
    gtk_window_set_position(GTK_WINDOW(ctx.window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(ctx.window), 8);

    g_signal_connect(ctx.window, "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(ctx.window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(ctx.window), vbox);

    GtkWidget *lang_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    ctx.source_combo = make_lang_combo(GTK_BOX(lang_hbox), "From:");

    GtkWidget *swap_btn = gtk_button_new_with_label("\xe2\x87\x84");
    gtk_widget_set_tooltip_text(swap_btn, "Swap languages");
    g_signal_connect(swap_btn, "clicked", G_CALLBACK(on_swap_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(lang_hbox), swap_btn, FALSE, FALSE, 0);

    ctx.target_combo = make_lang_combo(GTK_BOX(lang_hbox), "To:");

    char *saved_lang = load_config_target_lang();
    if (!saved_lang) saved_lang = strdup("russian");

    set_combo_active(GTK_COMBO_BOX(ctx.target_combo), saved_lang);
    set_combo_active(GTK_COMBO_BOX(ctx.source_combo), source_lang);

    g_signal_connect(ctx.source_combo, "changed", G_CALLBACK(on_lang_changed), NULL);
    g_signal_connect(ctx.target_combo, "changed", G_CALLBACK(on_lang_changed), NULL);

    free(saved_lang);
    gtk_box_pack_start(GTK_BOX(vbox), lang_hbox, FALSE, FALSE, 0);

    GtkWidget *input_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    ctx.text_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ctx.text_entry), text);
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx.text_entry), "Enter text to translate...");
    g_signal_connect(ctx.text_entry, "activate", G_CALLBACK(on_translate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(input_hbox), ctx.text_entry, TRUE, TRUE, 0);

    ctx.translate_btn = gtk_button_new_with_label("Translate");
    g_signal_connect(ctx.translate_btn, "clicked", G_CALLBACK(on_translate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(input_hbox), ctx.translate_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), input_hbox, FALSE, FALSE, 0);

    ctx.content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), ctx.content_box, TRUE, TRUE, 0);

    GtkWidget *loading = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(loading), "<i>Translating...</i>");
    gtk_box_pack_start(GTK_BOX(ctx.content_box), loading, FALSE, FALSE, 0);

    GtkWidget *close_btn = gtk_button_new_with_label("Close (Esc)");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), close_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(ctx.window);
    ctx.built = 1;

    ctx.busy = 1;
    TranslateJob *job = g_new(TranslateJob, 1);
    job->text = strdup(text);
    job->source_lang = strdup(source_lang);
    job->target_lang = strdup(target_lang);
    job->response = NULL;
    g_thread_new("initial-translate", initial_translate_thread, job);
}
