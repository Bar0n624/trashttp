#include "ssl_utils.h"
#include <stdio.h>
#include <sys/stat.h>

SSL_CTX *init_ssl_context(const char *cert_file, const char *key_file) {
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);

    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }

    if (SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "SSL certificate and key do not match\n");
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

int check_ssl_files(const char *cert_file, const char *key_file) {
    struct stat cert_stat, key_stat;

    if (stat(cert_file, &cert_stat) != 0 || !(S_ISREG(cert_stat.st_mode)) ||
        stat(key_file, &key_stat) != 0 || !(S_ISREG(key_stat.st_mode))) {
        return 0;
        }

    return 1;
}
