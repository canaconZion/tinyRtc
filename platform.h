/**
 * @file platform.h
 * @brief TRTC-style platform abstraction layer for tinyRtc
 * @author tinyRtc team
 * @date 2026-04-26
 */

#ifndef __trtc_H__
#define __trtc_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * Logging API
 *============================================================================*/

/**
 * @brief Enable or disable logging (set to 0 to disable)
 */
#define trtc_LOG_ENABLED 1

/**
 * @brief Simple logging macro that prefixes function name
 * Usage: trtc_log("message: %d", value);
 */
#if trtc_LOG_ENABLED
#define trtc_log(fmt, ...) \
    printf("[%s] " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define trtc_log(fmt, ...) ((void)0)
#endif

/*==============================================================================
 * Platform Basic Types
 *============================================================================*/

/* Handle types for platform resources */
typedef void* trtc_thread_t;
typedef void* trtc_mutex_t;
typedef void* trtc_cond_t;
typedef void* trtc_socket_t;
typedef void* trtc_memory_t;

/* Thread function type */
typedef void* (*trtc_thread_func_t)(void* args);

/* Platform result codes */
typedef enum {
    trtc_OK = 0,
    trtc_ERROR = -1,
    trtc_TIMEOUT = -2,
    trtc_INVALID_PARAM = -3,
    trtc_NO_MEMORY = -4,
    trtc_NOT_SUPPORTED = -5,
} trtc_result_t;

/*==============================================================================
 * Thread API
 *============================================================================*/

/**
 * @brief Create a new thread
 * @param func Thread function entry point
 * @param args Arguments passed to the thread function
 * @param stack_size Stack size in bytes (0 for default)
 * @param name Thread name for debugging
 * @param thread Pointer to store the created thread handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_thread_create(
    trtc_thread_func_t func,
    void* args,
    size_t stack_size,
    const char* name,
    trtc_thread_t* thread);

/**
 * @brief Wait for thread to finish (join)
 * @param thread Thread handle
 * @param retval Pointer to store thread return value (can be NULL)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_thread_join(trtc_thread_t thread, void** retval);

/**
 * @brief Detach thread from caller
 * @param thread Thread handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_thread_detach(trtc_thread_t thread);

/**
 * @brief Get current thread ID
 * @return Current thread handle
 */
trtc_thread_t trtc_thread_get_current(void);

/**
 * @brief Thread sleep function
 * @param ms Sleep duration in milliseconds
 */
void trtc_thread_sleep(uint32_t ms);

/**
 * @brief Thread yield function (give up current time slice)
 */
void trtc_thread_yield(void);

/*==============================================================================
 * Mutex API
 *============================================================================*/

/**
 * @brief Create a mutex
 * @param mutex Pointer to store the created mutex handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_mutex_create(trtc_mutex_t* mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Mutex handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_mutex_destroy(trtc_mutex_t mutex);

/**
 * @brief Lock a mutex (blocking)
 * @param mutex Mutex handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_mutex_lock(trtc_mutex_t mutex);

/**
 * @brief Try to lock a mutex (non-blocking)
 * @param mutex Mutex handle
 * @return trtc_OK if locked, trtc_ERROR if already locked
 */
trtc_result_t trtc_mutex_trylock(trtc_mutex_t mutex);

/**
 * @brief Unlock a mutex
 * @param mutex Mutex handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_mutex_unlock(trtc_mutex_t mutex);

/*==============================================================================
 * Condition Variable API
 *============================================================================*/

/**
 * @brief Create a condition variable
 * @param cond Pointer to store the created condition variable handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_cond_create(trtc_cond_t* cond);

/**
 * @brief Destroy a condition variable
 * @param cond Condition variable handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_cond_destroy(trtc_cond_t cond);

/**
 * @brief Wait on a condition variable
 * @param cond Condition variable handle
 * @param mutex Associated mutex (must be locked by caller)
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return trtc_OK on success, trtc_TIMEOUT on timeout, error code on failure
 */
trtc_result_t trtc_cond_wait(trtc_cond_t cond, trtc_mutex_t mutex, uint32_t timeout_ms);

/**
 * @brief Signal a condition variable (wake one waiter)
 * @param cond Condition variable handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_cond_signal(trtc_cond_t cond);

/**
 * @brief Broadcast a condition variable (wake all waiters)
 * @param cond Condition variable handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_cond_broadcast(trtc_cond_t cond);

/*==============================================================================
 * Socket API
 *============================================================================*/

/* Socket types */
typedef enum {
    trtc_SOCKET_TYPE_TCP = 0,
    trtc_SOCKET_TYPE_UDP = 1,
} trtc_socket_type_t;

/* Socket protocol family */
typedef enum {
    trtc_AF_INET = 0,    /* IPv4 */
    trtc_AF_INET6 = 1,   /* IPv6 */
} trtc_socket_family_t;

/* Socket shutdown mode */
typedef enum {
    trtc_SOCKET_SHUTDOWN_READ = 0,
    trtc_SOCKET_SHUTDOWN_WRITE = 1,
    trtc_SOCKET_SHUTDOWN_BOTH = 2,
} trtc_socket_shutdown_mode_t;

/* IP address structure */
typedef struct {
    char address[64];           /* IP address string */
    uint16_t port;             /* Port number */
    trtc_socket_family_t family;  /* Address family */
} trtc_ip_address_t;

/**
 * @brief Create a new socket
 * @param type Socket type (TCP/UDP)
 * @param family Address family (IPv4/IPv6)
 * @param socket Pointer to store the created socket handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_create(
    trtc_socket_type_t type,
    trtc_socket_family_t family,
    trtc_socket_t* socket);

/**
 * @brief Close a socket
 * @param socket Socket handle
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_close(trtc_socket_t socket);

/**
 * @brief Bind socket to an address
 * @param socket Socket handle
 * @param address Address to bind to
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_bind(trtc_socket_t socket, const trtc_ip_address_t* address);

/**
 * @brief Listen for incoming connections (TCP only)
 * @param socket Socket handle
 * @param backlog Maximum connection queue length
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_listen(trtc_socket_t socket, int backlog);

/**
 * @brief Accept an incoming connection
 * @param socket Listening socket handle
 * @param client_socket Pointer to store the accepted client socket
 * @param client_address Pointer to store client address (can be NULL)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_accept(
    trtc_socket_t socket,
    trtc_socket_t* client_socket,
    trtc_ip_address_t* client_address);

/**
 * @brief Connect to a remote address
 * @param socket Socket handle
 * @param address Remote address to connect to
 * @param timeout_ms Connection timeout in milliseconds (0 for infinite)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_connect(
    trtc_socket_t socket,
    const trtc_ip_address_t* address,
    uint32_t timeout_ms);

/**
 * @brief Send data on a socket
 * @param socket Socket handle
 * @param data Data buffer to send
 * @param size Size of data to send
 * @param flags Send flags
 * @param sent_size Pointer to store actual bytes sent (can be NULL)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_send(
    trtc_socket_t socket,
    const void* data,
    size_t size,
    int flags,
    size_t* sent_size);

/**
 * @brief Receive data from a socket
 * @param socket Socket handle
 * @param buffer Buffer to store received data
 * @param size Buffer size
 * @param flags Receive flags
 * @param received_size Pointer to store actual bytes received (can be NULL)
 * @return trtc_OK on success, trtc_TIMEOUT on timeout, error code on failure
 */
trtc_result_t trtc_socket_recv(
    trtc_socket_t socket,
    void* buffer,
    size_t size,
    int flags,
    size_t* received_size);

/**
 * @brief Send data to a specific address (UDP only)
 * @param socket Socket handle (must be UDP)
 * @param data Data buffer to send
 * @param size Size of data to send
 * @param address Destination address
 * @param sent_size Pointer to store actual bytes sent (can be NULL)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_sendto(
    trtc_socket_t socket,
    const void* data,
    size_t size,
    const trtc_ip_address_t* address,
    size_t* sent_size);

/**
 * @brief Receive data from a specific address (UDP only)
 * @param socket Socket handle (must be UDP)
 * @param buffer Buffer to store received data
 * @param size Buffer size
 * @param address Pointer to store sender address (can be NULL)
 * @param received_size Pointer to store actual bytes received (can be NULL)
 * @return trtc_OK on success, trtc_TIMEOUT on timeout, error code on failure
 */
trtc_result_t trtc_socket_recvfrom(
    trtc_socket_t socket,
    void* buffer,
    size_t size,
    trtc_ip_address_t* address,
    size_t* received_size);

/**
 * @brief Shutdown socket
 * @param socket Socket handle
 * @param mode Shutdown mode
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_shutdown(trtc_socket_t socket, trtc_socket_shutdown_mode_t mode);

/**
 * @brief Set socket option
 * @param socket Socket handle
 * @param level Option level (SOL_SOCKET, IPPROTO_TCP, etc.)
 * @param option Option name
 * @param value Option value
 * @param value_len Value size
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_setopt(
    trtc_socket_t socket,
    int level,
    int option,
    const void* value,
    size_t value_len);

/**
 * @brief Get socket option
 * @param socket Socket handle
 * @param level Option level
 * @param option Option name
 * @param value Buffer to store option value
 * @param value_len Buffer size (input/output)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_socket_getopt(
    trtc_socket_t socket,
    int level,
    int option,
    void* value,
    size_t* value_len);

/**
 * @brief Convert string to IP address structure
 * @param ip_string IP address string (e.g., "192.168.1.1" or "::1")
 * @param port Port number
 * @param family Address family (auto-detect if trtc_AF_INET)
 * @param address Pointer to store parsed address
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_ip_from_string(
    const char* ip_string,
    uint16_t port,
    trtc_socket_family_t family,
    trtc_ip_address_t* address);

/**
 * @brief Get local IP address
 * @param address Pointer to store local IP address
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_ip_get_local(trtc_ip_address_t* address);

/*==============================================================================
 * Memory API
 *============================================================================*/

/**
 * @brief Allocate memory
 * @param size Size of memory to allocate in bytes
 * @return Pointer to allocated memory, NULL on failure
 */
void* trtc_malloc(size_t size);

/**
 * @brief Allocate zero-initialized memory
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, NULL on failure
 */
void* trtc_calloc(size_t nmemb, size_t size);

/**
 * @brief Reallocate memory
 * @param ptr Pointer to existing memory
 * @param size New size
 * @return Pointer to reallocated memory, NULL on failure
 */
void* trtc_realloc(void* ptr, size_t size);

/**
 * @brief Free memory
 * @param ptr Pointer to memory to free
 */
void trtc_free(void* ptr);

/**
 * @brief Duplicate a string
 * @param str String to duplicate
 * @return Duplicated string, NULL on failure
 */
char* trtc_strdup(const char* str);

/*==============================================================================
 * Utility API
 *============================================================================*/

/**
 * @brief Get platform error message
 * @return Error message string
 */
const char* trtc_get_error_message(void);

/**
 * @brief Initialize platform subsystem (call at program start)
 * @return trtc_OK on success, error code on failure
 */
trtc_result_t trtc_init(void);

/**
 * @brief Cleanup platform subsystem (call at program end)
 */
void trtc_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __trtc_H__ */