#define _GNU_SOURCE

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

char *load_config_target_lang(void) {
    char *path = get_config_path();
    if (!path) return NULL;

    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;

    char line[256];
    char *lang = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "target_lang=", 12) == 0) {
            size_t len = strlen(line);
            if (len > 12 && line[len - 1] == '\n') line[len - 1] = '\0';
            lang = strdup(line + 12);
            break;
        }
    }
    fclose(f);
    return lang;
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

void save_config_target_lang(const char *lang) {
    char *path = get_config_path();
    if (!path) return;

    char *existing_source = read_value(path, "last_source_lang=", 17);

    FILE *f = fopen(path, "w");
    free(path);
    if (!f) {
        free(existing_source);
        return;
    }

    fprintf(f, "target_lang=%s\n", lang);
    if (existing_source) {
        fprintf(f, "last_source_lang=%s\n", existing_source);
        free(existing_source);
    }
    fclose(f);
}

char *load_config_last_source_lang(void) {
    char *path = get_config_path();
    if (!path) return NULL;
    char *val = read_value(path, "last_source_lang=", 17);
    free(path);
    return val;
}

void save_config_last_source_lang(const char *lang) {
    char *path = get_config_path();
    if (!path) return;

    char *existing_target = read_value(path, "target_lang=", 12);

    FILE *f = fopen(path, "w");
    free(path);
    if (!f) {
        free(existing_target);
        return;
    }

    if (existing_target) {
        fprintf(f, "target_lang=%s\n", existing_target);
        free(existing_target);
    }
    fprintf(f, "last_source_lang=%s\n", lang);
    fclose(f);
}
