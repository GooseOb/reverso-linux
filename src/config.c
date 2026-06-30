#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *get_config_path(void) {
    const char *home = getenv("HOME");
    if (!home) return NULL;
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *base = xdg ? xdg : home;
    char *dir;
    if (xdg) {
        dir = malloc(strlen(base) + 30);
        sprintf(dir, "%s/reverso-linux", base);
    } else {
        dir = malloc(strlen(base) + 40);
        sprintf(dir, "%s/.config/reverso-linux", base);
    }
    mkdir(dir, 0755);
    char *path = malloc(strlen(dir) + 10);
    sprintf(path, "%s/config", dir);
    free(dir);
    return path;
}

static char *read_value(const char *path, const char *key, int key_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[256];
    char *val = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0) {
            size_t len = strlen(line);
            if ((int)len > key_len && line[len - 1] == '\n') line[len - 1] = '\0';
            val = strdup(line + key_len);
            break;
        }
    }
    fclose(f);
    return val;
}

char *load_config_target_lang(void) {
    char *path = get_config_path();
    if (!path) return NULL;
    char *val = read_value(path, "target_lang=", 12);
    free(path);
    return val;
}

char *load_config_last_source_lang(void) {
    char *path = get_config_path();
    if (!path) return NULL;
    char *val = read_value(path, "last_source_lang=", 17);
    free(path);
    return val;
}

static void rewrite_config(const char *target, const char *source) {
    char *path = get_config_path();
    if (!path) return;
    FILE *f = fopen(path, "w");
    free(path);
    if (!f) return;
    if (target) fprintf(f, "target_lang=%s\n", target);
    if (source) fprintf(f, "last_source_lang=%s\n", source);
    fclose(f);
}

void save_config_target_lang(const char *lang) {
    char *source = load_config_last_source_lang();
    rewrite_config(lang, source);
    free(source);
}

void save_config_last_source_lang(const char *lang) {
    char *target = load_config_target_lang();
    rewrite_config(target, lang);
    free(target);
}
