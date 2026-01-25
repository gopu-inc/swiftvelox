#ifndef HTTP_H
#define HTTP_H

void init_http_module(void);
char* http_get(const char* url);
char* http_post(const char* url, const char* json_data);
char* http_download(const char* url, const char* output_filename);

#endif
