/**
 * @file platform.c
 * @brief TRTC-style platform abstraction layer implementation for Linux/GCC
 * @author tinyRtc team
 * @date 2026-04-26
 */

#define _GNU_SOURCE
#include "platform.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/*==============================================================================
 * Internal Structures
 *============================================================================*/

typedef struct {
    int fd;
    int type;
    int family;
} trtc_socket_internal_t;

/*==============================================================================
 * Global State
 *============================================================================*/

static bool g_trtc_initialized = false;
static pthread_mutex_t g_trtc_mutex = PTHREAD_MUTEX_INITIALIZER;

/*==============================================================================
 * Thread API Implementation
 *============================================================================*/

trtc_result_t trtc_thread_create(
    trtc_thread_func_t func,
    void* args,
    size_t stack_size,
    const char* name,
    trtc_thread_t* thread)
{
    trtc_log("Creating thread, stack_size=%zu, name=%s", stack_size, name ? name : "null");

    if (!func || !thread) {
        trtc_log("Invalid parameters");
        return trtc_INVALID_PARAM;
    }

    pthread_t* tid = (pthread_t*)trtc_malloc(sizeof(pthread_t));
    if (!tid) {
        trtc_log("Failed to allocate memory for thread");
        return trtc_NO_MEMORY;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    if (stack_size > 0) {
        pthread_attr_setstacksize(&attr, stack_size);
    }

    int ret = pthread_create(tid, &attr, func, args);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        trtc_free(tid);
        return trtc_ERROR;
    }

    *thread = (trtc_thread_t)tid;
    
    /* Try to set thread name (Linux specific) */
    (void)name;  /* Suppress unused warning when PR_SET_NAME is not defined */
    #ifdef PR_SET_NAME
    if (name) {
        pthread_setname_np(*tid, name);
    }
    #endif
    
    return trtc_OK;
}

trtc_result_t trtc_thread_join(trtc_thread_t thread, void** retval)
{
    if (!thread) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_join((pthread_t)thread, retval);
    if (ret != 0) {
        return trtc_ERROR;
    }

    trtc_free(thread);
    return trtc_OK;
}

trtc_result_t trtc_thread_detach(trtc_thread_t thread)
{
    if (!thread) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_detach((pthread_t)thread);
    if (ret != 0) {
        return trtc_ERROR;
    }

    return trtc_OK;
}

trtc_thread_t trtc_thread_get_current(void)
{
    pthread_t tid = pthread_self();
    pthread_t* tid_ptr = (pthread_t*)trtc_malloc(sizeof(pthread_t));
    if (tid_ptr) {
        *tid_ptr = tid;
    }
    return (trtc_thread_t)tid_ptr;
}

void trtc_thread_sleep(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void trtc_thread_yield(void)
{
    sched_yield();
}

/*==============================================================================
 * Mutex API Implementation
 *============================================================================*/

trtc_result_t trtc_mutex_create(trtc_mutex_t* mutex)
{
    if (!mutex) {
        return trtc_INVALID_PARAM;
    }

    pthread_mutex_t* mtx = (pthread_mutex_t*)trtc_malloc(sizeof(pthread_mutex_t));
    if (!mtx) {
        return trtc_NO_MEMORY;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);

    int ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);

    if (ret != 0) {
        trtc_free(mtx);
        return trtc_ERROR;
    }

    *mutex = (trtc_mutex_t)mtx;
    return trtc_OK;
}

trtc_result_t trtc_mutex_destroy(trtc_mutex_t mutex)
{
    if (!mutex) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_mutex_destroy((pthread_mutex_t*)mutex);
    trtc_free(mutex);
    
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

trtc_result_t trtc_mutex_lock(trtc_mutex_t mutex)
{
    if (!mutex) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_mutex_lock((pthread_mutex_t*)mutex);
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

trtc_result_t trtc_mutex_trylock(trtc_mutex_t mutex)
{
    if (!mutex) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_mutex_trylock((pthread_mutex_t*)mutex);
    if (ret == EBUSY) {
        return trtc_ERROR;
    }
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

trtc_result_t trtc_mutex_unlock(trtc_mutex_t mutex)
{
    if (!mutex) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_mutex_unlock((pthread_mutex_t*)mutex);
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

/*==============================================================================
 * Condition Variable API Implementation
 *============================================================================*/

trtc_result_t trtc_cond_create(trtc_cond_t* cond)
{
    if (!cond) {
        return trtc_INVALID_PARAM;
    }

    pthread_cond_t* cnd = (pthread_cond_t*)trtc_malloc(sizeof(pthread_cond_t));
    if (!cnd) {
        return trtc_NO_MEMORY;
    }

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);

    int ret = pthread_cond_init(cnd, &attr);
    pthread_condattr_destroy(&attr);

    if (ret != 0) {
        trtc_free(cnd);
        return trtc_ERROR;
    }

    *cond = (trtc_cond_t)cnd;
    return trtc_OK;
}

trtc_result_t trtc_cond_destroy(trtc_cond_t cond)
{
    if (!cond) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_cond_destroy((pthread_cond_t*)cond);
    trtc_free(cond);
    
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

trtc_result_t trtc_cond_wait(trtc_cond_t cond, trtc_mutex_t mutex, uint32_t timeout_ms)
{
    if (!cond || !mutex) {
        return trtc_INVALID_PARAM;
    }

    int ret;
    if (timeout_ms == 0) {
        ret = pthread_cond_wait((pthread_cond_t*)cond, (pthread_mutex_t*)mutex);
    } else {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        struct timespec ts;
        ts.tv_sec = now.tv_sec + timeout_ms / 1000;
        ts.tv_nsec = now.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += ts.tv_nsec / 1000000000;
            ts.tv_nsec %= 1000000000;
        }
        
        ret = pthread_cond_timedwait((pthread_cond_t*)cond, (pthread_mutex_t*)mutex, &ts);
        
        if (ret == ETIMEDOUT) {
            return trtc_TIMEOUT;
        }
    }

    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

trtc_result_t trtc_cond_signal(trtc_cond_t cond)
{
    if (!cond) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_cond_signal((pthread_cond_t*)cond);
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

trtc_result_t trtc_cond_broadcast(trtc_cond_t cond)
{
    if (!cond) {
        return trtc_INVALID_PARAM;
    }

    int ret = pthread_cond_broadcast((pthread_cond_t*)cond);
    return (ret == 0) ? trtc_OK : trtc_ERROR;
}

/*==============================================================================
 * Socket API Implementation
 *============================================================================*/

static int trtc_family_to_native(trtc_socket_family_t family)
{
    return (family == trtc_AF_INET6) ? AF_INET6 : AF_INET;
}

static int trtc_type_to_native(trtc_socket_type_t type)
{
    return (type == trtc_SOCKET_TYPE_UDP) ? SOCK_DGRAM : SOCK_STREAM;
}

trtc_result_t trtc_socket_create(
    trtc_socket_type_t type,
    trtc_socket_family_t family,
    trtc_socket_t* socket_out)
{
    trtc_log("Creating socket, type=%d, family=%d", type, family);

    if (!socket_out) {
        trtc_log("Invalid parameters: socket_out is NULL");
        return trtc_INVALID_PARAM;
    }

    int native_type = trtc_type_to_native(type);
    int native_family = trtc_family_to_native(family);

    int fd = socket(native_family, native_type, 0);
    if (fd < 0) {
        trtc_log("Failed to create socket, error=%s", trtc_get_error_message());
        return trtc_ERROR;
    }

    /* Set non-blocking for better control */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)trtc_malloc(sizeof(trtc_socket_internal_t));
    if (!sock) {
        close(fd);
        return trtc_NO_MEMORY;
    }

    sock->fd = fd;
    sock->type = type;
    sock->family = family;

    *socket_out = (trtc_socket_t)sock;
    return trtc_OK;
}

trtc_result_t trtc_socket_close(trtc_socket_t socket)
{
    if (!socket) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;
    if (sock->fd >= 0) {
        close(sock->fd);
        sock->fd = -1;
    }
    trtc_free(sock);

    return trtc_OK;
}

trtc_result_t trtc_socket_bind(trtc_socket_t socket, const trtc_ip_address_t* address)
{
    if (!socket || !address) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    struct sockaddr_storage ss;
    memset(&ss, 0, sizeof(ss));

    if (sock->family == trtc_AF_INET6) {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&ss;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(address->port);
        if (inet_pton(AF_INET6, address->address, &addr->sin6_addr) <= 0) {
            return trtc_ERROR;
        }
    } else {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ss;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(address->port);
        if (inet_pton(AF_INET, address->address, &addr->sin_addr) <= 0) {
            return trtc_ERROR;
        }
    }

    if (bind(sock->fd, (struct sockaddr*)&ss, sizeof(ss)) < 0) {
        return trtc_ERROR;
    }

    return trtc_OK;
}

trtc_result_t trtc_socket_listen(trtc_socket_t socket, int backlog)
{
    if (!socket) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;
    if (listen(sock->fd, backlog) < 0) {
        return trtc_ERROR;
    }

    return trtc_OK;
}

trtc_result_t trtc_socket_accept(
    trtc_socket_t socket,
    trtc_socket_t* client_socket,
    trtc_ip_address_t* client_address)
{
    if (!socket || !client_socket) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    struct sockaddr_storage ss;
    socklen_t addrlen = sizeof(ss);
    memset(&ss, 0, sizeof(ss));

    int client_fd = accept(sock->fd, (struct sockaddr*)&ss, &addrlen);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return trtc_TIMEOUT;
        }
        return trtc_ERROR;
    }

    trtc_socket_internal_t* client = (trtc_socket_internal_t*)trtc_malloc(sizeof(trtc_socket_internal_t));
    if (!client) {
        close(client_fd);
        return trtc_NO_MEMORY;
    }

    client->fd = client_fd;
    client->type = sock->type;
    client->family = sock->family;

    if (client_address) {
        memset(client_address, 0, sizeof(trtc_ip_address_t));
        
        if (ss.ss_family == AF_INET6) {
            struct sockaddr_in6* addr = (struct sockaddr_in6*)&ss;
            client_address->family = trtc_AF_INET6;
            client_address->port = ntohs(addr->sin6_port);
            inet_ntop(AF_INET6, &addr->sin6_addr, client_address->address, sizeof(client_address->address));
        } else {
            struct sockaddr_in* addr = (struct sockaddr_in*)&ss;
            client_address->family = trtc_AF_INET;
            client_address->port = ntohs(addr->sin_port);
            inet_ntop(AF_INET, &addr->sin_addr, client_address->address, sizeof(client_address->address));
        }
    }

    *client_socket = (trtc_socket_t)client;
    return trtc_OK;
}

trtc_result_t trtc_socket_connect(
    trtc_socket_t socket,
    const trtc_ip_address_t* address,
    uint32_t timeout_ms)
{
    if (!socket || !address) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    struct sockaddr_storage ss;
    memset(&ss, 0, sizeof(ss));

    if (sock->family == trtc_AF_INET6) {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&ss;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(address->port);
        if (inet_pton(AF_INET6, address->address, &addr->sin6_addr) <= 0) {
            return trtc_ERROR;
        }
    } else {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ss;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(address->port);
        if (inet_pton(AF_INET, address->address, &addr->sin_addr) <= 0) {
            return trtc_ERROR;
        }
    }

    int ret = connect(sock->fd, (struct sockaddr*)&ss, sizeof(ss));
    if (ret < 0) {
        if (errno == EINPROGRESS) {
            /* Non-blocking connect in progress, wait for completion */
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock->fd, &write_fds);

            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            ret = select(sock->fd + 1, NULL, &write_fds, NULL, &tv);
            if (ret <= 0) {
                return trtc_TIMEOUT;
            }

            /* Check if connection succeeded */
            int error = 0;
            socklen_t error_len = sizeof(error);
            getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &error, &error_len);
            if (error != 0) {
                return trtc_ERROR;
            }
        } else {
            return trtc_ERROR;
        }
    }

    return trtc_OK;
}

trtc_result_t trtc_socket_send(
    trtc_socket_t socket,
    const void* data,
    size_t size,
    int flags,
    size_t* sent_size)
{
    if (!socket || !data) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    ssize_t n = send(sock->fd, data, size, flags);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (sent_size) *sent_size = 0;
            return trtc_TIMEOUT;
        }
        return trtc_ERROR;
    }

    if (sent_size) {
        *sent_size = (size_t)n;
    }
    return trtc_OK;
}

trtc_result_t trtc_socket_recv(
    trtc_socket_t socket,
    void* buffer,
    size_t size,
    int flags,
    size_t* received_size)
{
    if (!socket || !buffer) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    ssize_t n = recv(sock->fd, buffer, size, flags);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (received_size) *received_size = 0;
            return trtc_TIMEOUT;
        }
        return trtc_ERROR;
    }

    if (n == 0) {
        /* Connection closed */
        if (received_size) *received_size = 0;
        return trtc_ERROR;
    }

    if (received_size) {
        *received_size = (size_t)n;
    }
    return trtc_OK;
}

trtc_result_t trtc_socket_sendto(
    trtc_socket_t socket,
    const void* data,
    size_t size,
    const trtc_ip_address_t* address,
    size_t* sent_size)
{
    if (!socket || !data || !address) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    struct sockaddr_storage ss;
    memset(&ss, 0, sizeof(ss));

    if (sock->family == trtc_AF_INET6) {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&ss;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(address->port);
        inet_pton(AF_INET6, address->address, &addr->sin6_addr);
    } else {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ss;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(address->port);
        inet_pton(AF_INET, address->address, &addr->sin_addr);
    }

    ssize_t n = sendto(sock->fd, data, size, 0, (struct sockaddr*)&ss, sizeof(ss));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (sent_size) *sent_size = 0;
            return trtc_TIMEOUT;
        }
        return trtc_ERROR;
    }

    if (sent_size) {
        *sent_size = (size_t)n;
    }
    return trtc_OK;
}

trtc_result_t trtc_socket_recvfrom(
    trtc_socket_t socket,
    void* buffer,
    size_t size,
    trtc_ip_address_t* address,
    size_t* received_size)
{
    if (!socket || !buffer) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    struct sockaddr_storage ss;
    socklen_t addrlen = sizeof(ss);
    memset(&ss, 0, sizeof(ss));

    ssize_t n = recvfrom(sock->fd, buffer, size, 0, (struct sockaddr*)&ss, &addrlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (received_size) *received_size = 0;
            return trtc_TIMEOUT;
        }
        return trtc_ERROR;
    }

    if (address) {
        memset(address, 0, sizeof(trtc_ip_address_t));
        
        if (ss.ss_family == AF_INET6) {
            struct sockaddr_in6* addr = (struct sockaddr_in6*)&ss;
            address->family = trtc_AF_INET6;
            address->port = ntohs(addr->sin6_port);
            inet_ntop(AF_INET6, &addr->sin6_addr, address->address, sizeof(address->address));
        } else {
            struct sockaddr_in* addr = (struct sockaddr_in*)&ss;
            address->family = trtc_AF_INET;
            address->port = ntohs(addr->sin_port);
            inet_ntop(AF_INET, &addr->sin_addr, address->address, sizeof(address->address));
        }
    }

    if (received_size) {
        *received_size = (size_t)n;
    }
    return trtc_OK;
}

trtc_result_t trtc_socket_shutdown(trtc_socket_t socket, trtc_socket_shutdown_mode_t mode)
{
    if (!socket) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    int how;
    switch (mode) {
        case trtc_SOCKET_SHUTDOWN_READ:
            how = SHUT_RD;
            break;
        case trtc_SOCKET_SHUTDOWN_WRITE:
            how = SHUT_WR;
            break;
        case trtc_SOCKET_SHUTDOWN_BOTH:
        default:
            how = SHUT_RDWR;
            break;
    }

    if (shutdown(sock->fd, how) < 0) {
        return trtc_ERROR;
    }

    return trtc_OK;
}

trtc_result_t trtc_socket_setopt(
    trtc_socket_t socket,
    int level,
    int option,
    const void* value,
    size_t value_len)
{
    if (!socket || !value) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    if (setsockopt(sock->fd, level, option, value, value_len) < 0) {
        return trtc_ERROR;
    }

    return trtc_OK;
}

trtc_result_t trtc_socket_getopt(
    trtc_socket_t socket,
    int level,
    int option,
    void* value,
    size_t* value_len)
{
    if (!socket || !value || !value_len) {
        return trtc_INVALID_PARAM;
    }

    trtc_socket_internal_t* sock = (trtc_socket_internal_t*)socket;

    socklen_t len = (socklen_t)*value_len;
    if (getsockopt(sock->fd, level, option, value, &len) < 0) {
        return trtc_ERROR;
    }

    *value_len = (size_t)len;
    return trtc_OK;
}

trtc_result_t trtc_ip_from_string(
    const char* ip_string,
    uint16_t port,
    trtc_socket_family_t family,
    trtc_ip_address_t* address)
{
    if (!ip_string || !address) {
        return trtc_INVALID_PARAM;
    }

    memset(address, 0, sizeof(trtc_ip_address_t));

    /* Try to detect family from IP string */
    if (family == trtc_AF_INET6 || strchr(ip_string, ':') != NULL) {
        address->family = trtc_AF_INET6;
    } else {
        address->family = trtc_AF_INET;
    }

    address->port = port;
    strncpy(address->address, ip_string, sizeof(address->address) - 1);
    address->address[sizeof(address->address) - 1] = '\0';

    /* Validate the IP address */
    int af = (address->family == trtc_AF_INET6) ? AF_INET6 : AF_INET;
    if (inet_pton(af, address->address, NULL) != 1) {
        return trtc_ERROR;
    }

    return trtc_OK;
}

trtc_result_t trtc_ip_get_local(trtc_ip_address_t* address)
{
    if (!address) {
        return trtc_INVALID_PARAM;
    }

    memset(address, 0, sizeof(trtc_ip_address_t));

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return trtc_ERROR;
    }

    /* Connect to 8.8.8.8:80 to determine local IP */
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &dest.sin_addr);

    if (connect(fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        close(fd);
        return trtc_ERROR;
    }

    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(fd, (struct sockaddr*)&local, &len) < 0) {
        close(fd);
        return trtc_ERROR;
    }

    close(fd);

    address->family = trtc_AF_INET;
    address->port = 0;
    inet_ntop(AF_INET, &local.sin_addr, address->address, sizeof(address->address));

    return trtc_OK;
}

/*==============================================================================
 * Memory API Implementation
 *============================================================================*/

void* trtc_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    return malloc(size);
}

void* trtc_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    return calloc(nmemb, size);
}

void* trtc_realloc(void* ptr, size_t size)
{
    if (size == 0) {
        if (ptr) {
            free(ptr);
        }
        return NULL;
    }
    return realloc(ptr, size);
}

void trtc_free(void* ptr)
{
    if (ptr) {
        free(ptr);
    }
}

char* trtc_strdup(const char* str)
{
    if (!str) {
        return NULL;
    }
    return strdup(str);
}

/*==============================================================================
 * Utility API Implementation
 *============================================================================*/

const char* trtc_get_error_message(void)
{
    return strerror(errno);
}

trtc_result_t trtc_init(void)
{
    pthread_mutex_lock(&g_trtc_mutex);
    
    if (!g_trtc_initialized) {
        g_trtc_initialized = true;
    }
    
    pthread_mutex_unlock(&g_trtc_mutex);
    
    return trtc_OK;
}

void trtc_cleanup(void)
{
    pthread_mutex_lock(&g_trtc_mutex);
    
    if (g_trtc_initialized) {
        g_trtc_initialized = false;
    }
    
    pthread_mutex_unlock(&g_trtc_mutex);
}