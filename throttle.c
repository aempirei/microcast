/* throttle
 * bandwidth throttling pipe utility
 * Copyright(c) 2012 by Christopher Abad | 20 GOTO 10
 *
 * email: aempirei@gmail.com aempirei@256.bz
 * http://www.256.bz/ http://www.twentygoto10.com/
 * git: git@github.com:aempirei/microcast.git
 * aim: ambientempire
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <sys/time.h>

typedef struct timeval timeval_t;

typedef struct configuration {
    int32_t rate;
    int input_buffering;
    int verbose;
    timeval_t started;
    timeval_t elapsed;
    int32_t bytes;
} configuration_t;

typedef struct state {
    timeval_t started;
    timeval_t elapsed;
    int32_t bytes;
    char *buffer;
    size_t buffer_sz;
} state_t;

const state_t initial_state = {
    .bytes = 0,
    .buffer = NULL,
    .buffer_sz = BUFSIZ * 16
};

const configuration_t default_config = {
    .rate = 5,
    .input_buffering = 1,
    .verbose = 0,
    .bytes = 0
};

const char *default_action(int default_value) {
    return default_value ? "disable" : "enable";
}

void usage_print(const char *option_str, const char *action, const char *option_desc) {

    const int option_width = -11;

    fprintf(stderr, "\t%*s%s %s\n", option_width, option_str, action, option_desc);
}

void usage(const char *arg0) {

    fprintf(stderr, "\nusage: %s [options]\n\n", arg0);

    usage_print("-h", "show", "this help");
    usage_print("-v", default_action(default_config.verbose), "verbose");
    usage_print("-b", default_action(default_config.input_buffering), "input buffering");
    usage_print("-r rate", "set", "rate in Bps (floating point values and the prefixes K, Ki, M & Mi are all accepted)");

    fputc('\n', stderr);
}

int32_t parse_rate(const char *str) {

    typedef struct {
        char *k;
        int32_t v;
    } kv_t;

    kv_t prefix[] = {
        {"Mi", 1024 * 1024},
        {"Ki", 1024},
        {"M", 1000 * 1000},
        {"K", 1000},
        {"k", 1000},
        {"", 1},
        {NULL, 0}
    };

    int32_t scale;
    int32_t rate;
    char *token;

    const char *p = str;
    int is_float = 0;

    /* format: ^(?-i)([1-9]\d*|0)(\.\d+|)([MK]i?|k|)$ */

    if (*p == '0') {
        p++;
    } else if (*p >= '1' && *p <= '9') {
        while (isdigit(*++p))
            /* nothing */ ;
    } else {
        return -1;
    }

    if (*p == '.') {
        if (!isdigit(*++p))
            return -1;
        is_float = 1;
        while (isdigit(*++p))
            /* nothing */ ;
    }

    for (kv_t * pi = prefix; pi->k != NULL; pi++) {
        if (strcmp(pi->k, p) == 0) {
            scale = pi->v;
            p += strlen(pi->k);
            break;
        }
    }

    if (*p != '\0')
        return -1;

    token = strdup(str);
    token[p - str] = '\0';

    if (is_float) {
        rate = (int32_t) rint(strtod(token, NULL) * scale);
    } else {
        rate = strtoul(token, NULL, 0) * scale;
    }

    free(token);

    return rate;
}

int cliconfig(configuration_t * config, int argc, char **argv) {

    int opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, "hvbr:")) != -1) {
        switch (opt) {
        case 'v':
            config->verbose = !default_config.verbose;
            break;
        case 'b':
            config->input_buffering = !default_config.input_buffering;
            break;
        case 'r':
            config->rate = parse_rate(optarg);
            if (config->rate == -1) {
                fprintf(stderr, "invalid rate: %s\n", optarg);
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        case '?':
            fprintf(stderr, "unknown option: -%c\n", optopt);
            usage(argv[0]);
            exit(EXIT_FAILURE);
        default:
            fprintf(stderr, "unimplemented option: -%c\n", opt);
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    return optind;
}

void getelapsedtime(timeval_t * elapsed, timeval_t * started) {

    timeval_t current;

    gettimeofday(&current, NULL);

    timersub(&current, started, elapsed);
}

void throttle(state_t * state, configuration_t * config, FILE * fpin, FILE * fpout) {

    int ch;

    if (config->verbose)
        fprintf(stderr, "throttling stdin at %dBps\n", config->rate);

    if (config->input_buffering)
        setvbuf(stdin, state->buffer, _IOFBF, state->buffer_sz);

    ch = fgetc(fpin);
    fputc(ch, fpout);

    getelapsedtime(&(config->elapsed), &(config->started));
}

void eprintf(int errnum, const char *format, ...) {

    va_list ap;
    char eb[256];
    char s[256];

    if (strerror_r(errnum, eb, sizeof(eb)) == -1) {
        perror("strerror_r()");
        return;
    }

    va_start(ap, format);
    vsnprintf(s, sizeof(s), format, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s\n", s, eb);
}

int main(int argc, char **argv) {

    const char fopen_mode[] = "r";

    configuration_t config = default_config;
    state_t state = initial_state;

    FILE *fp;
    int files_index;

    files_index = cliconfig(&config, argc, argv);

    if (config.input_buffering)
        state.buffer = malloc(state.buffer_sz);

    gettimeofday(&config.started, NULL);

    if (files_index == argc) {

        throttle(&state, &config, stdin, stdout);

    } else {

        for (int i = files_index; i < argc; i++) {

            if ((fp = fopen(argv[i], fopen_mode)) == NULL) {
                eprintf(errno, "fopen(\"%s\",\"%s\")", argv[i], fopen_mode);
            } else {
                throttle(&state, &config, fp, stdout);
                fclose(fp);
            }
        }

    }

    exit(EXIT_SUCCESS);
}
