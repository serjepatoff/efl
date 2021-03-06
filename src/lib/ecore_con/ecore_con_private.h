#ifndef _ECORE_CON_PRIVATE_H
#define _ECORE_CON_PRIVATE_H

#include "ecore_private.h"
#include "Ecore_Con.h"

#define ECORE_MAGIC_CON_SERVER             0x77665544
#define ECORE_MAGIC_CON_CLIENT             0x77556677
#define ECORE_MAGIC_CON_URL                0x77074255

#define ECORE_CON_TYPE 0x0f
#define ECORE_CON_SSL  0xf0
#define ECORE_CON_SUPER_SSL  0xf00

#if HAVE_GNUTLS
# include <gnutls/gnutls.h>
#elif HAVE_OPENSSL
# include <openssl/ssl.h>
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#define READBUFSIZ 65536

extern int _ecore_con_log_dom;

#ifdef ECORE_CON_DEFAULT_LOG_COLOR
#undef ECORE_LOG_DEFAULT_LOG_COLOR
#endif
#define ECORE_CON_DEFAULT_LOG_COLOR EINA_COLOR_BLUE

#ifdef ERR
# undef ERR
#endif
#define ERR(...) EINA_LOG_DOM_ERR(_ecore_con_log_dom, __VA_ARGS__)

#ifdef DBG
# undef DBG
#endif
#define DBG(...) EINA_LOG_DOM_DBG(_ecore_con_log_dom, __VA_ARGS__)

#ifdef INF
# undef INF
#endif
#define INF(...) EINA_LOG_DOM_INFO(_ecore_con_log_dom, __VA_ARGS__)

#ifdef WRN
# undef WRN
#endif
#define WRN(...) EINA_LOG_DOM_WARN(_ecore_con_log_dom, __VA_ARGS__)

#ifdef CRI
# undef CRI
#endif
#define CRI(...) EINA_LOG_DOM_CRIT(_ecore_con_log_dom, __VA_ARGS__)

typedef struct _Ecore_Con_Lookup Ecore_Con_Lookup;
typedef struct _Ecore_Con_Info Ecore_Con_Info;
typedef struct Ecore_Con_Socks Ecore_Con_Socks_v4;
typedef struct Ecore_Con_Socks_v5 Ecore_Con_Socks_v5;
typedef void (*Ecore_Con_Info_Cb)(void *data, Ecore_Con_Info *infos);

typedef enum _Ecore_Con_State
{
   ECORE_CON_CONNECTED,
   ECORE_CON_DISCONNECTED,
   ECORE_CON_INPROGRESS
} Ecore_Con_State;

typedef enum _Ecore_Con_Ssl_Error
{
   ECORE_CON_SSL_ERROR_NONE = 0,
   ECORE_CON_SSL_ERROR_NOT_SUPPORTED,
   ECORE_CON_SSL_ERROR_INIT_FAILED,
   ECORE_CON_SSL_ERROR_SERVER_INIT_FAILED,
   ECORE_CON_SSL_ERROR_SSL2_NOT_SUPPORTED,
   ECORE_CON_SSL_ERROR_SSL3_NOT_SUPPORTED
} Ecore_Con_Ssl_Error;

typedef enum _Ecore_Con_Ssl_Handshake
{
   ECORE_CON_SSL_STATE_DONE = 0,
   ECORE_CON_SSL_STATE_HANDSHAKING,
   ECORE_CON_SSL_STATE_INIT
} Ecore_Con_Ssl_State;

typedef enum Ecore_Con_Proxy_State
{  /* named PROXY instead of SOCKS in case some handsome and enterprising
    * developer decides to add HTTP CONNECT support
    */
   ECORE_CON_PROXY_STATE_DONE = 0,
   ECORE_CON_PROXY_STATE_RESOLVED,
   ECORE_CON_PROXY_STATE_INIT,
   ECORE_CON_PROXY_STATE_READ,
   ECORE_CON_PROXY_STATE_AUTH,
   ECORE_CON_PROXY_STATE_REQUEST,
   ECORE_CON_PROXY_STATE_CONFIRM,
} Ecore_Con_Proxy_State;

struct _Efl_Network_Client_Data
{
#ifdef _WIN32
   SOCKET fd;
#else
   int fd;
#endif
   Ecore_Con_Server *host_server;
   void *data;
   Ecore_Fd_Handler *fd_handler;
   size_t buf_offset;
   Eina_Binbuf *buf;
   const char *ip;
   Eina_List *event_count;
   struct sockaddr *client_addr;
   int client_addr_len;
   double start_time;
   Ecore_Timer *until_deletion;
   double disconnect_time;
#if HAVE_GNUTLS
   gnutls_datum_t session_ticket;
   gnutls_session_t session;
#elif HAVE_OPENSSL
   SSL *ssl;
   int ssl_err;
#endif
   Ecore_Con_Ssl_State ssl_state;
   Eina_Bool handshaking : 1;
   Eina_Bool upgrade : 1; /* STARTTLS queued */
   Eina_Bool delete_me : 1; /* del event has been queued */
};

typedef struct _Efl_Network_Client_Data Efl_Network_Client_Data;

struct _Efl_Network_Server_Data
{
#ifdef _WIN32
   SOCKET fd;
#else
   int fd;
#endif
   Ecore_Con_Type type;
   char *name;
   int port;
   char *path;
   void *data;
   Ecore_Fd_Handler *fd_handler;
   Eina_List *clients;
   unsigned int client_count;
   Eina_Binbuf *buf;
   size_t write_buf_offset;
   Eina_List *infos;
   Eina_List *event_count;
   int client_limit;
   pid_t ppid;
   /* socks */
   Ecore_Con_Socks *ecs;
   Ecore_Con_Proxy_State ecs_state;
   int ecs_addrlen;
   unsigned char ecs_addr[16];
   size_t ecs_buf_offset;
   Eina_Binbuf *ecs_buf;
   Eina_Binbuf *ecs_recvbuf;
   const char *proxyip;
   int proxyport;
   /* endsocks */
   const char *verify_name;
#if HAVE_GNUTLS
   gnutls_session_t session;
   gnutls_anon_client_credentials_t anoncred_c;
   gnutls_anon_server_credentials_t anoncred_s;
   gnutls_psk_client_credentials_t pskcred_c;
   gnutls_psk_server_credentials_t pskcred_s;
   gnutls_certificate_credentials_t cert;
   char *cert_file;
   gnutls_dh_params_t dh_params;
#elif HAVE_OPENSSL
   SSL_CTX *ssl_ctx;
   SSL *ssl;
   int ssl_err;
#endif
   double start_time;
   Ecore_Timer *until_deletion;
   double disconnect_time;
   double client_disconnect_time;
   const char *ip;
   Eina_Bool created : 1; /* @c EINA_TRUE if server is our listening server */
   Eina_Bool connecting : 1; /* @c EINA_FALSE if just initialized or connected */
   Eina_Bool handshaking : 1; /* @c EINA_TRUE if server is ssl handshaking */
   Eina_Bool upgrade : 1;  /* STARTTLS queued */
   Eina_Bool disable_proxy : 1; /* proxy should never be used with this connection */
   Eina_Bool ssl_prepared : 1;
   Eina_Bool use_cert : 1; /* @c EINA_TRUE if using certificate auth */
   Ecore_Con_Ssl_State ssl_state; /* current state of ssl handshake on the server */
   Eina_Bool verify : 1; /* @c EINA_TRUE if certificates will be verified */
   Eina_Bool verify_basic : 1; /* @c EINA_TRUE if certificates will be verified only against the hostname */
   Eina_Bool reject_excess_clients : 1;
   Eina_Bool delete_me : 1; /* del event has been queued */
#ifdef _WIN32
   Eina_Bool want_write : 1;
   Eina_Bool read_stop : 1;
   Eina_Bool read_stopped : 1;
   HANDLE pipe;
   HANDLE thread_read;
   HANDLE event_read;
   HANDLE event_peek;
   DWORD nbr_bytes;
#endif
};

typedef struct _Efl_Network_Server_Data Efl_Network_Server_Data;

struct _Ecore_Con_Info
{
   unsigned int size;
   struct addrinfo info;
   char ip[NI_MAXHOST];
   char service[NI_MAXSERV];
};

struct _Ecore_Con_Lookup
{
   Ecore_Con_Dns_Cb done_cb;
   const void *data;
};

struct Ecore_Con_Socks /* v4 */
{
   unsigned char version;

   const char *ip;
   int port;
   const char *username;
   unsigned int ulen;
   Eina_Bool lookup : 1;
   Eina_Bool bind : 1;
};

struct Ecore_Con_Socks_v5
{
   unsigned char version;

   const char *ip;
   int port;
   const char *username;
   unsigned int ulen;
   Eina_Bool lookup : 1;
   Eina_Bool bind : 1;
   /* v5 only */
   unsigned char method;
   const char *password;
   unsigned int plen;
};

#ifdef HAVE_SYSTEMD
extern int sd_fd_index;
extern int sd_fd_max;
#endif

/* init must be called from main thread */
Eina_Bool ecore_con_libproxy_init(void);
void ecore_con_libproxy_proxies_free(char **proxies);
/* BLOCKING! should be called from a worker thread */
char **ecore_con_libproxy_proxies_get(const char *url);


extern Ecore_Con_Socks *_ecore_con_proxy_once;
extern Ecore_Con_Socks *_ecore_con_proxy_global;
void ecore_con_socks_init(void);
void ecore_con_socks_shutdown(void);
Eina_Bool ecore_con_socks_svr_init(Ecore_Con_Server *svr);
void ecore_con_socks_read(Ecore_Con_Server *svr, unsigned char *buf, int num);
void ecore_con_socks_dns_cb(const char *canonname, const char *ip, struct sockaddr *addr, int addrlen, Ecore_Con_Server *svr);

/* from ecore_con.c */
void ecore_con_server_infos_del(Ecore_Con_Server *svr, void *info);
void ecore_con_event_proxy_bind(Ecore_Con_Server *svr);
void ecore_con_event_server_data(Ecore_Con_Server *svr, unsigned char *buf, int num, Eina_Bool duplicate);
void ecore_con_event_server_del(Ecore_Con_Server *svr);
#define ecore_con_event_server_error(svr, error) _ecore_con_event_server_error((svr), (char*)(error), EINA_TRUE)
void _ecore_con_event_server_error(Ecore_Con_Server *svr, char *error, Eina_Bool duplicate);
void ecore_con_event_client_add(Ecore_Con_Client *cl);
void ecore_con_event_client_data(Ecore_Con_Client *cl, unsigned char *buf, int num, Eina_Bool duplicate);
void ecore_con_event_client_del(Ecore_Con_Client *cl);
void ecore_con_event_client_error(Ecore_Con_Client *cl, const char *error);
void _ecore_con_server_kill(Ecore_Con_Server *svr);
void _ecore_con_client_kill(Ecore_Con_Client *cl);

int ecore_con_local_init(void);
int ecore_con_local_shutdown(void);
/* from ecore_local_win32.c */
#ifdef _WIN32
Eina_Bool ecore_con_local_listen(Ecore_Con_Server *svr);
Eina_Bool ecore_con_local_connect(Ecore_Con_Server *svr,
                            Eina_Bool (*cb_done)(void *data,
                                                 Ecore_Fd_Handler *fd_handler));
Eina_Bool ecore_con_local_win32_server_flush(Ecore_Con_Server *svr);
Eina_Bool ecore_con_local_win32_client_flush(Ecore_Con_Client *cl);
void      ecore_con_local_win32_server_del(Ecore_Con_Server *svr);
void      ecore_con_local_win32_client_del(Ecore_Con_Client *cl);
#else
/* from ecore_local.c */
int ecore_con_local_connect(Ecore_Con_Server *svr,
                            Eina_Bool (*cb_done)(
                               void *data,
                               Ecore_Fd_Handler *fd_handler),
                            void *data);
int ecore_con_local_listen(Ecore_Con_Server *svr,
                           Eina_Bool (*cb_listen)(
                              void *data,
                              Ecore_Fd_Handler *fd_handler),
                           void *data);
#endif

/* from ecore_con_info.c */
int                 ecore_con_info_init(void);
int                 ecore_con_info_shutdown(void);
int                 ecore_con_info_tcp_connect(Ecore_Con_Server *svr,
                                               Ecore_Con_Info_Cb done_cb,
                                               void *data);
int                 ecore_con_info_tcp_listen(Ecore_Con_Server *svr,
                                              Ecore_Con_Info_Cb done_cb,
                                              void *data);
int                 ecore_con_info_udp_connect(Ecore_Con_Server *svr,
                                               Ecore_Con_Info_Cb done_cb,
                                               void *data);
int                 ecore_con_info_udp_listen(Ecore_Con_Server *svr,
                                              Ecore_Con_Info_Cb done_cb,
                                              void *data);
int                 ecore_con_info_mcast_listen(Ecore_Con_Server *svr,
                                                Ecore_Con_Info_Cb done_cb,
                                                void *data);
void                ecore_con_info_data_clear(void *info);

void ecore_con_event_server_add(Ecore_Con_Server *svr);


/* from ecore_con_ssl.c */
Ecore_Con_Ssl_Error ecore_con_ssl_init(void);
Ecore_Con_Ssl_Error ecore_con_ssl_shutdown(void);
Ecore_Con_Ssl_Error ecore_con_ssl_server_prepare(Ecore_Con_Server *svr, int ssl_type);
Ecore_Con_Ssl_Error ecore_con_ssl_server_init(Ecore_Con_Server *svr);
Ecore_Con_Ssl_Error ecore_con_ssl_server_shutdown(Ecore_Con_Server *svr);
int                 ecore_con_ssl_server_read(Ecore_Con_Server *svr,
                                              unsigned char *buf,
                                              int size);
int                 ecore_con_ssl_server_write(Ecore_Con_Server *svr,
                                               const unsigned char *buf,
                                               int size);
Ecore_Con_Ssl_Error ecore_con_ssl_client_init(Ecore_Con_Client *svr);
Ecore_Con_Ssl_Error ecore_con_ssl_client_shutdown(Ecore_Con_Client *svr);
int                 ecore_con_ssl_client_read(Ecore_Con_Client *svr,
                                              unsigned char *buf,
                                              int size);
int                 ecore_con_ssl_client_write(Ecore_Con_Client *svr,
                                               const unsigned char *buf,
                                               int size);

int                 ecore_con_info_get(Ecore_Con_Server *svr,
                                       Ecore_Con_Info_Cb done_cb,
                                       void *data,
                                       struct addrinfo *hints);


#define GENERIC_ALLOC_FREE_HEADER(TYPE, Type) \
  TYPE *Type##_alloc(void);		      \
  void Type##_free(TYPE *e);

GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Client_Add, ecore_con_event_client_add);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Client_Del, ecore_con_event_client_del);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Client_Write, ecore_con_event_client_write);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Client_Data, ecore_con_event_client_data);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Server_Error, ecore_con_event_server_error);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Client_Error, ecore_con_event_client_error);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Server_Add, ecore_con_event_server_add);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Server_Del, ecore_con_event_server_del);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Server_Write, ecore_con_event_server_write);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Server_Data, ecore_con_event_server_data);
GENERIC_ALLOC_FREE_HEADER(Ecore_Con_Event_Proxy_Bind, ecore_con_event_proxy_bind);

void ecore_con_mempool_init(void);
void ecore_con_mempool_shutdown(void);

#undef GENERIC_ALLOC_FREE_HEADER

/* allow windows and posix to use the same error comparison */
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef _WIN32
#define closesocket(fd) close(fd)
#define SOCKET int
#endif

/* some platforms do not have AI_V4MAPPED, then define to 0 so bitwise OR won't be changed */
#ifndef AI_V4MAPPED
#define AI_V4MAPPED 0
#endif

void _efl_net_server_udp_client_init(Eo *client, SOCKET fd, const struct sockaddr *addr, socklen_t addrlen, const char *str);
void _efl_net_server_udp_client_feed(Eo *client, Eina_Rw_Slice slice);

void _efl_net_socket_udp_init(Eo *o, const struct sockaddr *addr, socklen_t addrlen, const char *str);

#ifndef _WIN32
Eina_Bool efl_net_unix_fmt(char *buf, size_t buflen, SOCKET fd, const struct sockaddr_un *addr, socklen_t addrlen);
#endif
Eina_Bool efl_net_ip_port_fmt(char *buf, size_t buflen, const struct sockaddr *addr);

#ifdef HAVE_SYSTEMD
/**
 * Checks if the next FD in the sd_fd_index:sd_fd_max is of the
 * expected family, protocol and if it's listening.
 *
 * This is similar to sd_is_socket()/sd_is_socket_inet(), but will
 * also parse address in our standard format "IP:PORT", including IPv6
 * within braces, and then will validate the address with
 * getsockaddr() for INET.
 *
 * @param address the address to validate
 * @param family AF_UNIX or AF_UNSPEC for INET, in that case AF_INET
 *        or AF_INET6 will be inferred from @a address.
 * @param type SOCK_STREAM or SOCK_DGRAM
 * @param[out] listening where to return listening state, should be
 *       NULL for @a type SOCK_DGRAM
 *
 * @return 0 on success, error otherwise.
 *
 * @internal
 */
Eina_Error efl_net_ip_socket_activate_check(const char *address, int family, int type, Eina_Bool *listening);
#endif

/**
 * @brief splits an address in the format "host:port" in two
 * null-terminated strings.
 *
 * The address may be 'server.com:1234', 'server.com:http',
 * 'server.com' (@c *p_port will be NULL), IPv4 127.0.0.1:456 or
 * IPv6 [::1]:456
 *
 * @param[inout] buf contains the string to be split and will be modified.
 * @param[out] p_host returns a pointer inside @a buf with
 *             null-terminated host part.
 * @param[out] p_port returns a pointer with null-terminated port
 *             part. The pointer may be inside @a buf if port was
 *             specified or #NULL if it wasn't specified.
 *
 * @return #EINA_TRUE on success, #EINA_FALSE on errors.
 *
 * @internal
 */
Eina_Bool efl_net_ip_port_split(char *buf, const char **p_host, const char **p_port);

SOCKET efl_net_socket4(int domain, int type, int protocol, Eina_Bool close_on_exec);

/**
 * @brief callback to notify of resolved address.
 *
 * The callback is given the ownership of the result, thus must free
 * it with freeaddrinfo().
 *
 * @internal
 */
typedef void (*Efl_Net_Ip_Resolve_Async_Cb)(void *data, const char *host, const char *port, const struct addrinfo *hints, struct addrinfo *result, int gai_error);

/**
 * @brief asynchronously resolve a host and port using getaddrinfo().
 *
 * This will call getaddrinfo() in a thread, taking care to return the
 * result to the main loop and calling @a cb with given user @a data.
 *
 * @internal
 */
Ecore_Thread *efl_net_ip_resolve_async_new(const char *host, const char *port, const struct addrinfo *hints, Efl_Net_Ip_Resolve_Async_Cb cb, const void *data);

/**
 * @brief callback to notify of connection.
 *
 * The callback is given the ownership of the socket (sockfd), thus
 * must close().
 *
 * @internal
 */
typedef void (*Efl_Net_Connect_Async_Cb)(void *data, const struct sockaddr *addr, socklen_t addrlen, int sockfd, Eina_Error error);

/**
 * @brief asynchronously create a socket and connect to the address.
 *
 * This will call socket() and connect() in a thread, taking care to
 * return the result to the main loop and calling @a cb with given
 * user @a data.
 *
 * For name resolution and proxy support use
 * efl_net_ip_connect_async_new()
 *
 * @internal
 */
Ecore_Thread *efl_net_connect_async_new(const struct sockaddr *addr, socklen_t addrlen, int type, int protocol, Eina_Bool close_on_exec, Efl_Net_Connect_Async_Cb cb, const void *data);

/**
 * @brief asynchronously create a socket and connect to the IP address.
 *
 * This wil resolve the address using getaddrinfo(), create a socket
 * and connect in a thread.
 *
 * If a @a proxy is given, then it's always used. Otherwise the
 * environment variable @a proxy_env is used unless it matches @a
 * no_proxy_env. Some systems may do special queries for proxy from
 * the thread.
 *
 * @param address the host:port to connect. Host may be a name or an
 *        IP address, IPv6 addresses should be enclosed in braces.
 * @param proxy a mandatory proxy to use. If "" (empty string), it's
 *        disabled. If NULL, then @a proxy_env is used unless it
 *        matches @a no_proxy_env.
 * @param proxy_env if @a proxy is NULL, then this will be used as the
 *        proxy unless it matches @a no_proxy_env.
 * @param no_proxy_env a comma-separated list of matches that will
 *        avoid using @a proxy_env. "server.com" will inhibit proxy
 *        for "server.com", "host.server.com" but not "xserver.com".
 * @param type the socket type, such as SOCK_STREAM or SOCK_DGRAM.
 * @param protocol the socket protocol, such as IPPROTO_TCP.
 * @param close_on_exec if EINA_TRUE, will set SOCK_CLOEXEC.
 * @param cb the callback to report connection
 * @param data data to give to callback
 *
 * @return an Ecore_Thread that will execute the connection.
 *
 * @internal
 */
Ecore_Thread *efl_net_ip_connect_async_new(const char *address, const char *proxy, const char *proxy_env, const char *no_proxy_env, int type, int protocol, Eina_Bool close_on_exec, Efl_Net_Connect_Async_Cb cb, const void *data);

static inline Eina_Error
efl_net_socket_error_get(void)
{
#ifndef _WIN32
   return errno;
#else
   Eina_Error err = WSAGetLastError();

   if (0) { }

   /* used by send() */
#if defined(WSAEACCES) && (WSAEACCES != EACCES)
   else if (err == WSAEACCES) err = EACCES;
#endif
#if defined(WSAEWOULDBLOCK) && (WSAEWOULDBLOCK != EAGAIN)
   else if (err == WSAEWOULDBLOCK) err = EAGAIN;
#endif
#if defined(WSAEBADF) && (WSAEBADF != EBADF)
   else if (err == WSAEBADF) err = EBADF;
#endif
#if defined(WSAECONNRESET) && (WSAECONNRESET != ECONNRESET)
   else if (err == WSAECONNRESET) err = ECONNRESET;
#endif
#if defined(WSAEDESTADDRREQ) && (WSAEDESTADDRREQ != EDESTADDRREQ)
   else if (err == WSAEDESTADDRREQ) err = EDESTADDRREQ;
#endif
#if defined(WSAEFAULT) && (WSAEFAULT != EFAULT)
   else if (err == WSAEFAULT) err = EFAULT;
#endif
#if defined(WSAEINTR) && (WSAEINTR != EINTR)
   else if (err == WSAEINTR) err = EINTR;
#endif
#if defined(WSAEINVAL) && (WSAEINVAL != EINVAL)
   else if (err == WSAEINVAL) err = EINVAL;
#endif
#if defined(WSAEISCONN) && (WSAEISCONN != EISCONN)
   else if (err == WSAEISCONN) err = EISCONN;
#endif
#if defined(WSAEMSGSIZE) && (WSAEMSGSIZE != EMSGSIZE)
   else if (err == WSAEMSGSIZE) err = EMSGSIZE;
#endif
#if defined(WSAENOBUFS) && (WSAENOBUFS != ENOBUFS)
   else if (err == WSAENOBUFS) err = ENOBUFS;
#endif
#if defined(WSA_NOT_ENOUGH_MEMORY) && (WSA_NOT_ENOUGH_MEMORY != ENOMEM)
   else if (err == WSA_NOT_ENOUGH_MEMORY) err = ENOMEM;
#endif
#if defined(WSAENOTCONN) && (WSAENOTCONN != ENOTCONN)
   else if (err == WSAENOTCONN) err = ENOTCONN;
#endif
#if defined(WSAENOTSOCK) && (WSAENOTSOCK != ENOTSOCK)
   else if (err == WSAENOTSOCK) err = ENOTSOCK;
#endif
#if defined(WSAEOPNOTSUPP) && (WSAEOPNOTSUPP != EOPNOTSUPP)
   else if (err == WSAEOPNOTSUPP) err = EOPNOTSUPP;
#endif
#if defined(WSAESHUTDOWN) && (WSAESHUTDOWN != EPIPE)
   else if (err == WSAESHUTDOWN) err = EPIPE;
#endif

   /* extras used by recv() */
#if defined(WSAECONNREFUSED) && (WSAECONNREFUSED != ECONNREFUSED)
   else if (err == WSAECONNREFUSED) err = ECONNREFUSED;
#endif

   /* extras used by getsockopt() */
#if defined(WSAENOPROTOOPT) && (WSAENOPROTOOPT != ENOPROTOOPT)
   else if (err == WSAENOPROTOOPT) err = ENOPROTOOPT;
#endif

   return err;
#endif
}

/**
 * Join a multicast group specified by address.
 *
 * Address must be an IPv4 or IPv6 depending on @a fd and will be
 * parsed using inet_pton() with corresponding @a family. The address
 * may contain an '@@' delimiter to specify the local interface IP
 * address to use. No interface means '0.0.0.0'.
 *
 * @param fd socket to operate on.
 * @param family the socket family of fd, AF_INET or AF_INET6.
 * @param address the address in the format IP[@@IFACE]
 *
 * @return 0 on success, errno mapping otherwise.
 * @internal
 */
Eina_Error efl_net_multicast_join(SOCKET fd, int family, const char *address);

/**
 * Leave a multicast group specified by address.
 *
 * This reverses the effect of efl_net_multicast_join().
 *
 * @param fd socket to operate on.
 * @param family the socket family of fd, AF_INET or AF_INET6.
 * @param address the address in the format IP[@@IFACE]
 *
 * @return 0 on success, errno mapping otherwise.
 * @internal
 */
Eina_Error efl_net_multicast_leave(SOCKET fd, int family, const char *address);

/**
 * Sets the Time-To-Live of multicast packets. <= 1 disables going
 * outside of local network.
 *
 * @param fd socket to operate on.
 * @param family the socket family of fd, AF_INET or AF_INET6.
 * @param ttl the time-to-live in units.
 *
 * @return 0 on success, errno mapping otherwise.
 * @internal
 */
Eina_Error efl_net_multicast_ttl_set(SOCKET fd, int family, uint8_t ttl);

/**
 * Retrieves the current time-to-live of multicast packets.
 *
 * @param fd socket to operate on.
 * @param family the socket family of fd, AF_INET or AF_INET6.
 * @param[out] ttl returns the time-to-live in units.
 *
 * @return 0 on success, errno mapping otherwise.
 * @internal
 */
Eina_Error efl_net_multicast_ttl_get(SOCKET fd, int family, uint8_t *ttl);

/**
 * Sets if the current local address should get a copy of the packets sent.
 *
 * @param fd socket to operate on.
 * @param family the socket family of fd, AF_INET or AF_INET6.
 * @param loopback if #EINA_TRUE, enables receive of local copy. #EINA_FALSE means only remote peers will do.
 *
 * @return 0 on success, errno mapping otherwise.
 * @internal
 */
Eina_Error efl_net_multicast_loopback_set(SOCKET fd, int family, Eina_Bool loopback);

/**
 * Gets if the current local address should get a copy of the packets sent.
 *
 * @param fd socket to operate on.
 * @param family the socket family of fd, AF_INET or AF_INET6.
 * @param[out] loopback returns if #EINA_TRUE, enables receive of local copy. #EINA_FALSE means only remote peers will do.
 *
 * @return 0 on success, errno mapping otherwise.
 * @internal
 */
Eina_Error efl_net_multicast_loopback_get(SOCKET fd, int family, Eina_Bool *loopback);

/**
 * Query the size of the next UDP datagram pending on queue.
 *
 * @param fd socket to operate on.
 * @return the size in bytes.
 * @internal
 */
size_t efl_net_udp_datagram_size_query(SOCKET fd);


/* SSL abstraction API */
extern void *efl_net_ssl_context_connection_new(Efl_Net_Ssl_Context *context);

#endif
