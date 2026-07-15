/* endian.h — minimal byte-order macros.
 *
 * musl's src/string/memcpy.c includes <endian.h> only for __BYTE_ORDER /
 * __LITTLE_ENDIAN, to select the misaligned shift-copy direction. We provide
 * just those (musl's full endian.h pulls <features.h> + byte-swap helpers we
 * don't need) and map them to clang's per-target built-ins, so the choice is
 * correct on every target we build. */
#ifndef _ENDIAN_H
#define _ENDIAN_H

#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN    __ORDER_BIG_ENDIAN__
#define __BYTE_ORDER    __BYTE_ORDER__

#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN    __BIG_ENDIAN
#define BYTE_ORDER    __BYTE_ORDER

#endif
