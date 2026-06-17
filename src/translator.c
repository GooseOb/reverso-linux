#define _GNU_SOURCE

#include "translator.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *API_URL = "https://api.reverso.net/translate/v1/translation";

static struct {
    const char *name;
    const char *code;
} LANG_MAP[] = {{"english", "eng"}, {"russian", "rus"}, {"ukrainian", "ukr"}, {"french", "fra"},
    {"german", "ger"}, {"spanish", "spa"}, {"italian", "ita"}, {"portuguese", "por"},
    {"polish", "pol"}, {"dutch", "dut"}, {"arabic", "ara"}, {"hebrew", "heb"}, {"japanese", "jpn"},
    {"turkish", "tur"}, {"chinese", "chi"}, {"romanian", "rum"}, {"swedish", "swe"}, {NULL, NULL}};

const char *SUPPORTED_LANGUAGES[] = {"english", "russian", "ukrainian", "french", "german",
    "spanish", "italian", "portuguese", "polish", "dutch", "arabic", "hebrew", "japanese",
    "turkish", "chinese", "romanian", "swedish", NULL};

const char *lang_to_code(const char *lang) {
    for (int i = 0; LANG_MAP[i].name; i++)
        if (strcasecmp(LANG_MAP[i].name, lang) == 0) return LANG_MAP[i].code;
    return NULL;
}

const char *code_to_lang(const char *code) {
    for (int i = 0; LANG_MAP[i].name; i++)
        if (strcasecmp(LANG_MAP[i].code, code) == 0) return LANG_MAP[i].name;
    return NULL;
}

struct WriteBuf {
    char *data;
    size_t len;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    struct WriteBuf *buf = userdata;
    size_t total = size * nmemb;
    char *tmp = realloc(buf->data, buf->len + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

static char *strip_html(const char *src) {
    size_t len = strlen(src);
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    size_t j = 0;
    int in_tag = 0;
    char tag_buf[16];
    size_t tag_pos = 0;
    for (size_t i = 0; i < len && src[i]; i++) {
        if (src[i] == '<') {
            in_tag = 1;
            tag_pos = 0;
            continue;
        }
        if (src[i] == '>') {
            in_tag = 0;
            tag_buf[tag_pos] = '\0';
            if (tag_pos >= 2 && tag_buf[0] == 'e' && tag_buf[1] == 'm') {
                const char *b = "<b>";
                for (int k = 0; b[k]; k++) out[j++] = b[k];
            } else if (tag_pos >= 3 && tag_buf[0] == '/' && tag_buf[1] == 'e' &&
                       tag_buf[2] == 'm') {
                const char *b = "</b>";
                for (int k = 0; b[k]; k++) out[j++] = b[k];
            }
            continue;
        }
        if (in_tag) {
            if (tag_pos < sizeof(tag_buf) - 1) tag_buf[tag_pos++] = src[i];
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
    return out;
}

TranslationResponse *translate_text(const char *text, const char *source, const char *target) {
    const char *src_code = lang_to_code(source);
    const char *tgt_code = lang_to_code(target);
    if (!src_code || !tgt_code) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    json_object *root = json_object_new_object();
    json_object_object_add(root, "format", json_object_new_string("text"));
    json_object_object_add(root, "from", json_object_new_string(src_code));
    json_object_object_add(root, "input", json_object_new_string(text));

    json_object *opts = json_object_new_object();
    json_object_object_add(opts, "contextResults", json_object_new_boolean(1));
    json_object_object_add(opts, "origin", json_object_new_string("reversomobile"));
    json_object_object_add(opts, "sentenceSplitter", json_object_new_boolean(0));
    json_object_object_add(root, "options", opts);
    json_object_object_add(root, "to", json_object_new_string(tgt_code));

    const char *body = json_object_to_json_string(root);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Origin: https://www.reverso.net");
    headers = curl_slist_append(headers, "Referer: https://www.reverso.net/");
    headers = curl_slist_append(headers, "Connection: keep-alive");
    headers = curl_slist_append(headers, "Sec-Fetch-Dest: empty");
    headers = curl_slist_append(headers, "Sec-Fetch-Mode: cors");
    headers = curl_slist_append(headers, "Sec-Fetch-Site: same-site");

    struct WriteBuf buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 "
        "Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);

    json_object_put(root);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !buf.data) {
        free(buf.data);
        return NULL;
    }

    json_object *resp = json_tokener_parse(buf.data);
    if (!resp) {
        free(buf.data);
        return NULL;
    }

    TranslationResponse *r = calloc(1, sizeof(TranslationResponse));
    strncpy(r->input_text, text, MAX_TEXT - 1);
    strncpy(r->source_lang, source, MAX_LANG - 1);
    strncpy(r->target_lang, target, MAX_LANG - 1);

    json_object *trans_arr;
    if (json_object_object_get_ex(resp, "translation", &trans_arr)) {
        int n = json_object_array_length(trans_arr);
        r->translations = calloc(n, sizeof(char *));
        for (int i = 0; i < n; i++) {
            json_object *t = json_object_array_get_idx(trans_arr, i);
            const char *s = json_object_get_string(t);
            r->translations[i] = strdup(s ? s : "");
            r->num_translations++;
        }
    }

    json_object *ctx_results;
    if (json_object_object_get_ex(resp, "contextResults", &ctx_results)) {
        json_object *results;
        if (json_object_object_get_ex(ctx_results, "results", &results)) {
            int n = json_object_array_length(results);
            if (n > MAX_OPTIONS) n = MAX_OPTIONS;
            r->options = calloc(n, sizeof(TranslationOption));
            for (int i = 0; i < n; i++) {
                json_object *result = json_object_array_get_idx(results, i);

                json_object *trans;
                if (json_object_object_get_ex(result, "translation", &trans)) {
                    r->options[i].translation = strdup(json_object_get_string(trans));
                }

                json_object *freq;
                if (json_object_object_get_ex(result, "frequency", &freq))
                    r->options[i].frequency = json_object_get_int(freq);

                json_object *src_ex;
                int cnt = 0;
                if (json_object_object_get_ex(result, "sourceExamples", &src_ex))
                    cnt = json_object_array_length(src_ex);
                r->options[i].occurrence_count = cnt;

                json_object *tgt_ex;
                if (json_object_object_get_ex(result, "targetExamples", &tgt_ex)) {
                    int ex_n = json_object_array_length(tgt_ex);
                    if (ex_n > MAX_EXAMPLES) ex_n = MAX_EXAMPLES;
                    r->options[i].source_examples = calloc(ex_n, sizeof(char *));
                    r->options[i].target_examples = calloc(ex_n, sizeof(char *));
                    r->options[i].num_examples = ex_n;
                    for (int j = 0; j < ex_n; j++) {
                        json_object *se = json_object_array_get_idx(src_ex, j);
                        json_object *te = json_object_array_get_idx(tgt_ex, j);
                        r->options[i].source_examples[j] = strip_html(json_object_get_string(se));
                        r->options[i].target_examples[j] = strip_html(json_object_get_string(te));
                    }
                }

                r->num_options++;
            }
        }
    }

    json_object_put(resp);
    free(buf.data);
    return r;
}

void free_translation_response(TranslationResponse *r) {
    if (!r) return;
    for (int i = 0; i < r->num_translations; i++) free(r->translations[i]);
    free(r->translations);
    for (int i = 0; i < r->num_options; i++) {
        free(r->options[i].translation);
        for (int j = 0; j < r->options[i].num_examples; j++) {
            free(r->options[i].source_examples[j]);
            free(r->options[i].target_examples[j]);
        }
        free(r->options[i].source_examples);
        free(r->options[i].target_examples);
    }
    free(r->options);
    free(r);
}

#define MAX_FETCHED 256

static const char *lang_to_code_short(const char *lang) {
    static const char *map[][2] = {{"english", "en"}, {"russian", "ru"}, {"ukrainian", "uk"},
        {"french", "fr"}, {"german", "de"}, {"spanish", "es"}, {"italian", "it"},
        {"portuguese", "pt"}, {"polish", "pl"}, {"dutch", "nl"}, {"arabic", "ar"}, {"hebrew", "he"},
        {"japanese", "ja"}, {"turkish", "tr"}, {"chinese", "zh"}, {"romanian", "ro"},
        {"swedish", "sv"}, {NULL, NULL}};
    for (int i = 0; map[i][0]; i++)
        if (strcasecmp(map[i][0], lang) == 0) return map[i][1];
    return NULL;
}

ContextExamples *fetch_context_examples(
    const char *text, const char *source, const char *target, const char *fragment) {
    if (!fragment) return NULL;

    const char *src_short = lang_to_code_short(source);
    const char *tgt_short = lang_to_code_short(target);
    if (!src_short || !tgt_short) return NULL;

    json_object *body = json_object_new_object();
    json_object_object_add(body, "source_text", json_object_new_string(text));
    json_object_object_add(body, "target_text", json_object_new_string(fragment));
    json_object_object_add(body, "source_lang", json_object_new_string(src_short));
    json_object_object_add(body, "target_lang", json_object_new_string(tgt_short));
    json_object_object_add(body, "npage", json_object_new_int(1));
    json_object_object_add(body, "mode", json_object_new_int(0));

    const char *body_str = json_object_to_json_string(body);

    CURL *curl = curl_easy_init();
    if (!curl) {
        json_object_put(body);
        return NULL;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Origin: https://context.reverso.net");
    headers = curl_slist_append(headers, "Referer: https://context.reverso.net/");
    headers = curl_slist_append(
        headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");

    struct WriteBuf buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, "https://context.reverso.net/bst-query-service");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_object_put(body);

    ContextExamples *ctx = calloc(1, sizeof(ContextExamples));

    if (res != CURLE_OK || !buf.data) {
        ctx->error = strdup("Network error: could not reach context.reverso.net");
        free(buf.data);
        return ctx;
    }

    json_object *resp = json_tokener_parse(buf.data);
    if (!resp) {
        FILE *f = fopen("/tmp/reverso-bst-error.log", "w");
        if (f) {
            fprintf(f,
                "HTTP %ld\ncurl error: %d\nreq: source_text=%s target_text=%s source_lang=%s "
                "target_lang=%s\nresp:\n%s\n",
                http_code, res, text, fragment, src_short, tgt_short, buf.data);
            fclose(f);
        }
        ctx->error = strdup("Failed to parse response from context.reverso.net");
        free(buf.data);
        return ctx;
    }

    json_object *list;
    if (!json_object_object_get_ex(resp, "list", &list) ||
        json_object_get_type(list) != json_type_array) {
        FILE *f = fopen("/tmp/reverso-bst-error.log", "w");
        if (f) {
            fprintf(f,
                "HTTP %ld\ncurl error: %d\nno 'list' array in response\nreq: source_text=%s "
                "target_text=%s source_lang=%s target_lang=%s\nresp:\n%s\n",
                http_code, res, text, fragment, src_short, tgt_short, buf.data);
            fclose(f);
        }
        ctx->error = strdup("No examples list in response from context.reverso.net");
        json_object_put(resp);
        free(buf.data);
        return ctx;
    }

    int n = json_object_array_length(list);
    if (n > MAX_FETCHED) n = MAX_FETCHED;

    ctx->source_examples = calloc(n, sizeof(char *));
    ctx->target_examples = calloc(n, sizeof(char *));

    for (int i = 0; i < n; i++) {
        json_object *item = json_object_array_get_idx(list, i);
        json_object *s_text, *t_text;

        if (json_object_object_get_ex(item, "s_text", &s_text)) {
            const char *raw = json_object_get_string(s_text);
            ctx->source_examples[i] = strip_html(raw ? raw : "");
        }
        if (json_object_object_get_ex(item, "t_text", &t_text)) {
            const char *raw = json_object_get_string(t_text);
            ctx->target_examples[i] = strip_html(raw ? raw : "");
        }
        ctx->num_examples++;
    }

    json_object_put(resp);
    free(buf.data);

    if (ctx->num_examples == 0) { ctx->error = strdup("No examples found on context.reverso.net"); }

    return ctx;
}

int fill_bst_options(
    TranslationResponse *r, const char *text, const char *source, const char *target) {
    const char *src_short = lang_to_code_short(source);
    const char *tgt_short = lang_to_code_short(target);
    if (!src_short || !tgt_short) return -1;

    const char *fragment = r->num_translations > 0 ? r->translations[0] : "";

    json_object *body = json_object_new_object();
    json_object_object_add(body, "source_text", json_object_new_string(text));
    json_object_object_add(body, "target_text", json_object_new_string(fragment));
    json_object_object_add(body, "source_lang", json_object_new_string(src_short));
    json_object_object_add(body, "target_lang", json_object_new_string(tgt_short));
    json_object_object_add(body, "npage", json_object_new_int(1));
    json_object_object_add(body, "mode", json_object_new_int(0));

    const char *body_str = json_object_to_json_string(body);

    CURL *curl = curl_easy_init();
    if (!curl) {
        json_object_put(body);
        return -1;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Origin: https://context.reverso.net");
    headers = curl_slist_append(headers, "Referer: https://context.reverso.net/");
    headers = curl_slist_append(
        headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");

    struct WriteBuf buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, "https://context.reverso.net/bst-query-service");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_object_put(body);

    if (res != CURLE_OK || !buf.data) {
        free(buf.data);
        return -1;
    }

    json_object *resp = json_tokener_parse(buf.data);
    if (!resp) {
        free(buf.data);
        return -1;
    }

    int count = 0;

    json_object *dict_entries;
    if (json_object_object_get_ex(resp, "dictionary_entry_list", &dict_entries) &&
        json_object_get_type(dict_entries) == json_type_array) {
        int n = json_object_array_length(dict_entries);
        if (n > MAX_OPTIONS) n = MAX_OPTIONS;

        if (n > 0) {
            r->options = calloc(n, sizeof(TranslationOption));

            for (int i = 0; i < n; i++) {
                json_object *entry = json_object_array_get_idx(dict_entries, i);

                json_object *term;
                if (json_object_object_get_ex(entry, "term", &term)) {
                    r->options[i].translation = strdup(json_object_get_string(term));
                }

                json_object *freq;
                if (json_object_object_get_ex(entry, "frequency", &freq))
                    r->options[i].frequency = json_object_get_int(freq);

                count++;
            }
            r->num_options = count;
        }
    }

    json_object *list;
    if (json_object_object_get_ex(resp, "list", &list) &&
        json_object_get_type(list) == json_type_array) {
        int n = json_object_array_length(list);
        if (n > MAX_EXAMPLES) n = MAX_EXAMPLES;

        int opt_idx = 0;
        if (r->num_options > 0 && *fragment) {
            for (int i = 0; i < r->num_options; i++) {
                if (r->options[i].translation && strcmp(r->options[i].translation, fragment) == 0) {
                    opt_idx = i;
                    break;
                }
            }
        }

        if (r->num_options > 0 && opt_idx < r->num_options) {
            TranslationOption *opt = &r->options[opt_idx];
            opt->source_examples = calloc(n, sizeof(char *));
            opt->target_examples = calloc(n, sizeof(char *));

            for (int i = 0; i < n; i++) {
                json_object *item = json_object_array_get_idx(list, i);
                json_object *s_text, *t_text;

                if (json_object_object_get_ex(item, "s_text", &s_text)) {
                    const char *raw = json_object_get_string(s_text);
                    opt->source_examples[i] = strip_html(raw ? raw : "");
                }
                if (json_object_object_get_ex(item, "t_text", &t_text)) {
                    const char *raw = json_object_get_string(t_text);
                    opt->target_examples[i] = strip_html(raw ? raw : "");
                }
                opt->num_examples++;
            }
        }
    }

    json_object_put(resp);
    free(buf.data);

    return count > 0 ? 0 : -1;
}

void free_context_examples(ContextExamples *ctx) {
    if (!ctx) return;
    free(ctx->error);
    for (int i = 0; i < ctx->num_examples; i++) {
        free(ctx->source_examples[i]);
        free(ctx->target_examples[i]);
    }
    free(ctx->source_examples);
    free(ctx->target_examples);
    free(ctx);
}
