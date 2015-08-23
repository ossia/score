#pragma once
#include <QDebug>
#define ISCORE_TODO do { qDebug() << "TODO"; } while (0)
#if defined(ISCORE_DEBUG)
#define ISCORE_BREAKPOINT do { __asm__("int3"); } while (0)
#else
#define ISCORE_BREAKPOINT do {} while (0)
#endif
