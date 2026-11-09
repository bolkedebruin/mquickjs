/*
 * JS_RebaseBytecodeWindow() test: rebasing bytecode relocated at A to B
 * must give exactly the bytecode relocated at B, whatever the window size;
 * with the ROM table "moved" by D, exactly the ROM pointers move by D (and
 * moving them back restores it); a pointer outside every range fails.
 *
 * usage: test_rebase <bytecode file from mqjs -o>
 *
 * Built from mqjs.c (its stdlib and C functions) with its main() renamed.
 */
#define main mqjs_main
#include "../mqjs.c"
#undef main

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

/* rebase 'src' (relocated, len bytes) into 'dst' with windows of 'win' bytes */
static int rebase(const uint8_t *src, uint8_t *dst, uint32_t len, uint32_t win,
                  const JSRebaseRange *r, int n, uintptr_t new_base)
{
    uint32_t off;

    for (off = 0; off < len; off += win) {
        uint32_t w = len - off < win ? len - off : win;
        memcpy(dst + off, src + off, w);
        if (JS_RebaseBytecodeWindow(src, len, dst + off, off, w, r, n, new_base))
            return -1;
    }
    return 0;
}

static uint8_t *relocated_copy(JSContext *ctx, const uint8_t *pi, int len)
{
    uint8_t *buf = malloc(len);
    memcpy(buf, pi, len);
    if (JS_RelocateBytecode(ctx, buf, len)) {
        fprintf(stderr, "Could not relocate bytecode\n");
        exit(1);
    }
    return buf;
}

int main(int argc, const char **argv)
{
    static const uint32_t windows[] = { sizeof(JSValue), 64, 4096, 1u << 30 };
    const size_t mem_size = 1 << 20;
    const uint32_t hdr = sizeof(JSBytecodeHeader);
    uint8_t *pi, *a, *b, *out, *back, *mem;
    JSContext *ctx;
    uintptr_t rom, rom_end, D;
    int len, i, moved = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: test_rebase <bytecode file>\n");
        return 2;
    }
    pi = load_file(argv[1], &len);
    mem = malloc(mem_size);
    ctx = JS_NewContext(mem, mem_size, &js_stdlib);
    JS_SetLogFunc(ctx, js_log_func);

    a = relocated_copy(ctx, pi, len);
    b = relocated_copy(ctx, pi, len);
    out = malloc(len);
    back = malloc(len);
    rom = (uintptr_t)js_stdlib.stdlib_table;
    rom_end = rom + js_stdlib.stdlib_table_len * sizeof(JSWord);

    /* A -> B, ROM unchanged: equal to the bytecode relocated at B */
    for (i = 0; i < (int)countof(windows); i++) {
        JSRebaseRange r[2] = {
            { (uintptr_t)a + hdr, (uintptr_t)a + len, (uintptr_t)b - (uintptr_t)a },
            { rom, rom_end, 0 },
        };
        memset(out, 0xa5, len);
        CHECK(rebase(a, out, len, windows[i], r, 2, (uintptr_t)b + hdr) == 0,
              "rebase A->B failed (window %u)", windows[i]);
        CHECK(memcmp(out, b, len) == 0, "A->B differs from B (window %u)", windows[i]);
    }

    /* A -> B with the ROM table moved by D: only ROM pointers move, by D */
    D = 0x10000;
    {
        JSRebaseRange r[2] = {
            { (uintptr_t)a + hdr, (uintptr_t)a + len, (uintptr_t)b - (uintptr_t)a },
            { rom, rom_end, D },
        };
        CHECK(rebase(a, out, len, 64, r, 2, (uintptr_t)b + hdr) == 0, "rebase with ROM move failed");
        for (i = 0; i + (int)sizeof(JSValue) <= len; i += sizeof(JSValue)) {
            JSValue vb, vo;
            memcpy(&vb, b + i, sizeof(vb));
            memcpy(&vo, out + i, sizeof(vo));
            if (vb == vo)
                continue;
            moved++;
            CHECK(JS_IsPtr(vb) && (uintptr_t)JS_VALUE_TO_PTR(vb) >= rom &&
                  (uintptr_t)JS_VALUE_TO_PTR(vb) < rom_end && vo == vb + D,
                  "word at %d changed but is no ROM pointer moved by D", i);
        }
        CHECK(moved > 0, "no ROM pointer found to move");
    }
    /* ... and back */
    {
        JSRebaseRange r[2] = {
            { (uintptr_t)b + hdr, (uintptr_t)b + len, 0 },
            { rom + D, rom_end + D, (uintptr_t)0 - D },
        };
        CHECK(rebase(out, back, len, 4096, r, 2, (uintptr_t)b + hdr) == 0, "rebase back failed");
        CHECK(memcmp(back, b, len) == 0, "moving the ROM back does not restore B");
    }

    /* a ROM pointer outside every range: refused */
    {
        JSRebaseRange r[1] = {
            { (uintptr_t)a + hdr, (uintptr_t)a + len, (uintptr_t)b - (uintptr_t)a },
        };
        CHECK(rebase(a, out, len, 4096, r, 1, (uintptr_t)b + hdr) != 0,
              "pointer outside the ranges was not refused");
    }

    /* the reference runs */
    {
        JSValue val = JS_LoadBytecode(ctx, b);
        CHECK(!JS_IsException(val), "load failed");
        if (!JS_IsException(val))
            CHECK(!JS_IsException(JS_Run(ctx, val)), "run failed");
    }

    if (failures == 0)
        printf("rebase: OK (%d ROM pointers moved, %d bytes)\n", moved, len);
    return failures ? 1 : 0;
}
