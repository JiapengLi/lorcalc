#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

int print(char *fmt, ...);

typedef enum {
    BOOL_FALSE          = false,
    BOOL_TRUE           = true,
    BOOL_AUTO           = 2,
} auto_bool_t;

typedef struct {
    uint32_t sf;
    uint32_t bw;
    uint32_t cr;
    auto_bool_t ldro;
    auto_bool_t fs;
    auto_bool_t li;
    uint32_t pr;
    bool hdr;
    bool crc;
    uint32_t pl;
} opt_t;

struct option app_long_options[] = {
    {"help",        no_argument,            0,      'h'},
    {"version",     no_argument,            0,      'v'},

    /* abbr*/
    {"bw",                              required_argument,      0,      'b'},
    {"sf",                              required_argument,      0,      's'},
    {"ldro",                            required_argument,      0,      'o'},
    {"fs",                              required_argument,      0,      'f'},           // fine-sync
    {"li",                              required_argument,      0,      'i'},
    {"cr",                              required_argument,      0,      'c'},
    {"pr",                              required_argument,      0,      'r'},
    {"hdr",                             required_argument,      0,      'd'},
    {"crc",                             required_argument,      0,      'k'},
    {"pl",                              required_argument,      0,      'l'},

    /* full */
    {"bandwidth",                       required_argument,      0,      'b'},
    {"spread-factor",                   required_argument,      0,      's'},
    {"low-datarate-optimize",           required_argument,      0,      'o'},
    {"fine-sync",                       required_argument,      0,      'f'},
    {"long-interleaving",               required_argument,      0,      'i'},
    {"coderate",                        required_argument,      0,      'c'},
    {"preamble",                        required_argument,      0,      'r'},
    {"header",                          required_argument,      0,      'd'},
    {"crc",                             required_argument,      0,      'k'},
    {"payload",                         required_argument,      0,      'l'},

    {0,             0,                      0,      0},
};

double lrc(opt_t *opt)
{
    uint32_t bandwidth;
    uint32_t datarate;
    uint8_t coderate;
    uint16_t preambleLen;
    bool fixLen;
    uint8_t payloadLen;
    bool crcOn;

    bandwidth = opt->bw;
    datarate = opt->sf;
    coderate = opt->cr;
    preambleLen = opt->pr;
    fixLen = !opt->hdr;
    crcOn = opt->crc;
    payloadLen = opt->pl;

    double LocalTimeOnAir;
    double symbolPeriod;
    double fec_rate;
    double Nsymbol_header;
    double total_bytes_nb;
    double symbols_nb_preamble;
    double symbols_nb_data;
    double tx_bits_symbol;
    double tx_infobits_header;
    double tx_infobits_payload;

    bool fine_synch;
    bool long_interleaving;
    bool low_datarate_optimize;

    /* option #1: fine sync */
    fine_synch = (datarate <= 6);
    if (opt->fs != BOOL_AUTO) {
        fine_synch = opt->fs;
    } else {
        opt->fs = fine_synch;
    }

    /* option #2: long interleaving */
    long_interleaving = (coderate > 4);
    if (opt->li != BOOL_AUTO) {
        long_interleaving = opt->li;
    } else {
        opt->li = long_interleaving;
    }
    if (long_interleaving) {
        int cr_is_7 = (coderate == 7) ? 1 : 0;
        fec_rate = 4.0 / (coderate + cr_is_7);
    } else {
        fec_rate = 4.0 / (4.0 + coderate);
    }

    /* option #3: low data rate*/
    low_datarate_optimize = (datarate >= 11);
    if (opt->ldro != BOOL_AUTO) {
        low_datarate_optimize = opt->ldro;
    } else {
        opt->ldro = low_datarate_optimize;
    }
    tx_bits_symbol = datarate - 2 * (low_datarate_optimize ? 1 : 0);

    /* header */
    Nsymbol_header = (fixLen == false) ? 20 : 0;

    /* payload and crc */
    total_bytes_nb = payloadLen + 2 * (crcOn  ? 1 : 0);

    tx_infobits_header = (datarate * 4 + (fine_synch ? 1 : 0) * 8 - 8 - Nsymbol_header);

    /* symbol time */
    symbolPeriod = pow(2, datarate) / (double)bandwidth * 1000000;

    if (!long_interleaving) {
        tx_infobits_payload = MAX(0, 8 * total_bytes_nb - tx_infobits_header);
        symbols_nb_data = 8 + ceil(tx_infobits_payload / 4 / tx_bits_symbol) * (coderate + 4);
    } else {
        if (fixLen == false) {
            if (tx_infobits_header < 8 * total_bytes_nb) {
                tx_infobits_header = MIN(tx_infobits_header, 8 * payloadLen);
            }
            tx_infobits_payload = MAX(0, 8 * total_bytes_nb - tx_infobits_header);
            symbols_nb_data = 8 + ceil(tx_infobits_payload / fec_rate / tx_bits_symbol);
        } else {
            double tx_bits_symbol_start = datarate - 2 + 2 * (fine_synch ? 1 : 0);
            double symbols_nb_start = ceil(8 * total_bytes_nb / fec_rate / tx_bits_symbol_start);
            if (symbols_nb_start < 8) {
                symbols_nb_data = symbols_nb_start;
            } else  {
                double tx_codedbits_header = tx_bits_symbol_start * 8;
                double tx_codedbits_payload = 8 * total_bytes_nb / fec_rate - tx_codedbits_header;
                symbols_nb_data = 8 + ceil(tx_codedbits_payload / tx_bits_symbol);
            }
        }
    }

    symbols_nb_preamble = preambleLen + 4.25 + 2 * (fine_synch ? 1 : 0);
    LocalTimeOnAir = (symbols_nb_preamble + symbols_nb_data) * symbolPeriod;

    return LocalTimeOnAir;
}

int print(char *fmt, ...)
{

#define LOG_STRFMT_LEN              (10)

    int i = 0, d, ret, len, j;
    char c, *s;
    uint8_t *hbuf;
    double f;
    char strfmt[LOG_STRFMT_LEN + 2];
    va_list ap;

    if (fmt != NULL) {
        va_start(ap, fmt);
        i = 0;
        while (*fmt) {
            if (*fmt == '%') {
                strfmt[0] = '%';
                j = 1;
                while ((fmt[j] >= '0' && fmt[j] <= '9') ||
                        (fmt[j] == '-') || (fmt[j] == '+') || (fmt[j] == '.')) {
                    strfmt[j] = fmt[j];
                    j++;
                    if (j == LOG_STRFMT_LEN) {
                        break;
                    }
                }
                strfmt[j] = fmt[j];
                fmt += j;
                j++;
                strfmt[j] = '\0';

                switch (*fmt) {
                case '%':
                    ret = printf(strfmt);
                    i += ret;
                    break;
                case 'd':
                    d = va_arg(ap, int);
                    ret = printf(strfmt, d);
                    i += ret;
                    break;
                case 'u':
                    d = va_arg(ap, int);
                    ret = printf(strfmt, (uint32_t)d);
                    i += ret;
                    break;
                case 'x':
                case 'X':
                    d = va_arg(ap, int);
                    ret = printf(strfmt, d);
                    i += ret;
                    break;
                case 'h':
                case 'H':
                    hbuf = va_arg(ap, uint8_t *);
                    len = va_arg(ap, int);
                    for (d = 0; d < len; d++) {
                        if (*fmt == 'h') {
                            ret = printf("%02X", hbuf[d]);
                        } else {
                            ret = printf("%02X ", hbuf[d]);
                        }
                        i += ret;
                    }
                    break;
                case 's':
                    s = va_arg(ap, char *);
                    ret = printf(strfmt, s);
                    i += ret;
                    break;
                case 'c':
                    c = (char)va_arg(ap, int);
                    ret = printf(strfmt, c);
                    i += ret;
                    break;
                case 'f':
                    f = va_arg(ap, double);
                    ret = printf(strfmt, f);
                    i += ret;
                    break;
                }
                fmt++;
            } else {
                fputc(*fmt++, stdout);
                i++;
            }
        }
        va_end(ap);
    }

    printf("\n");
    fflush(stdout);
    i++;


    return i;
}

bool get_u32(const char * const s, uint32_t *v)
{
    char *ptr;
    uint32_t tmp;

    tmp = strtoul(optarg, &ptr, 10);
    if( (*ptr != '\0') || (tmp < 0) ){
        return false;
    }

    *v = tmp;

    return true;
}

bool get_bool(const char * const s, bool *bl)
{
    char *ptr;
    uint32_t tmp;

    if ((0 == strcmp(s, "true")) || (0 == strcmp(s, "on")) ) {
        *bl = true;
    } else if ((0 == strcmp(s, "false")) || (0 == strcmp(s, "off")) ) {
        *bl = false;
    } else if (get_u32(s, &tmp)) {
        *bl = (bool)tmp;
    } else {
        return false;
    }
    return true;
}

const char * const str_auto_bool(auto_bool_t abl)
{
    switch (abl) {
    case BOOL_AUTO:
        return "auto";
    case BOOL_TRUE:
        return "on";
    case BOOL_FALSE:
        return "off";
    }
    return "unknown";
}

const char * const str_bool(bool bl)
{
    if (bl) {
        return "on";
    } else {
        return "off";
    }
}

#define VER_MAJOR          (0)
#define VER_MINOR          (0)
#define VER_PATCH          (1)

enum {
    ERR_PARA            = -1,

};

void usage(void)
{
    print("Usage: lrc [OPTIONS]");
    print(" -h, --help                                      Help");
    print(" -v, --version                                   Version %d.%d.%d", VER_MAJOR, VER_MINOR, VER_PATCH);
    print(" -b, --bw,   --bandwidth                 <uint>  Bandwidth in hz, eg: 125000, 250000, 203125");
    print(" -s, --sf,   --spread-factor             <uint>  Spread factor (5 - 12)");
    print(" -o, --ldro, --low-datarate-optimize     <bool>  Low datarate optimize (on / off)");
    print(" -f, --fs,   --fine-synch                <bool>  Fine synch for high speed lora SF5 and SF6, ");
    print(" -i, --li,   --long-interleaving         <bool>  Available in 2.4GHz lora (sx128x)");
    print(" -c, --cr,   --coderate                  <uint>  coding rate CR4/5 CR4/6... (1 - 4)");
    print(" -r, --pr,   --preamble                  <uint>  Raw preamble lenght");
    print(" -d, --hdr,  --header                    <bool>  Header option (on / off)");
    print(" -k, --crc,                              <bool>  CRC option (on / off)");
    print(" -l, --pl,   --payload                   <uint>  payload length");
}

int main(int argc, char **argv)
{
    opt_t app_opt;
    opt_t *opt;
    int ret, index, hlen, i;
    char *ptr;
    uint32_t tmp;
    bool bl;

    opt = &app_opt;
    opt->sf = 7;
    opt->bw = 125000;
    opt->cr = 1;
    opt->ldro = BOOL_AUTO;
    opt->fs = BOOL_AUTO;
    opt->li = BOOL_AUTO;
    opt->pr = 8;
    opt->hdr = true;
    opt->crc = true;
    opt->pl = 12;

    while (1) {
        ret = getopt_long(argc, argv, ":hvs:b:c:o:f:i:r:d:k:l:", app_long_options, &index);
        if (ret == -1) {
            break;
        }
        switch (ret) {
        case 'h':
            usage();
            return 0;
        case 'v':
            print("v%d.%d.%d", VER_MAJOR, VER_MINOR, VER_PATCH);
            return 0;
        case 's':
            if (!get_u32(optarg, &opt->sf)) {
                return ERR_PARA;
            }
            break;
        case 'b':
            if (!get_u32(optarg, &opt->bw)) {
                return ERR_PARA;
            }
            break;
        case 'c':
            if (!get_u32(optarg, &opt->cr)) {
                return ERR_PARA;
            }
            break;
        case 'o':
            if (!get_bool(optarg, &bl)) {
                return ERR_PARA;
            }
            opt->ldro = bl;
            break;
        case 'f':
            if (!get_bool(optarg, &bl)) {
                return ERR_PARA;
            }
            opt->fs = bl;
            break;
        case 'i':
            if (!get_bool(optarg, &bl)) {
                return ERR_PARA;
            }
            opt->li = bl;
            break;
        case 'r':
            if (!get_u32(optarg, &opt->pr)) {
                return ERR_PARA;
            }
            break;
        case 'd':
            if (!get_bool(optarg, &opt->hdr)) {
                return ERR_PARA;
            }
            break;
        case 'k':
            if (!get_bool(optarg, &opt->crc)) {
                return ERR_PARA;
            }
            break;
        case 'l':
            if (!get_u32(optarg, &opt->pl)) {
                return ERR_PARA;
            }
            break;
        }
    }

    double toa = lrc(opt);

    print("toa: %.0fus, %.3fms, %.3fs", toa, toa / 1000, toa / 1000 / 1000);

    print("sf %u, bw %u, cr %u, ldro %s, fs %s, li %s, pr %u, hdr %s, crc %s, pl %u",
        opt->sf,
        opt->bw,
        opt->cr,
        str_auto_bool(opt->ldro),
        str_auto_bool(opt->fs),
        str_auto_bool(opt->li),
        opt->pr,
        str_bool(opt->hdr),
        str_bool(opt->crc),
        opt->pl
        );

    return 0;
}
