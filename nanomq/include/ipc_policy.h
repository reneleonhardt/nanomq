#ifndef NANOMQ_IPC_POLICY_H
#define NANOMQ_IPC_POLICY_H

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nng/nng.h"
#include "nng/supplemental/nanolib/log.h"
#include "nng/supplemental/util/platform.h"

#ifndef NANO_PLATFORM_WINDOWS
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define IPC_ROOT_ENV "NANOMQ_IPC_ROOT"
#define IPC_NAMESPACE_ENV "NANOMQ_IPC_NAMESPACE"
#define IPC_URL_BUFFER_SIZE 256
#define IPC_NAMESPACE_MAX_LEN 64
#define IPC_ROOT_MAX_LEN 192
#define IPC_INVALID_URL "ipc://"

static inline int
is_valid_ipc_component(const char *value, size_t max_len)
{
	if (value == NULL || value[0] == '\0' || strlen(value) > max_len) {
		return 0;
	}
	for (size_t i = 0; i < strlen(value); i++) {
		unsigned char c = (unsigned char) value[i];
		if (isalnum(c) || c == '_' || c == '-' || c == '.') {
			continue;
		}
		return 0;
	}
	return 1;
}

static inline int
is_valid_ipc_root(const char *root)
{
	if (root == NULL || root[0] != '/' || strlen(root) > IPC_ROOT_MAX_LEN ||
	    strstr(root, "..") != NULL) {
		return 0;
	}
	for (size_t i = 0; i < strlen(root); i++) {
		unsigned char c = (unsigned char) root[i];
		if (isalnum(c) || c == '/' || c == '_' || c == '-' || c == '.') {
			continue;
		}
		return 0;
	}
	return 1;
}

static inline int
is_valid_ipc_url(const char *url)
{
	const char *ipc_prefix = "ipc://";
	size_t      prefix_len  = strlen(ipc_prefix);
	if (url == NULL || url[0] == '\0' || strlen(url) <= prefix_len) {
		return 0;
	}
	if (strncmp(url, ipc_prefix, prefix_len) != 0) {
		return 0;
	}
	return strlen(url + prefix_len) <= NNG_MAXADDRLEN &&
	    is_valid_ipc_root(url + prefix_len);
}

static inline int
is_secure_ipc_root(const char *root)
{
#ifndef NANO_PLATFORM_WINDOWS
	char        canonical_root[PATH_MAX];
	struct stat st;

	if (!is_valid_ipc_root(root) || realpath(root, canonical_root) == NULL ||
	    strcmp(root, canonical_root) != 0 || stat(canonical_root, &st) != 0 ||
	    !S_ISDIR(st.st_mode) || st.st_uid != geteuid() ||
	    (st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
		return 0;
	}
#else
	if (!is_valid_ipc_root(root)) {
		return 0;
	}
#endif
	return 1;
}

static inline const char *
resolve_ipc_url_internal(
    const char *value, const char *label, const char *ipc_target, size_t max_len)
{
	if (value == NULL || value[0] == '\0') {
		return NULL;
	}
	if (strlen(value) >= max_len) {
		log_error("Invalid %s for %s; refusing fallback", label, ipc_target);
		return value;
	}
	if (!is_valid_ipc_url(value)) {
		log_error("Invalid %s=%s for %s; refusing fallback", label, value,
		    ipc_target);
		return value;
	}
	return value;
}

static inline const char *
resolve_ipc_url(const char *env_var, const char *config_url, const char *fallback_url,
    const char *ipc_basename, char *ipc_url, size_t ipc_url_sz)
{
	const char *env_url = getenv(env_var);
	const char *root;
	const char *namespace;
	int         n;

	if (env_url != NULL && env_url[0] != '\0') {
		const char *validated_url =
		    resolve_ipc_url_internal(env_url, env_var, ipc_basename, ipc_url_sz);
		if (validated_url != NULL) {
			return validated_url;
		}
	}

	if (config_url != NULL && config_url[0] != '\0') {
		const char *validated_url =
		    resolve_ipc_url_internal(config_url, "config URL", ipc_basename, ipc_url_sz);
		if (validated_url != NULL) {
			return validated_url;
		}
	}

	root = getenv(IPC_ROOT_ENV);
	if (root != NULL && root[0] != '\0') {
		if (!is_secure_ipc_root(root)) {
			log_warn("Ignoring unsafe %s=%s for %s", IPC_ROOT_ENV, root,
			    ipc_basename);
			return fallback_url;
		}
		namespace = getenv(IPC_NAMESPACE_ENV);
		if (namespace != NULL && namespace[0] != '\0') {
			if (!is_valid_ipc_component(namespace, IPC_NAMESPACE_MAX_LEN)) {
				log_error("Invalid %s=%s for %s; refusing fallback",
				    IPC_NAMESPACE_ENV, namespace, ipc_basename);
				return IPC_INVALID_URL;
			} else {
				n = snprintf(ipc_url, ipc_url_sz, "ipc://%s/%s.%s.ipc", root,
				    ipc_basename, namespace);
				if (n > 0 && (size_t) n < ipc_url_sz && is_valid_ipc_url(ipc_url)) {
					return ipc_url;
				}
				log_error("Unable to construct %s with %s; refusing fallback",
				    ipc_basename, IPC_NAMESPACE_ENV);
				return IPC_INVALID_URL;
			}
		}

		n = snprintf(ipc_url, ipc_url_sz, "ipc://%s/%s.ipc", root, ipc_basename);
		if (n > 0 && (size_t) n < ipc_url_sz && is_valid_ipc_url(ipc_url)) {
			return ipc_url;
		}
	}

	return fallback_url;
}

static inline const char *
resolve_ipc_path(const char *url)
{
	const char *ipc_prefix = "ipc://";
	size_t      prefix_len  = strlen(ipc_prefix);

	if (url != NULL && strncmp(url, ipc_prefix, prefix_len) == 0 &&
	    url[prefix_len] != '\0') {
		return url + prefix_len;
	}
	return NULL;
}

#endif // NANOMQ_IPC_POLICY_H
