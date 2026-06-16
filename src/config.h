#ifndef CONFIG_H
#define CONFIG_H

char *load_config_target_lang(void);
void save_config_target_lang(const char *lang);
char *load_config_last_source_lang(void);
void save_config_last_source_lang(const char *lang);

#endif
