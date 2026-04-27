#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "qsl_card.h"

#define QSL_CARD_LINE_BUF 64

static uint8_t qsl_line_buf[QSL_CARD_LINE_BUF];

static const char *skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static void card_write_line(tty_t *ttyp, const char *text, const char *lr)
{
    char left = '|';
    char right = '|';

    if (lr && lr[0] && lr[1]) {
        left = lr[0];
        right = lr[1];
    }

    int n = snprintf((char *)qsl_line_buf, QSL_CARD_LINE_BUF,
                     "  %c %-46.46s %c\r\n",
                     left, text ? text : "", right);
    tty_write(ttyp, qsl_line_buf, n);
}

static const char *find_qsl_object(const char *json)
{
    const char *p = json;
    bool in_string = false;
    bool esc = false;

    while (*p) {
        if (in_string) {
            if (esc) esc = false;
            else if (*p == '\\') esc = true;
            else if (*p == '"') in_string = false;
            p++;
            continue;
        }
        if (*p == '"') {
            if (!strncmp(p, "\"QSL\"", 5)) {
                const char *q = skip_ws(p + 5);
                if (*q != ':') {
                    p++;
                    continue;
                }
                q = skip_ws(q + 1);
                if (*q == '{') return q;
            }
            in_string = true;
        }
        p++;
    }
    return NULL;
}

static const char *json_copy_string(const char *p, char *out, int out_sz)
{
    int n = 0;
    bool esc = false;

    if (*p != '"') return NULL;
    p++;
    while (*p) {
        char ch = *p++;
        if (esc) {
            if (n < out_sz - 1) out[n++] = ch;
            esc = false;
            continue;
        }
        if (ch == '\\') {
            esc = true;
            continue;
        }
        if (ch == '"') {
            out[n] = '\0';
            return p;
        }
        if (n < out_sz - 1) out[n++] = ch;
    }
    return NULL;
}

static const char *json_value_end(const char *p)
{
    int depth = 0;
    bool in_string = false;
    bool esc = false;

    while (*p) {
        char ch = *p;
        if (in_string) {
            if (esc) esc = false;
            else if (ch == '\\') esc = true;
            else if (ch == '"') in_string = false;
            p++;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            p++;
            continue;
        }
        if (ch == '{' || ch == '[') depth++;
        else if (ch == '}' || ch == ']') {
            if (depth == 0) return p;
            depth--;
        } else if (ch == ',' && depth == 0) return p;
        p++;
    }
    return p;
}

static void qsl_value_set(char *dst, int dst_sz, const char *src)
{
    if (!dst || dst_sz <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

static bool json_find_top_level_string(const char *json, const char *key, char *out, int out_sz)
{
    const char *p = json;
    int depth = 0;
    bool in_string = false;
    bool esc = false;

    if (!json || !key || !out || out_sz <= 0) return false;
    out[0] = '\0';

    while (*p) {
        char ch = *p;
        if (in_string) {
            if (esc) esc = false;
            else if (ch == '\\') esc = true;
            else if (ch == '"') in_string = false;
            p++;
            continue;
        }
        if (ch == '"') {
            if (depth == 1) {
                char found_key[8];
                const char *q = json_copy_string(p, found_key, sizeof(found_key));
                const char *v;
                if (!q) return false;
                v = skip_ws(q);
                if (*v == ':') {
                    v = skip_ws(v + 1);
                    if (!strcmp(found_key, key) && *v == '"') {
                        if (!json_copy_string(v, out, out_sz)) return false;
                        return true;
                    }
                }
                p = q;
                continue;
            }
            in_string = true;
            p++;
            continue;
        }
        if (ch == '{') depth++;
        else if (ch == '}' && depth > 0) depth--;
        p++;
    }
    return false;
}

bool qsl_card_parse(const char *json, qsl_card_t *card)
{
    const char *p;

    if (!json || !card) return false;

    memset(card, 0, sizeof(*card));
    if (json_find_top_level_string(json, "FR", card->from_fr, sizeof(card->from_fr))) {
        card->has_fr = true;
    }
    p = find_qsl_object(json);
    if (!p) return false;

    p++;
    while (*p) {
        char key[8];
        char val[48];
        const char *vend;

        p = skip_ws(p);
        if (*p == '}') {
            card->has_qsl = true;
            return true;
        }
        if (*p != '"') return false;
        p = json_copy_string(p, key, sizeof(key));
        if (!p) return false;
        p = skip_ws(p);
        if (*p != ':') return false;
        p = skip_ws(p + 1);
        if (*p == '"') {
            p = json_copy_string(p, val, sizeof(val));
            if (!p) return false;
        } else {
            vend = json_value_end(p);
            snprintf(val, sizeof(val), "%.*s", (int)(vend - p), p);
            p = vend;
        }

        if (!strcmp(key, "C")) qsl_value_set(card->to_call, sizeof(card->to_call), val);
        else if (!strcmp(key, "S")) qsl_value_set(card->report, sizeof(card->report), val);
        else if (!strcmp(key, "D")) qsl_value_set(card->date, sizeof(card->date), val);
        else if (!strcmp(key, "T")) qsl_value_set(card->time, sizeof(card->time), val);
        else if (!strcmp(key, "F")) qsl_value_set(card->freq, sizeof(card->freq), val);
        else if (!strcmp(key, "M")) qsl_value_set(card->mode, sizeof(card->mode), val);
        else if (!strcmp(key, "P")) qsl_value_set(card->qth, sizeof(card->qth), val);
        else if (card->ext_n < QSL_CARD_EXT_MAX) {
            snprintf(card->ext[card->ext_n], sizeof(card->ext[card->ext_n]), "%s: %s", key, val);
            card->ext_n++;
        }

        p = skip_ws(p);
        if (*p == ',') p++;
    }

    return false;
}

static void card_write_wrapped45(tty_t *ttyp, const char *text)
{
    size_t len;
    size_t off = 0;
    char line[80];

    if (!text) {
        card_write_line(ttyp, "", NULL);
        return;
    }

    len = strlen(text);
    if (len == 0) {
        card_write_line(ttyp, "", NULL);
        return;
    }

    while (off < len) {
        size_t chunk = len - off;
        if (chunk > 45) chunk = 45;
        snprintf(line, sizeof(line), "%.*s", (int)chunk, text + off);
        card_write_line(ttyp, line, NULL);
        off += chunk;
    }
}

void qsl_card_render(tty_t *ttyp, const qsl_card_t *card, const char *from, const char *addr, const char *raw_data, const char *sig_b64, const char *status)
{
    char line[80];

    if (!ttyp || !card || !from || !addr || !sig_b64 || !status) return;

    tty_write_str(ttyp, "\r\n");
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    card_write_line(ttyp, "Digitally Signed QSL Card", NULL);
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    snprintf(line, sizeof(line), "From     : %s", from);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "To Call  : %s", card->to_call);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "Report   : %s", card->report);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "Date     : %s", card->date);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "Time     : %s", card->time);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "Freq     : %s", card->freq);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "Mode     : %s", card->mode);
    card_write_line(ttyp, line, NULL);
    snprintf(line, sizeof(line), "QTH      : %s", card->qth);
    card_write_line(ttyp, line, NULL);
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    card_write_line(ttyp, "Extended entries", NULL);
    for (int i = 0; i < card->ext_n; i++) {
        card_write_line(ttyp, card->ext[i], NULL);
    }
    if (card->ext_n == 0) card_write_line(ttyp, "", NULL);
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    card_write_line(ttyp, "Raw Data", NULL);
    card_write_wrapped45(ttyp, raw_data);
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    card_write_line(ttyp, "Signature", NULL);
    card_write_wrapped45(ttyp, sig_b64);
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    card_write_line(ttyp, "Signed ID (Mona address)", "><");
    card_write_line(ttyp, addr, "><");
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    if (!strcmp(status, "OK")) {
        static const char *suffix = "Confirming Our QSO.";
        int left_n = snprintf(line, sizeof(line), "Status   : %s", status);
        int pad_n = 46 - left_n - (int)strlen(suffix);
        if (pad_n < 1) pad_n = 1;
        snprintf(line, sizeof(line), "Status   : %s%*s%s", status, pad_n, "", suffix);
    } else {
        snprintf(line, sizeof(line), "Status   : %s", status);
    }
    card_write_line(ttyp, line, NULL);
    tty_write_str(ttyp, "  +------------------------------------------------+\r\n");
    tty_write_str(ttyp, "\r\n");
}
