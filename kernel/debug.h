#ifndef KERNEL_DEBUG_H__
#define KERNEL_DEBUG_H__

void panic_spin(const char *filename, int line, const char *func, const char *condition);

#ifdef NDEBUG
#define ASSERT(CONDITION) ((void)0)
#else
#define PANIC(...) panic_spin (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define ASSERT(CONDITION) \
if (CONDITION) {} else {  \
    PANIC(#CONDITION);    \
}
#endif  //NDEBUG

#endif
