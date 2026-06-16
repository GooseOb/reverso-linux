#define _GNU_SOURCE

#include "translator.h"
#include "gui.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <gtk/gtk.h>

#define MAX_TEXT 4096

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options] [text...]\n\n"
            "Options:\n"
            "  -t, --target LANG   Default target language\n"
            "  -v, --version       Show version\n"
            "  -h, --help          Show this help\n\n"
            "Source and target are remembered between sessions.\n"
            "Examples:\n"
            "  %s hello world\n"
            "  wl-paste -p | %s\n",
            prog, prog, prog);
}

static char *read_text(int argc, char *argv[], int optind) {
    if (optind < argc) {
        size_t len = 0;
        for (int i = optind; i < argc; i++)
            len += strlen(argv[i]) + 1;
        char *text = malloc(len + 1);
        if (!text) return NULL;
        text[0] = '\0';
        for (int i = optind; i < argc; i++) {
            if (i > optind) strcat(text, " ");
            strcat(text, argv[i]);
        }
        return text;
    }

    if (!isatty(STDIN_FILENO)) {
        char *buf = malloc(MAX_TEXT);
        if (!buf) return NULL;
        size_t pos = 0;
        int ch;
        while ((ch = fgetc(stdin)) != EOF && pos < MAX_TEXT - 1)
            buf[pos++] = ch;
        buf[pos] = '\0';
        while (pos > 0 && (buf[pos - 1] == '\n' || buf[pos - 1] == '\r'))
            buf[--pos] = '\0';
        if (pos == 0) { free(buf); return NULL; }
        return buf;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    static struct option opts[] = {
        {"target",  required_argument, 0, 't'},
        {"version", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "t:vh", opts, NULL)) != -1) {
        switch (c) {
        case 't':
            save_config_target_lang(optarg);
            break;
        case 'v':
            printf("reverso-linux %s\n", VERSION);
            return 0;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }

    char *text = read_text(argc, argv, optind);
    if (!text) {
        if (optind == 1 && argc > 1) {
            usage(argv[0]);
            return 1;
        }
        return 0;
    }

    char *target = load_config_target_lang();
    if (!target) target = strdup("russian");

    char *saved_src = load_config_last_source_lang();
    char *source_lang = NULL;
    if (saved_src && lang_to_code(saved_src) &&
        strcasecmp(saved_src, target) != 0)
        source_lang = strdup(saved_src);
    if (!source_lang)
        source_lang = strdup(strcasecmp(target, "english") == 0
                             ? "russian" : "english");

    TranslationResponse *r = translate_text(text, source_lang, target);

    save_config_target_lang(target);
    save_config_last_source_lang(source_lang);

    free(saved_src);
    free(target);

    if (!r) {
        fprintf(stderr, "Translation failed\n");
        free(text);
        free(source_lang);
        return 1;
    }

    g_set_prgname("reverso-linux");
    gtk_init(0, NULL);

    show_translation_gui(text, source_lang, r);
    gtk_main();

    free(source_lang);
    free(text);
    return 0;
}
