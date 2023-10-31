/*
Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT
*/

/*
 * XVC server interface
 *
 * This interface can be used to implement XVC server using this
 * reusable XVC protocol implementation.
 *
 * To start the XVC server an implementation should call the
 * xvcserver_start() function.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XvcClient XvcClient;

typedef enum  {
    LOG_MODE_DEFAULT,
    LOG_MODE_VERBOSE,
    LOG_MODE_QUIET,
} LoggingMode;

enum ReturnErrorCodes {
    NO_ERROR                         = 0,
    ERROR_INVALID_ARGUMENT           = 1,
    ERROR_LOOPBACK_TEST_FAILED       = 2,
    ERROR_INVALID_URL_TRANSPORT_TYPE = 3,
    ERROR_INVALID_URL_FIELD          = 4,
    ERROR_SOCKET_CREATION            = 5,
    ERROR_GETHOSTNAME_FAILED         = 6,
    ERROR_HSDP_OPEN_FAILED           = 7
};

/*
 * XVC server callback function table.
 */
typedef struct XvcServerHandlers {
    /* Called when a connection is established to allow the
     * implementation to initialize the virtual cable and save the
     * XvcClient pointer needed to report errors. */
    int (*open_port)(
        void * client_data, XvcClient * c);

    /* Called when a connection is closed to allow the implementation
     * to close the virtual cable. */
    void (*close_port)(
        void * client_data);

    /* Called to notify the implementation that the effect of any
     * pending commands must be completed.  This callback is optional
     * and must be set to NULL when not implemented. */
    int (*flush)(
        void * client_data);

    /* Called when the lock: command is received to lock the scan
     * chain and prevent sources other than the xvcserver to perform
     * scan chain operations until the next unlock() callback.  If the
     * lock cannot be acquired within <timeout> seconds then the error
     * "TIMEOUT" should be generated using the xvcserver_set_error()
     * function.  This callback is optional and must be set to NULL
     * when not implemented. */
    void (*lock)(
        void * client_data,
        unsigned timeout);

    /* Called when the unlock: command is received to unlock the scan
     * chain and allow other sources to perform scan chain operations.
     * This callback is optional and must be set to NULL when not
     * implemented. */
    void (*unlock)(
        void * client_data);

    /* Called when the idpc: command is received to send a packet from
     * client to the Debug Packet Controller.
     */
    void (*idpc)(
        void * client_data,
        unsigned flags,
        size_t num_bytes,
        unsigned char * buf);

    /* Called when the edpc: command is received to get a packet
     * received from the Debug Packet Controller.
     */
    void (*edpc)(
        void * client_data,
        unsigned flags,
        size_t * num_bytes,
        unsigned char ** buf);
} XvcServerHandlers;

/*
 * This function can be used by callback functions to report errors.
 */
void xvcserver_set_error(
    XvcClient * c,
    const char * fmt, ...);

/*
 * Start XVC server listing for incomming connections on <url>.  This
 * function will wait indefinitely for incomming connections.  When a
 * connection is established this function will initiate callback
 * functions defined in <handlers>.  Each callback will be passed the
 * <client_data> argument given to this function in addition to other
 * callback specific arguments.
 */
int xvcserver_start(
    const char * url,
    void * client_data,
    XvcServerHandlers * handlers,
    LoggingMode log_mode);

#ifdef __cplusplus
}
#endif
