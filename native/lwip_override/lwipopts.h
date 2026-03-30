/**
 * Shadow nxdk's lwipopts.h: include it once, then undef LWIP_DEBUG so lwIP does not
 * call LWIP_PLATFORM_DIAG -> debugPrint (same framebuffer as the QR).
 *
 * Requires nxdk as sibling of this repo (../../nxdk from native/lwip_override). If NXDK_DIR differs,
 * edit this path.
 */
#ifndef LWIP_LWIPOPTS_H
#include "../../nxdk/lib/net/nforceif/include/lwipopts.h"
#undef LWIP_DEBUG
#endif
