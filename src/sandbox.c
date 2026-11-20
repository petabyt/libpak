/* Pure policy checks: no DNS, socket operations, or platform dependencies. */
#include "main.h"
#include <stdint.h>
#include <string.h>

static int digit(char c) { return c >= '0' && c <= '9'; }
static int hex(char c) {
    if (digit(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int ipv4(const char *s, size_t len, uint8_t out[4]) {
    size_t pos = 0;
    for (int i = 0; i < 4; i++) {
        unsigned n = 0, digits = 0;
        while (pos < len && digit(s[pos])) {
            if (++digits > 3 || (digits > 1 && s[pos - digits + 1] == '0')) return 0;
            n = n * 10 + (unsigned)(s[pos++] - '0');
        }
        if (!digits || n > 255) return 0;
        out[i] = (uint8_t)n;
        if (i < 3 && (pos == len || s[pos++] != '.')) return 0;
    }
    return pos == len;
}
static int ip_parse(const char *s, size_t len, uint8_t out[16]) {
    if (!s || !len) return 0;
    if (!memchr(s, ':', len)) return ipv4(s, len, out) ? 4 : 0;
    uint8_t tmp[16] = {0};
    size_t pos = 0, used = 0, gap = 17;
    if (s[pos] == ':') {
        if (len < 2 || s[1] != ':') return 0;
        gap = 0; pos = 2;
    }
    while (pos < len) {
        size_t start = pos;
        while (pos < len && s[pos] != ':') pos++;
        size_t n = pos - start;
        if (!n) return 0;
        if (memchr(s + start, '.', n)) {
            if (pos != len || used > 12 || !ipv4(s + start, n, tmp + used)) return 0;
            used += 4;
            break;
        }
        if (n > 4 || used > 14) return 0;
        unsigned value = 0;
        for (size_t i = start; i < pos; i++) {
            int d = hex(s[i]);
            if (d < 0) return 0;
            value = value * 16 + (unsigned)d;
        }
        tmp[used++] = (uint8_t)(value >> 8);
        tmp[used++] = (uint8_t)value;
        if (pos == len) break;
        pos++;
        if (pos == len) return 0;
        if (s[pos] == ':') {
            if (gap != 17) return 0;
            gap = used; pos++;
        }
    }
    if (gap != 17) {
        if (used >= 16) return 0;
        memmove(tmp + gap + 16 - used, tmp + gap, used - gap);
        memset(tmp + gap, 0, 16 - used);
    } else if (used != 16) return 0;
    memcpy(out, tmp, 16);
    return 16;
}
static int ip_rule_matches(const char *rule, const uint8_t *ip, int family) {
    if (!rule) return 0;
    const char *slash = strchr(rule, '/');
    size_t len = slash ? (size_t)(slash - rule) : strlen(rule);
    uint8_t network[16];
    if (ip_parse(rule, len, network) != family) return 0;
    unsigned prefix = (unsigned)family * 8;
    if (slash) {
        const char *p = slash + 1;
        if (!*p) return 0;
        prefix = 0;
        while (*p) {
            if (!digit(*p) || prefix > 128) return 0;
            prefix = prefix * 10 + (unsigned)(*p++ - '0');
        }
        if (prefix > (unsigned)family * 8) return 0;
    }
    unsigned whole = prefix / 8, bits = prefix % 8;
    if (memcmp(network, ip, whole)) return 0;
    return !bits || ((network[whole] ^ ip[whole]) & (0xffu << (8 - bits))) == 0;
}
int pak_sandbox_allows_ip(const struct PakSandboxPolicy *policy, const char *ip) {
    uint8_t address[16];
    if (!policy || !policy->rules || !ip) return 0;
    int family = ip_parse(ip, strlen(ip), address);
    if (!family) return 0;
    for (size_t i = 0; i < policy->count; i++)
        if (policy->rules[i].type == PAK_SANDBOX_IP && ip_rule_matches(policy->rules[i].value, address, family)) return 1;
    return 0;
}
/* DNS names are ASCII, case-insensitive, and optionally have a final dot.
 * Require a letter in the name to avoid treating numeric IPs as domains. */
static size_t domain_length(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s), label = 0;
    int letter = 0;
    if (len && s[len - 1] == '.') len--;
    if (!len || len > 253) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '.') {
            if (!label || label > 63 || s[i - 1] == '-') return 0;
            label = 0;
        } else {
            int alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (!alpha && !digit(c) && c != '-') return 0;
            if (!label && c == '-') return 0;
            letter |= alpha; label++;
        }
    }
    return label && label <= 63 && s[len - 1] != '-' && letter ? len : 0;
}
static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c; }
int pak_sandbox_allows_domain(const struct PakSandboxPolicy *policy, const char *domain) {
    size_t len = domain_length(domain);
    if (!policy || !policy->rules || !len) return 0;
    for (size_t i = 0; i < policy->count; i++) {
        const struct PakSandboxRule *r = &policy->rules[i];
        if (r->type != PAK_SANDBOX_DOMAIN || domain_length(r->value) != len) continue;
        size_t j = 0;
        while (j < len && lower(r->value[j]) == lower(domain[j])) j++;
        if (j == len) return 1;
    }
    return 0;
}
