#include <stdio.h>

extern bool left_bad();
extern bool left_local();
extern bool left_fixed();
extern bool left_workaround();

static volatile float current_axis;
static volatile float clobber_value = 0.75f;

float axis()
{
    return current_axis;
}

void change_animation()
{
    // Simulate a call that actually destroys both call-clobbered FP registers.
    __asm__ volatile ("fmove.s %0,%%fp0\n\tfmove.s %0,%%fp1"
                      : : "m" (clobber_value) : "fp0", "fp1");
}

int main()
{
    struct Case { unsigned long bits; int expected; };
    static const Case cases[] = {
        {0xbf800000UL, 1}, // -1
        {0x3f800000UL, 0}, // +1
        {0xbf000000UL, 1}, // -0.5
        {0x3f000000UL, 0}, // +0.5
        {0xc0000000UL, 1}, // -2
        {0x40000000UL, 0}, // +2
        {0xbf400000UL, 1}, // -0.75
        {0x3f400000UL, 0}, // +0.75
        {0xbecccccdUL, 0}, // -float(0.4), threshold is strict
        {0x3ecccccdUL, 0}, // +float(0.4)
        {0xbeccccceUL, 1}, // next float below -float(0.4)
        {0x3eccccceUL, 0}, // next float above +float(0.4)
        {0xbe800000UL, 0}, // -0.25
        {0x3e800000UL, 0}, // +0.25
        {0x80000000UL, 0}, // -0
        {0x00000000UL, 0}, // +0
    };
    unsigned bad = 0, local_bad = 0, fixed_bad = 0, workaround_bad = 0;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        union { unsigned long bits; float value; } input;
        input.bits = cases[i].bits;
        current_axis = input.value;
        int a = left_bad();
        int b = left_local();
        int c = left_fixed();
        int d = left_workaround();
        bad += a != cases[i].expected;
        local_bad += b != cases[i].expected;
        fixed_bad += c != cases[i].expected;
        workaround_bad += d != cases[i].expected;
        printf("bits=%08lx expected=%d rc9=%d local=%d fixed=%d workaround=%d\n",
               cases[i].bits, cases[i].expected, a, b, c, d);
    }
    printf("SUMMARY cases=16 rc9_errors=%u local_errors=%u fixed_errors=%u workaround_errors=%u\n",
           bad, local_bad, fixed_bad, workaround_bad);
    const bool ok = bad != 0 && local_bad != 0 && fixed_bad == 0 && workaround_bad == 0;
    printf("RESULT %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 10;
}
