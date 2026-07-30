#ifndef WEBHOOK_INPROC_H
#define WEBHOOK_INPROC_H

#include "nng/supplemental/nanolib/conf.h"
#include "nng/nng.h"
#include "include/ipc_policy.h"

#define HOOK_IPC_URL "ipc:///tmp/nanomq_hook.ipc"
// Override via env: NANOMQ_HOOK_IPC_URL
#define HOOK_IPC_URL_ENV "NANOMQ_HOOK_IPC_URL"
#define HOOK_IPC_BASENAME "nanomq_hook"

extern int start_hook_service(conf *conf);
extern int stop_hook_service(void);

#endif
