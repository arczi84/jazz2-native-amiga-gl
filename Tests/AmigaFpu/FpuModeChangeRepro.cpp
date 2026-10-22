// Compile without linking; definitions must remain in another translation unit.
// Before the fix, late_combine2 replaces an XF reload with an SF read at sp+8.
extern float axis();
extern void change_animation();

bool left()
{
    float x = axis();
    if (__builtin_fabsf(x) > 0.4f) {
        change_animation();
        return x < 0.0f;
    }
    return false;
}
