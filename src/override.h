#ifndef FORCE_LOG_OVERRIDE_H
#define FORCE_LOG_OVERRIDE_H

#include "plat.h"
#include "log.h"

#ifdef NCNN_LOGE
#undef NCNN_LOGE
#endif

#define NCNN_LOGE(...) logwarning(##__VA_ARGS__)

#endif // FORCE_LOG_OVERRIDE_H