#ifndef SSL_UTILS_H
#define SSL_UTILS_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>

SSL_CTX *init_ssl_context(const char *cert_file, const char *key_file);
int check_ssl_files(const char *cert_file, const char *key_file);

#endif
