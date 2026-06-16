#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#define MAX_LANG 16
#define MAX_TEXT 4096
#define MAX_OPTIONS 64
#define MAX_EXAMPLES 128

typedef struct {
    char *translation;
    int occurrence_count;
    int frequency;
    char **source_examples;
    char **target_examples;
    int num_examples;
} TranslationOption;

typedef struct {
    char input_text[MAX_TEXT];
    char source_lang[MAX_LANG];
    char target_lang[MAX_LANG];
    char detected_language[MAX_LANG];
    int direction_confidence;
    char **translations;
    int num_translations;
    TranslationOption *options;
    int num_options;
} TranslationResponse;

const char *lang_to_code(const char *lang);
const char *code_to_lang(const char *code);
TranslationResponse *translate_text(const char *text, const char *source, const char *target);
void free_translation_response(TranslationResponse *r);

extern const char *SUPPORTED_LANGUAGES[];

#endif
