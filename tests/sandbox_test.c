#include <assert.h>
#include "../src/main.h"

int main(void) {
    const struct PakSandboxRule rules[] = {
        { PAK_SANDBOX_IP, "192.168.1.42/24" },
        { PAK_SANDBOX_IP, "10.0.0.1" },
        { PAK_SANDBOX_IP, "2001:db8:abcd::/48" },
        { PAK_SANDBOX_IP, "::ffff:192.0.2.1" },
        { PAK_SANDBOX_DOMAIN, "Camera.Example." },
        { PAK_SANDBOX_DOMAIN, "*.example.org" },
        { PAK_SANDBOX_IP, "0.0.0.0/33" },
    };
    struct PakSandboxPolicy p = { rules, sizeof(rules) / sizeof(rules[0]) };
    assert(pak_sandbox_allows_ip(&p, "192.168.1.255"));
    assert(!pak_sandbox_allows_ip(&p, "192.168.2.1"));
    assert(pak_sandbox_allows_ip(&p, "10.0.0.1"));
    assert(!pak_sandbox_allows_ip(&p, "10.0.0.2"));
    assert(pak_sandbox_allows_ip(&p, "2001:DB8:abcd:1::1"));
    assert(!pak_sandbox_allows_ip(&p, "2001:db8:abce::1"));
    assert(pak_sandbox_allows_ip(&p, "::ffff:c000:201"));
    assert(!pak_sandbox_allows_ip(&p, "192.0.2.1"));
    assert(!pak_sandbox_allows_ip(&p, "192.168.01.1"));
    assert(!pak_sandbox_allows_ip(&p, "2001:db8:::1"));
    assert(!pak_sandbox_allows_ip(&p, "1.2.3.4/24"));
    assert(pak_sandbox_allows_domain(&p, "camera.example"));
    assert(pak_sandbox_allows_domain(&p, "CAMERA.EXAMPLE."));
    assert(!pak_sandbox_allows_domain(&p, "sub.camera.example"));
    assert(!pak_sandbox_allows_domain(&p, "x.example.org"));
    assert(!pak_sandbox_allows_domain(&p, "camera.example.."));
    assert(!pak_sandbox_allows_domain(&p, "192.168.1.1"));
    assert(!pak_sandbox_allows_domain(&p, NULL));
    assert(!pak_sandbox_allows_ip(NULL, "10.0.0.1"));
    struct PakSandboxPolicy empty = { NULL, 0 };
    assert(!pak_sandbox_allows_ip(&empty, "10.0.0.1"));
    const struct PakSandboxRule any[] = { { PAK_SANDBOX_IP, "::/0" }, { PAK_SANDBOX_IP, "0.0.0.0/0" } };
    struct PakSandboxPolicy all = { any, 2 };
    assert(pak_sandbox_allows_ip(&all, "::1"));
    assert(pak_sandbox_allows_ip(&all, "255.255.255.255"));
    return 0;
}
