#define _GNU_SOURCE

#include "gui.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>

static struct {
    char *input_text;
    TranslationResponse *response;
    GtkWidget *content_box;
    GtkWidget *source_combo;
    GtkWidget *target_combo;
    GtkWidget *text_label;
    GtkWidget *examples_header;
    GtkWidget *examples_grid;
    GtkWidget *option_buttons[64];
    int num_buttons;
    int built;
    int toggling;
    int updating_source;
    int suppressing;
} ctx;

static int is_short_text(const char *text) {
    if (!text || !*text) return 0;
    int words = 0, in_word = 0, punct = 0;
    for (const char *p = text; *p; p++) {
        if (isspace((unsigned char)*p)) { in_word = 0; }
        else {
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
    for (GList *l = child; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
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

static void rebuild_examples(int option_idx) {
    if (!ctx.response || option_idx < 0 ||
        option_idx >= ctx.response->num_options) return;

    TranslationOption *opt = &ctx.response->options[option_idx];
    char header[256];
    snprintf(header, sizeof(header), "Examples for: %s",
             opt->translation ? opt->translation : "?");
    gtk_label_set_text(GTK_LABEL(ctx.examples_header), header);

    GList *child = gtk_container_get_children(GTK_CONTAINER(ctx.examples_grid));
    for (GList *l = child; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(child);

    char *src_lang = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(ctx.source_combo));
    char *tgt_lang = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(ctx.target_combo));

    ContextExamples *fetched = NULL;
    if (src_lang && tgt_lang && ctx.input_text)
        fetched = fetch_context_examples(ctx.input_text, src_lang, tgt_lang,
                                         opt->translation);

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
        free_context_examples(fetched);
    } else {
        const char *err_msg = fetched && fetched->error ? fetched->error
            : "Failed to fetch examples from context.reverso.net";
        char markup[512];
        snprintf(markup, sizeof(markup),
                 "<span color='red'>%s</span>", err_msg);
        GtkWidget *err = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(err), markup);
        gtk_label_set_xalign(GTK_LABEL(err), 0.0);
        gtk_label_set_line_wrap(GTK_LABEL(err), TRUE);
        gtk_grid_attach(GTK_GRID(ctx.examples_grid), err, 0, 0, 2, 1);
        free_context_examples(fetched);
    }

    g_free(src_lang);
    g_free(tgt_lang);
    gtk_widget_show_all(ctx.examples_grid);
}

static void on_option_toggled(GtkToggleButton *btn, gpointer user_data) {
    (void)user_data;
    if (ctx.toggling) return;

    if (gtk_toggle_button_get_active(btn)) {
        ctx.toggling = 1;
        int idx = -1;
        for (int i = 0; i < ctx.num_buttons; i++) {
            if (ctx.option_buttons[i] == GTK_WIDGET(btn)) idx = i;
            else gtk_toggle_button_set_active(
                     GTK_TOGGLE_BUTTON(ctx.option_buttons[i]), FALSE);
        }
        ctx.toggling = 0;
        if (idx >= 0) rebuild_examples(idx);
    } else {
        rebuild_all_examples();
    }
}

static void remove_all_children(GtkWidget *box) {
    GList *child = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *l = child; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
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

        for (int i = 0; i < ctx.response->num_options &&
                        ctx.num_buttons < 64; i++) {
            TranslationOption *opt = &ctx.response->options[i];
            if (!opt->translation || !*opt->translation) continue;

            char label[128];
            int cnt = opt->frequency > 0 ? opt->frequency
                                         : opt->occurrence_count;
            if (cnt > 0)
                snprintf(label, sizeof(label), "%s [%d]",
                         opt->translation, cnt);
            else
                snprintf(label, sizeof(label), "%s", opt->translation);

            GtkWidget *btn = gtk_toggle_button_new_with_label(label);
            gtk_widget_set_tooltip_text(btn, "Click to see examples");
            gtk_container_add(GTK_CONTAINER(flowbox), btn);
            ctx.option_buttons[ctx.num_buttons++] = btn;
            g_signal_connect(btn, "toggled",
                             G_CALLBACK(on_option_toggled), NULL);
        }

        gtk_box_pack_start(GTK_BOX(ctx.content_box), flowbox,
                           FALSE, FALSE, 0);
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
        gtk_box_pack_start(GTK_BOX(ctx.content_box), trans_lbl,
                           FALSE, FALSE, 0);
    }

    ctx.examples_header = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ctx.examples_header), 0.0);
    gtk_widget_set_margin_top(ctx.examples_header, 2);
    gtk_box_pack_start(GTK_BOX(ctx.content_box), ctx.examples_header,
                       FALSE, FALSE, 0);

    ctx.examples_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(ctx.examples_grid), 24);
    gtk_grid_set_row_spacing(GTK_GRID(ctx.examples_grid), 4);

    GtkWidget *ex_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ex_sw),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(ex_sw), ctx.examples_grid);
    gtk_box_pack_start(GTK_BOX(ctx.content_box), ex_sw, TRUE, TRUE, 0);

    if (show_options && ctx.num_buttons > 0)
        rebuild_all_examples();
    else if (!show_options && ctx.response->num_options > 0)
        rebuild_examples(0);

    gtk_widget_show_all(ctx.content_box);
}

static void do_translate(const char *src, const char *tgt) {
    int swapped = 0;
    TranslationResponse *r = translate_text(ctx.input_text, src, tgt);
    if (!r) {
        gtk_label_set_text(GTK_LABEL(ctx.examples_header), "Translation failed");
        return;
    }

    if (r->detected_language[0]) {
        const char *detected = r->detected_language;

        if (strcasecmp(detected, src) == 0 &&
            r->direction_confidence < 1000) {
            TranslationResponse *r2 = translate_text(ctx.input_text, tgt, src);
            if (r2 && r2->direction_confidence > r->direction_confidence) {
                free_translation_response(r); r = r2; swapped = 1;
            } else if (r2) {
                free_translation_response(r2);
            }
        } else if (strcasecmp(detected, src) != 0 &&
                   strcasecmp(detected, tgt) != 0) {
            TranslationResponse *r2 = translate_text(
                ctx.input_text, detected, tgt);
            if (r2) { free_translation_response(r); r = r2; }
        }
    }

    free_translation_response(ctx.response);
    ctx.response = r;

    if (swapped) {
        save_config_target_lang(src);
        save_config_last_source_lang(
            r->detected_language[0] ? r->detected_language : tgt);
        ctx.updating_source = 1;
        set_combo_active(GTK_COMBO_BOX(ctx.source_combo), tgt);
        ctx.updating_source = 0;
        ctx.suppressing = 1;
        set_combo_active(GTK_COMBO_BOX(ctx.target_combo), src);
        ctx.suppressing = 0;
    } else {
        save_config_target_lang(tgt);
        if (r->detected_language[0])
            save_config_last_source_lang(r->detected_language);
        ctx.updating_source = 1;
        if (r->detected_language[0])
            set_combo_active(GTK_COMBO_BOX(ctx.source_combo),
                             r->detected_language);
        ctx.updating_source = 0;
    }
    build_ui();
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
                if (end && *end == ']')
                    bracket[-1] = '\0';
            }
            return text;
        }
    }
    return NULL;
}

static const char *find_swap_lang(const char *exclude) {
    for (int i = 0; SUPPORTED_LANGUAGES[i]; i++)
        if (strcasecmp(SUPPORTED_LANGUAGES[i], exclude) != 0)
            return SUPPORTED_LANGUAGES[i];
    return "english";
}

static void retranslate(void) {
    if (ctx.toggling) return;
    if (ctx.suppressing) return;

    char *src = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(ctx.source_combo));
    char *tgt = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(ctx.target_combo));
    if (!src || !tgt) { g_free(src); g_free(tgt); return; }

    if (strcasecmp(src, tgt) == 0) {
        char *saved_tgt = load_config_target_lang();
        char *saved_src = load_config_last_source_lang();
        const char *new_src;
        const char *new_tgt;

        if (saved_tgt && saved_src &&
            strcasecmp(saved_tgt, saved_src) != 0 &&
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
            char buf[520];
            snprintf(buf, sizeof(buf), "\xe2\x80\x9c%.500s\xe2\x80\x9d", ctx.input_text);
            gtk_label_set_text(GTK_LABEL(ctx.text_label), buf);
        }

        g_free(src); g_free(tgt);
        do_translate(new_src, new_tgt);
        g_free(saved_tgt); g_free(saved_src);
        return;
    }

    do_translate(src, tgt);
    g_free(src); g_free(tgt);
}

static void on_lang_changed(GtkComboBox *combo, gpointer data) {
    (void)combo; (void)data;
    if (ctx.suppressing) return;
    if (ctx.updating_source) return;
    if (!ctx.built) return;
    retranslate();
}

static void on_destroy(void) { gtk_main_quit(); }

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer data) {
    (void)widget; (void)data;
    if (event->keyval == GDK_KEY_Escape) { gtk_main_quit(); return TRUE; }
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
        gtk_combo_box_text_append_text(
            GTK_COMBO_BOX_TEXT(combo), SUPPORTED_LANGUAGES[i]);
    gtk_box_pack_start(parent, combo, FALSE, FALSE, 0);
    return combo;
}

void show_translation_gui(const char *text, const char *source_lang,
                          TranslationResponse *initial_r) {
    memset(&ctx, 0, sizeof(ctx));
    ctx.input_text = strdup(text);
    ctx.response = initial_r;
    ctx.built = 0;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Reverso Linux");
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 320);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 8);

    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *lang_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    ctx.source_combo = make_lang_combo(GTK_BOX(lang_hbox), "From:");

    GtkWidget *arr = gtk_label_new(" \xe2\x86\x92 ");
    gtk_box_pack_start(GTK_BOX(lang_hbox), arr, FALSE, FALSE, 0);

    ctx.target_combo = make_lang_combo(GTK_BOX(lang_hbox), "To:");

    char *saved_lang = load_config_target_lang();
    if (!saved_lang) saved_lang = strdup("russian");

    set_combo_active(GTK_COMBO_BOX(ctx.target_combo), saved_lang);
    set_combo_active(GTK_COMBO_BOX(ctx.source_combo), source_lang);

    g_signal_connect(ctx.source_combo, "changed",
                     G_CALLBACK(on_lang_changed), NULL);
    g_signal_connect(ctx.target_combo, "changed",
                     G_CALLBACK(on_lang_changed), NULL);

    free(saved_lang);
    gtk_box_pack_start(GTK_BOX(vbox), lang_hbox, FALSE, FALSE, 0);

    char text_buf[520];
    snprintf(text_buf, sizeof(text_buf),
             "\xe2\x80\x9c%.500s\xe2\x80\x9d", text);
    ctx.text_label = gtk_label_new(text_buf);
    gtk_label_set_xalign(GTK_LABEL(ctx.text_label), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(ctx.text_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), ctx.text_label, FALSE, FALSE, 0);

    ctx.content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), ctx.content_box, TRUE, TRUE, 0);

    build_ui();

    GtkWidget *close_btn = gtk_button_new_with_label("Close (Esc)");
    g_signal_connect_swapped(close_btn, "clicked",
                             G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), close_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    ctx.built = 1;
}
