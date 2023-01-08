/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * COMMENTS:
 * For parsing help, there is a variable naming convention we maintain:
 *      strings:            char * and does not end in "buffer"
 *      binary:             char * and is either exactly "buffer" or ends
 *                          with "_buffer"
 *      single values:      don't end with "_array" or "_buffer"
 *      arrays:             <type> * and is either "array" or ends
 *                          with "_array". Use "_fixed_array" to skip resize using
 *                          tupleSize for the thrift generator.
 *      array length:       is either "length", "count", or ends with
 *                          "_length" or "_count". Use "_fixed_array" to skip resize
 *                          using tupleSize for the thrift generator.
 */

#ifndef __HAPI_h__
#define __HAPI_h__

#include "HAPI_API.h"
#include "HAPI_Common.h"
#include "HAPI_Helpers.h"

/// @defgroup Sessions
/// Functions for creating and inspecting HAPI session state.

/// @brief  Creates a new in-process session.  There can only be
///         one such session per host process.
///
/// @ingroup Sessions
///
/// @param[out]     session
///                 A ::HAPI_Session struct to receive the session id,
///                 in this case always 0.
///
HAPI_DECL HAPI_CreateInProcessSession( HAPI_Session * session );

/// @brief  Starts a Thrift RPC server process on the local host serving
///         clients on a TCP socket and waits for it to start serving.
///         It is safe to create an RPC session on local host using the
///         specified port after this call succeeds.
///
/// @ingroup Sessions
/// @param[in]      options
///                 Options to configure the server being started.
///
/// @param[in]      port
///                 The TCP socket to create on the server.
///
/// @param[out]     process_id
///                 The process id of the server, if started successfully.
///
HAPI_DECL HAPI_StartThriftSocketServer(
                                    const HAPI_ThriftServerOptions * options,
                                    int port,
                                    HAPI_ProcessId * process_id );

/// @brief  Creates a Thrift RPC session using a TCP socket as transport.
///
/// @ingroup Sessions
/// @param[out]     session
///                 A ::HAPI_Session struct to receive the unique session id
///                 of the new session.
///
/// @param[in]      host_name
///                 The name of the server host.
///
/// @param[in]      port
///                 The server port to connect to.
///
HAPI_DECL HAPI_CreateThriftSocketSession( HAPI_Session * session,
                                          const char * host_name,
                                          int port );

/// @brief  Starts a Thrift RPC server process on the local host serving
///         clients on a Windows named pipe or a Unix domain socket and
///         waits for it to start serving. It is safe to create an RPC
///         session using the specified pipe or socket after this call
///         succeeds.
///
/// @ingroup Sessions
///
/// @param[in]      options
///                 Options to configure the server being started.
///
/// @param[in]      pipe_name
///                 The name of the pipe or socket.
///
/// @param[out]     process_id
///                 The process id of the server, if started successfully.
///
HAPI_DECL HAPI_StartThriftNamedPipeServer(
                                    const HAPI_ThriftServerOptions * options,
                                    const char * pipe_name,
                                    HAPI_ProcessId * process_id );

/// @brief  Creates a Thrift RPC session using a Windows named pipe
///         or a Unix domain socket as transport.
///
/// @ingroup Sessions
///
/// @param[out]     session
///                 A ::HAPI_Session struct to receive the unique session id
///                 of the new session.
///
/// @param[in]      pipe_name
///                 The name of the pipe or socket.
///
HAPI_DECL HAPI_CreateThriftNamedPipeSession( HAPI_Session * session,
                                             const char * pipe_name );

/// @brief  Binds a new implementation DLL to one of the custom session
///         slots.
///
/// @ingroup Sessions
///
/// @param[in]      session_type
///                 Which custom implementation slot to bind the
///                 DLL to. Must be one of ::HAPI_SESSION_CUSTOM1,
///                 ::HAPI_SESSION_CUSTOM2, or ::HAPI_SESSION_CUSTOM3.
///
/// @param[in]      dll_path
///                 The path to the custom implementation DLL.
///
HAPI_DECL HAPI_BindCustomImplementation( HAPI_SessionType session_type,
                                         const char * dll_path );

/// @brief  Creates a new session using a custom implementation.
///         Note that the implementation DLL must already have
///         been bound to the session via calling
///         ::HAPI_BindCustomImplementation().
///
/// @ingroup Sessions
///
/// @param[in]      session_type
///                 session_type indicates which custom session
///                 slot to create the session on.
///
/// @param[in,out]  session_info
///                 Any data required by the custom implementation to
///                 create its session.
///
/// @param[out]     session
///                 A ::HAPI_Session struct to receive the session id,
///                 The sessionType parameter of the struct should
///                 also match the session_type parameter passed in.
///
HAPI_DECL HAPI_CreateCustomSession( HAPI_SessionType session_type,
                                    void * session_info,
                                    HAPI_Session * session );

/// @brief  Checks whether the session identified by ::HAPI_Session::id is
///         a valid session opened in the implementation identified by
///         ::HAPI_Session::type.
///
/// @ingroup Sessions
///
/// @param[in]      session
///                 The ::HAPI_Session to check.
///
/// @return         ::HAPI_RESULT_SUCCESS if the session is valid.
///                 Otherwise, the session is invalid and passing it to
///                 other HAPI calls may result in undefined behavior.
///
HAPI_DECL HAPI_IsSessionValid( const HAPI_Session * session );

/// @brief  Closes a session. If the session has been established using
///         RPC, then the RPC connection is closed.
///
/// @ingroup Sessions
///
/// @param[in]     session
///                The HAPI_Session to close. After this call, this
///                session is invalid and passing it to HAPI calls other
///                than ::HAPI_IsSessionValid() may result in undefined
///                behavior.
///
HAPI_DECL HAPI_CloseSession( const HAPI_Session * session );

// INITIALIZATION / CLEANUP -------------------------------------------------

/// @brief  Check whether the runtime has been initialized yet using
///         ::HAPI_Initialize(). Function will return ::HAPI_RESULT_SUCCESS
///         if the runtime has been initialized and ::HAPI_RESULT_NOT_INITIALIZED
///         otherwise.
///
/// @ingroup Sessions
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_IsInitialized( const HAPI_Session * session );

/// @brief  Create the asset manager, set up environment variables, and
///         initialize the main Houdini scene. No license check is done
///         during this step. Only when you try to load an asset library
///         (OTL) do we actually check for licenses.
///
/// @ingroup Sessions
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      cook_options
///                 Global cook options used by subsequent default cooks.
///                 This can be overwritten by individual cooks but if
///                 you choose to instantiate assets with cook_on_load
///                 set to true then these cook options will be used.
///
/// @param[in]      use_cooking_thread
///                 Use a separate thread for cooking of assets. This
///                 allows for asynchronous cooking and larger stack size.
///                 <!-- default true -->
///
/// @param[in]      cooking_thread_stack_size
///                 Set the stack size of the cooking thread. Use -1 to
///                 set the stack size to the Houdini default. This
///                 value is in bytes.
///                 <!-- default -1 -->
///
/// @param[in]      houdini_environment_files
///                 A list of paths, separated by a ";" on Windows and a ":"
///                 on Linux and Mac, to .env files that follow the same
///                 syntax as the houdini.env file in Houdini's user prefs
///                 folder. These will be applied after the default
///                 houdini.env file and will overwrite the process'
///                 environment variable values. You an use this to enforce
///                 a stricter environment when running engine.
///                 For more info, see:
///                 http://www.sidefx.com/docs/houdini/basics/config_env
///                 <!-- default NULL -->
///
/// @param[in]      otl_search_path
///                 The directory where OTLs are searched for. You can
///                 pass NULL here which will only use the default
///                 Houdini OTL search paths. You can also pass in
///                 multiple paths separated by a ";" on Windows and a ":"
///                 on Linux and Mac. If something other than NULL is
///                 passed the default Houdini search paths will be
///                 appended to the end of the path string.
///                 <!-- default NULL -->
///
/// @param[in]      dso_search_path
///                 The directory where generic DSOs (custom plugins) are
///                 searched for. You can pass NULL here which will
///                 only use the default Houdini DSO search paths. You
///                 can also pass in multiple paths separated by a ";"
///                 on Windows and a ":" on Linux and Mac. If something
///                 other than NULL is passed the default Houdini search
///                 paths will be appended to the end of the path string.
///                 <!-- default NULL -->
///
/// @param[in]      image_dso_search_path
///                 The directory where image DSOs (custom plugins) are
///                 searched for. You can pass NULL here which will
///                 only use the default Houdini DSO search paths. You
///                 can also pass in multiple paths separated by a ";"
///                 on Windows and a ":" on Linux and Mac. If something
///                 other than NULL is passed the default Houdini search
///                 paths will be appended to the end of the path string.
///                 <!-- default NULL -->
///
/// @param[in]      audio_dso_search_path
///                 The directory where audio DSOs (custom plugins) are
///                 searched for. You can pass NULL here which will
///                 only use the default Houdini DSO search paths. You
///                 can also pass in multiple paths separated by a ";"
///                 on Windows and a ":" on Linux and Mac. If something
///                 other than NULL is passed the default Houdini search
///                 paths will be appended to the end of the path string.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_Initialize( const HAPI_Session * session,
                           const HAPI_CookOptions * cook_options,
                           HAPI_Bool use_cooking_thread,
                           int cooking_thread_stack_size,
                           const char * houdini_environment_files,
                           const char * otl_search_path,
                           const char * dso_search_path,
                           const char * image_dso_search_path,
                           const char * audio_dso_search_path );

/// @brief  Clean up memory. This will unload all assets and you will
///         need to call ::HAPI_Initialize() again to be able to use any
///         HAPI methods again.
///
///         @note This does not release any licenses.  The license will be returned when
///         the process terminates.
///
/// @ingroup Sessions
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_Cleanup( const HAPI_Session * session );

/// @brief  When using an in-process session, this method **must** be called in
///         order for the host process to shutdown cleanly. This method should
///         be called before ::HAPI_CloseSession(). 
///
///         @note This method should only be called before exiting the program,
///         because HAPI can no longer be used by the process once this method
///         has been called.
///
/// @ingroup Sessions
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_Shutdown( const HAPI_Session * session );

/// @defgroup Environment
/// Functions for reading and writing to the session environment

/// @brief  Gives back a certain environment integers like version number.
///         Note that you do not need a session for this. These constants
///         are hard-coded in all HAPI implementations, including HARC and
///         HAPIL. This should be the first API you call to determine if
///         any future API calls will mismatch implementation.
///
/// @ingroup Environment
///
/// @param[in]      int_type
///                 One of ::HAPI_EnvIntType.
///
/// @param[out]     value
///                 Int value.
///
HAPI_DECL HAPI_GetEnvInt( HAPI_EnvIntType int_type, int * value );

/// @brief  Gives back a certain session-specific environment integers
///         like current license type being used.
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      int_type
///                 One of ::HAPI_SessionEnvIntType.
///
/// @param[out]     value
///                 Int value.
///
HAPI_DECL HAPI_GetSessionEnvInt( const HAPI_Session * session,
                                 HAPI_SessionEnvIntType int_type,
                                 int * value );

/// @brief  Get environment variable from the server process as an integer.
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      variable_name
///                 Name of the environmnet variable.
///
/// @param[out]     value
///                 The int pointer to return the value in.
///
HAPI_DECL HAPI_GetServerEnvInt( const HAPI_Session * session,
                                const char * variable_name,
                                int * value );

/// @brief  Get environment variable from the server process as a string.
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      variable_name
///                 Name of the environmnet variable.
///
/// @param[out]     value
///                 The HAPI_StringHandle pointer to return the value in.
///
HAPI_DECL HAPI_GetServerEnvString( const HAPI_Session * session,
                                   const char * variable_name,
                                   HAPI_StringHandle * value );

/// @brief  Provides the number of environment variables that are in
///         the server environment's process
///
///         Note that ::HAPI_GetServerEnvVarList() should be called directly after
///         this method, otherwise there is the possibility that the environment
///         variable count of the server will have changed by the time that
///         ::HAPI_GetServerEnvVarList() is called.
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     env_count
///                 A pointer to an int to return the value in
HAPI_DECL HAPI_GetServerEnvVarCount( const HAPI_Session * session,
                                     int * env_count );

/// @brief  Provides a list of all of the environment variables
///         in the server's process
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     values_array
///                 An ::HAPI_StringHandle array at least the size of length
///
/// @param[in]      start
///                 First index of range. Must be at least @c 0 and at most
///                 @c env_count - 1 where @c env_count is the count returned by
///                 ::HAPI_GetServerEnvVarCount()
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetServerEnvVarCount -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Given @c env_count returned by ::HAPI_GetServerEnvVarCount(),
///                 length should be at least @c 0 and at most <tt>env_count - start.</tt>
///                 <!-- default 0 -->
HAPI_DECL HAPI_GetServerEnvVarList( const HAPI_Session * session,
                                    HAPI_StringHandle * values_array,
                                    int start,
                                    int length );

/// @brief  Set environment variable for the server process as an integer.
///
///         Note that this may affect other sessions on the same server
///         process. The session parameter is mainly there to identify the
///         server process, not the specific session.
///
///         For in-process sessions, this will affect the current process's
///         environment.
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      variable_name
///                 Name of the environment variable.
///
/// @param[in]      value
///                 The integer value.
///
HAPI_DECL HAPI_SetServerEnvInt( const HAPI_Session * session,
                                const char * variable_name,
                                int value );

/// @brief  Set environment variable for the server process as a string.
///
///         Note that this may affect other sessions on the same server
///         process. The session parameter is mainly there to identify the
///         server process, not the specific session.
///
///         For in-process sessions, this will affect the current process's
///         environment.
///
/// @ingroup Environment
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      variable_name
///                 Name of the environmnet variable.
///
/// @param[in]      value
///                 The string value.
///
HAPI_DECL HAPI_SetServerEnvString( const HAPI_Session * session,
                                   const char * variable_name,
                                   const char * value );

/// @defgroup Status
/// Functions for reading session connection and cook status.

/// @brief  Gives back the status code for a specific status type.
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      status_type
///                 One of ::HAPI_StatusType.
///
/// @param[out]     status
///                 Actual status code for the status type given. That is,
///                 if you pass in ::HAPI_STATUS_CALL_RESULT as
///                 status_type, you'll get back a ::HAPI_Result for this
///                 argument. If you pass in ::HAPI_STATUS_COOK_STATE
///                 as status_type, you'll get back a ::HAPI_State enum
///                 for this argument.
///
HAPI_DECL HAPI_GetStatus( const HAPI_Session * session,
                          HAPI_StatusType status_type,
                          int * status );

/// @brief  Return length of string buffer storing status string message.
///
///         If called with ::HAPI_STATUS_COOK_RESULT this will actually
///         parse the node networks for the previously cooked asset(s)
///         and aggregate all node errors, warnings, and messages
///         (depending on the @c verbosity level set). Usually this is done
///         just for the last cooked single asset but if you load a whole
///         Houdini scene using ::HAPI_LoadHIPFile() then you'll have
///         multiple "previously cooked assets".
///
///         You MUST call ::HAPI_GetStatusStringBufLength() before calling
///         ::HAPI_GetStatusString() because ::HAPI_GetStatusString() will
///         not return the real status string and instead return a
///         cached version of the string that was created inside
///         ::HAPI_GetStatusStringBufLength(). The reason for this is that
///         the length of the real status string may change between
///         the call to ::HAPI_GetStatusStringBufLength() and the call to
///         ::HAPI_GetStatusString().
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      status_type
///                 One of ::HAPI_StatusType.
///
/// @param[in]      verbosity
///                 Preferred verbosity level.
///
/// @param[out]     buffer_length
///                 Length of buffer char array ready to be filled.
///
HAPI_DECL HAPI_GetStatusStringBufLength( const HAPI_Session * session,
                                         HAPI_StatusType status_type,
                                         HAPI_StatusVerbosity verbosity,
                                         int * buffer_length );

/// @brief  Return status string message.
///
///         You MUST call ::HAPI_GetStatusStringBufLength() before calling
///         ::HAPI_GetStatusString() because ::HAPI_GetStatusString() will
///         not return the real status string and instead return a
///         cached version of the string that was created inside
///         ::HAPI_GetStatusStringBufLength(). The reason for this is that
///         the length of the real status string may change between
///         the call to ::HAPI_GetStatusStringBufLength() and the call to
///         ::HAPI_GetStatusString().
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      status_type
///                 One of ::HAPI_StatusType.
///
/// @param[out]     string_value
///                 Buffer char array ready to be filled.
///
/// @param[in]      length
///                 Length of the string buffer (must match size of
///                 @p string_value - so including NULL terminator).
///                 <!-- source ::HAPI_GetStatusStringBufLength -->
///
HAPI_DECL HAPI_GetStatusString( const HAPI_Session * session,
                                HAPI_StatusType status_type,
                                char * string_value,
                                int length );

/// @brief  Compose the cook result string (errors and warnings) of a
///         specific node.
///
///         This will actually parse the node network inside the given
///         node and return ALL errors/warnings/messages of all child nodes,
///         combined into a single string. If you'd like a more narrowed
///         search, call this function on one of the child nodes.
///
///         You MUST call ::HAPI_ComposeNodeCookResult() before calling
///         ::HAPI_GetComposedNodeCookResult() because
///         ::HAPI_GetComposedNodeCookResult() will
///         not return the real result string and instead return a
///         cached version of the string that was created inside
///         ::HAPI_ComposeNodeCookResult(). The reason for this is that
///         the length of the real status string may change between
///         the call to ::HAPI_ComposeNodeCookResult() and the call to
///         ::HAPI_GetComposedNodeCookResult().
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      verbosity
///                 Preferred verbosity level.
///
/// @param[out]     buffer_length
///                 Length of buffer char array ready to be filled.
///
HAPI_DECL HAPI_ComposeNodeCookResult( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_StatusVerbosity verbosity,
                                      int * buffer_length );

/// @brief  Return cook result string message on a single node.
///
///         You MUST call ::HAPI_ComposeNodeCookResult() before calling
///         ::HAPI_GetComposedNodeCookResult() because
///         ::HAPI_GetComposedNodeCookResult() will
///         not return the real result string and instead return a
///         cached version of the string that was created inside
///         ::HAPI_ComposeNodeCookResult(). The reason for this is that
///         the length of the real status string may change between
///         the call to ::HAPI_ComposeNodeCookResult() and the call to
///         ::HAPI_GetComposedNodeCookResult().
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     string_value
///                 Buffer char array ready to be filled.
///
/// @param[in]      length
///                 Length of the string buffer (must match size of
///                 @p string_value - so including NULL terminator).
///                 <!-- source ::HAPI_ComposeNodeCookResult -->
///
HAPI_DECL HAPI_GetComposedNodeCookResult( const HAPI_Session * session,
                                          char * string_value,
                                          int length );

/// @brief  Recursively check for specific errors by error code on a node.
///
///         Note that checking for errors can be expensive because it checks
///         ALL child nodes within a node and then tries to do a string match
///         for the errors being looked for. This is why such error checking
///         is part of a standalone function and not done during the cooking
///         step.
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      errors_to_look_for
///                 The HAPI_ErrorCode error codes (as a bitfield) to look for.
///
/// @param[out]     errors_found
///                 Returned HAPI_ErrorCode bitfield indicating which of the
///                 looked for errors have been found.
///
HAPI_DECL HAPI_CheckForSpecificErrors( const HAPI_Session * session,
                                       HAPI_NodeId node_id,
                                       HAPI_ErrorCodeBits errors_to_look_for,
                                       HAPI_ErrorCodeBits * errors_found );

/// @brief  Clears the connection error. Should be used before starting
///         or creating Thrift server.
///
///         Only available when using Thrift connections.
///
/// @ingroup Status
///
HAPI_DECL HAPI_ClearConnectionError( );

/// @brief  Return the length of string buffer storing connection error
///         message.
///
/// @ingroup Status
///
///         Only available when using Thrift connections.
///
/// @param[out]     buffer_length
///                 Length of buffer char array ready to be filled.
///
HAPI_DECL HAPI_GetConnectionErrorLength( int * buffer_length );

/// @brief  Return the connection error message.
///
///         You MUST call ::HAPI_GetConnectionErrorLength() before calling
///         this to get the correct string length.
///
///         Only available when using Thrift connections.
///
/// @ingroup Status
///
/// @param[out]     string_value
///                 Buffer char array ready to be filled.
///
/// @param[in]      length
///                 Length of the string buffer (must match size of
///                 string_value - so including NULL terminator).
///                 Use ::HAPI_GetConnectionErrorLength to get this length.
///
/// @param[in]      clear
///                 If true, will clear the error when HAPI_RESULT_SUCCESS
///                 is returned.
///
HAPI_DECL HAPI_GetConnectionError( char * string_value, 
                                   int length,
                                   HAPI_Bool clear );

/// @brief  Get total number of nodes that need to cook in the current
///         session.
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     count
///                 Total cook count.
///
HAPI_DECL HAPI_GetCookingTotalCount( const HAPI_Session * session,
                                     int * count );

/// @brief  Get current number of nodes that have already cooked in the
///         current session. Note that this is a very crude approximation
///         of the cooking progress - it may never make it to 100% or it
///         might spend another hour at 100%. Use ::HAPI_GetStatusString
///         to get a better idea of progress if this number gets stuck.
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     count
///                 Current cook count.
///
HAPI_DECL HAPI_GetCookingCurrentCount( const HAPI_Session * session,
                                       int * count );

/// @brief  Interrupt a cook or load operation.
///
/// @ingroup Status
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_Interrupt( const HAPI_Session * session );

/// @defgroup Utility
/// Utility math and other functions

/// @brief  Converts the transform described by a ::HAPI_TransformEuler
///         struct into a different transform and rotation order.
///
/// @ingroup Utility
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      transform_in
///                 The transform to be converted.
///
/// @param[in]      rst_order
///                 The desired transform order of the output.
///
/// @param[in]      rot_order
///                 The desired rotation order of the output.
///
/// @param[out]     transform_out
///                 The converted transform.
///
HAPI_DECL HAPI_ConvertTransform( const HAPI_Session * session,
                                 const HAPI_TransformEuler * transform_in,
                                 HAPI_RSTOrder rst_order,
                                 HAPI_XYZOrder rot_order,
                                 HAPI_TransformEuler * transform_out );

/// @brief  Converts a 4x4 matrix into its TRS form.
///
/// @ingroup Utility
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      matrix
///                 A 4x4 matrix expressed in a 16 element float array.
///
/// @param[in]      rst_order
///                 The desired transform order of the output.
///
/// @param[out]     transform_out
///                 Used for the output.
///
HAPI_DECL HAPI_ConvertMatrixToQuat( const HAPI_Session * session,
                                    const float * matrix,
                                    HAPI_RSTOrder rst_order,
                                    HAPI_Transform * transform_out );

/// @brief  Converts a 4x4 matrix into its TRS form.
///
/// @ingroup Utility
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      matrix
///                 A 4x4 matrix expressed in a 16 element float array.
///
/// @param[in]      rst_order
///                 The desired transform order of the output.
///
/// @param[in]      rot_order
///                 The desired rotation order of the output.
///
/// @param[out]     transform_out
///                 Used for the output.
///
HAPI_DECL HAPI_ConvertMatrixToEuler( const HAPI_Session * session,
                                     const float * matrix,
                                     HAPI_RSTOrder rst_order,
                                     HAPI_XYZOrder rot_order,
                                     HAPI_TransformEuler * transform_out );

/// @brief  Converts ::HAPI_Transform into a 4x4 transform matrix.
///
/// @ingroup Utility
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      transform
///                 The ::HAPI_Transform you wish to convert.
///
/// @param[out]     matrix
///                 A 16 element float array that will contain the result.
///
HAPI_DECL HAPI_ConvertTransformQuatToMatrix( const HAPI_Session * session,
                                             const HAPI_Transform * transform,
                                             float * matrix );

/// @brief  Converts ::HAPI_TransformEuler into a 4x4 transform matrix.
///
/// @ingroup Utility
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      transform
///                 The ::HAPI_TransformEuler you wish to convert.
///
/// @param[out]     matrix
///                 A 16 element float array that will contain the result.
///
HAPI_DECL HAPI_ConvertTransformEulerToMatrix(
                                    const HAPI_Session * session,
                                    const HAPI_TransformEuler * transform,
                                    float * matrix );

/// @brief  Acquires or releases the Python interpreter lock. This is
///         needed if HAPI is called from Python and HAPI is in threaded
///         mode (see ::HAPI_Initialize()).
///
///         The problem arises when async functions like
///         ::HAPI_CreateNode() may start a cooking thread that
///         may try to run Python code. That is, we would now have
///         Python running on two different threads - something not
///         allowed by Python by default.
///
///         We need to tell Python to explicitly "pause" the Python state
///         on the client thread while we run Python in our cooking thread.
///
///         You must call this function first with locked == true before
///         any async HAPI call. Then, after the async call finished,
///         detected via calls to ::HAPI_GetStatus(), call this method
///         again to release the lock with locked == false.
///
/// @ingroup Utility
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      locked
///                 True will acquire the interpreter lock to use it for
///                 the HAPI cooking thread. False will release the lock
///                 back to the client thread.
///
HAPI_DECL HAPI_PythonThreadInterpreterLock( const HAPI_Session * session,
                                            HAPI_Bool locked );

/// @defgroup Strings
/// Functions for handling strings.

/// @brief  Gives back the string length of the string with the
///         given handle.
///
/// @ingroup Strings
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      string_handle
///                 Handle of the string to query.
///
/// @param[out]     buffer_length
///                 Buffer length of the queried string (including NULL
///                 terminator).
///
HAPI_DECL HAPI_GetStringBufLength( const HAPI_Session * session,
                                   HAPI_StringHandle string_handle,
                                   int * buffer_length );

/// @brief  Gives back the string value of the string with the
///         given handle.
///
/// @ingroup Strings
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      string_handle
///                 Handle of the string to query.
///
/// @param[out]     string_value
///                 Actual string value (character array).
///
/// @param[in]      length
///                 Length of the string buffer (must match size of
///                 @p string_value - so including NULL terminator).
///
HAPI_DECL HAPI_GetString( const HAPI_Session * session,
                          HAPI_StringHandle string_handle,
                          char * string_value,
                          int length );

/// @brief  Adds the given string to the string table and returns 
///         the handle. It is the responsibility of the caller to
///         manage access to the string. The intended use for custom strings
///         is to allow structs that reference strings to be passed in to HAPI
///
/// @ingroup Strings
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      string_value
///                 Actual string value (character array).
///
/// @param[out]     handle_value
///                 Handle of the string that was added
///
HAPI_DECL HAPI_SetCustomString( const HAPI_Session * session,
                                const char * string_value,
                                HAPI_StringHandle * handle_value );

/// @brief  Removes the specified string from the server
///         and invalidates the handle
///
/// @ingroup Strings
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]     string_handle
///                 Handle of the string that was added
///
HAPI_DECL HAPI_RemoveCustomString( const HAPI_Session * session,
                                   const HAPI_StringHandle string_handle );

/// @brief  Gives back the length of the buffer needed to hold
///         all the values null-separated for the given string 
///         handles.  Used with ::HAPI_GetStringBatch().
///
/// @ingroup Strings
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      string_handle_array
///                 Array of string handles to be read.
///
/// @param[in]      string_handle_count
///                 Length of @p string_handle_array
///
/// @param[out]     string_buffer_size
///                 Buffer length required for subsequent call to
///                 HAPI_GetStringBatch to hold all the given 
///                 string values null-terminated
///
HAPI_DECL HAPI_GetStringBatchSize( const HAPI_Session * session,
                                   const int * string_handle_array,
                                   int string_handle_count,
                                   int * string_buffer_size );

/// @brief  Gives back the values of the given string handles.
///         The given char array is filled with null-separated 
///         values, and the final value is null-terminated.
///         Used with ::HAPI_GetStringBatchSize().  Using this function
///         instead of repeated calls to ::HAPI_GetString() can be more
///         more efficient for a large number of strings.
///
/// @ingroup Strings
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     char_buffer
///                 Array of characters to hold string values.
///
/// @param[in]      char_array_length
///                 Length of @p char_array.  Must be large enough to hold
///                 all the string values including null separators.
///                 <!-- min ::HAPI_GetStringBatchSize -->
///                 <!-- source ::HAPI_GetStringBatchSize -->
///
HAPI_DECL HAPI_GetStringBatch( const HAPI_Session * session,
                               char * char_buffer,
                               int char_array_length );


/// @defgroup Time
/// Time related functions

/// @brief  Gets the global time of the scene. All API calls deal with
///         this time to cook.
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     time
///                 Time as a float in seconds.
///
HAPI_DECL HAPI_GetTime( const HAPI_Session * session, float * time );

/// @brief  Sets the global time of the scene. All API calls will deal
///         with this time to cook.
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      time
///                 Time as a float in seconds.
///
HAPI_DECL HAPI_SetTime( const HAPI_Session * session, float time );

/// @brief  Returns whether the Houdini session will use the current time in
///         Houdini when cooking and retrieving data. By default this is
///         disabled and the Houdini session uses time 0 (i.e. frame 1).
///         In SessionSync, it is enabled by default, but can be overridden.
///         Note that this function will ALWAYS return
///         ::HAPI_RESULT_SUCCESS.
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     enabled
///                 Whether use Houdini time is enabled or not.
///
HAPI_DECL HAPI_GetUseHoudiniTime( const HAPI_Session * session, 
                                  HAPI_Bool * enabled );

/// @brief  Sets whether the Houdini session should use the current time in
///         Houdini when cooking and retrieving data. By default this is
///         disabled and the Houdini session uses time 0 (i.e. frame 1).
///         In SessionSync, it is enabled by default, but can be overridden.
///         Note that this function will ALWAYS return
///         ::HAPI_RESULT_SUCCESS.
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      enabled
///                 Set to true to use Houdini time.
///
HAPI_DECL HAPI_SetUseHoudiniTime( const HAPI_Session * session, 
                                  HAPI_Bool enabled );

/// @brief  Gets the current global timeline options.
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     timeline_options
///                 The global timeline options struct.
///
HAPI_DECL HAPI_GetTimelineOptions( const HAPI_Session * session,
                                   HAPI_TimelineOptions * timeline_options );

/// @brief  Sets the global timeline options.
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      timeline_options
///                 The global timeline options struct.
///
HAPI_DECL HAPI_SetTimelineOptions(
                            const HAPI_Session * session,
                            const HAPI_TimelineOptions * timeline_options );

/// @brief  Gets the global compositor options.
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     compositor_options
///                 The compositor options struct.
///
HAPI_DECL HAPI_GetCompositorOptions(
                            const HAPI_Session * session,
                            HAPI_CompositorOptions * compositor_options);

/// @brief  Sets the global compositor options.
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      compositor_options
///                 The compositor options.
///
HAPI_DECL HAPI_SetCompositorOptions(
                        const HAPI_Session * session,
                        const HAPI_CompositorOptions * compositor_options);

/// @defgroup Assets
/// Functions for managing asset libraries

/// @brief  Loads a Houdini asset library (OTL) from a .otl file.
///         It does NOT create anything inside the Houdini scene.
///
///         @note This is when we actually check for valid licenses.
///
///         The next step is to call ::HAPI_GetAvailableAssetCount()
///         to get the number of assets contained in the library using the
///         returned library_id. Then call ::HAPI_GetAvailableAssets()
///         to get the list of available assets by name. Use the asset
///         names with ::HAPI_CreateNode() to actually create
///         one of these nodes in the Houdini scene and get back
///         an asset_id.
///
///         @note The HIP file saved using ::HAPI_SaveHIPFile() will only
///             have an absolute path reference to the loaded OTL meaning
///             that if the OTL is moved or renamed the HIP file won't
///             load properly. It also means that if you change the OTL
///             using the saved HIP scene the same OTL file will change
///             as the one used with Houdini Engine.
///             See @ref HAPI_Fundamentals_SavingHIPFile.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      file_path
///                 Absolute path to the .otl file.
///
/// @param[in]      allow_overwrite
///                 With this true, if the library file being loaded
///                 contains asset definitions that have already been
///                 loaded they will overwrite the existing definitions.
///                 Otherwise, a library containing asset definitions that
///                 already exist will fail to load, returning a
///                 ::HAPI_Result of
///                 ::HAPI_RESULT_ASSET_DEF_ALREADY_LOADED.
///
/// @param[out]     library_id
///                 Newly loaded otl id to be used with
///                 ::HAPI_GetAvailableAssetCount() and
///                 ::HAPI_GetAvailableAssets().
///
HAPI_DECL HAPI_LoadAssetLibraryFromFile( const HAPI_Session * session,
                                         const char * file_path,
                                         HAPI_Bool allow_overwrite,
                                         HAPI_AssetLibraryId * library_id );

/// @brief  Loads a Houdini asset library (OTL) from memory.
///         It does NOT create anything inside the Houdini scene.
///
///         @note This is when we actually check for valid licenses.
///
///         Please note that the performance benefit of loading a library
///         from memory are negligible at best. Due to limitations of
///         Houdini's library manager, there is still some disk access
///         and file writes because every asset library needs to be
///         saved to a real file. Use this function only as a convenience
///         if you already have the library file in memory and don't wish
///         to have to create your own temporary library file and then
///         call ::HAPI_LoadAssetLibraryFromFile().
///
///         The next step is to call ::HAPI_GetAvailableAssetCount()
///         to get the number of assets contained in the library using the
///         returned library_id. Then call ::HAPI_GetAvailableAssets()
///         to get the list of available assets by name. Use the asset
///         names with ::HAPI_CreateNode() to actually create
///         one of these nodes in the Houdini scene and get back
///         an asset_id.
///
///         @note The saved HIP file using ::HAPI_SaveHIPFile() will
///             @a contain the OTL loaded as part of its @b Embedded OTLs.
///             This means that you can safely move or rename the original
///             OTL file and the HIP will continue to work but if you make
///             changes to the OTL while using the saved HIP the changes
///             won't be saved to the original OTL.
///             See @ref HAPI_Fundamentals_SavingHIPFile.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      library_buffer
///                 The memory buffer containing the asset definitions
///                 in the same format as a standard Houdini .otl file.
///
/// @param[in]      library_buffer_length
///                 The size of the OTL memory buffer.
///
/// @param[in]      allow_overwrite
///                 With this true, if the library file being loaded
///                 contains asset definitions that have already been
///                 loaded they will overwrite the existing definitions.
///                 Otherwise, a library containing asset definitions that
///                 already exist will fail to load, returning a
///                 ::HAPI_Result of
///                 ::HAPI_RESULT_ASSET_DEF_ALREADY_LOADED.
///
/// @param[out]     library_id
///                 Newly loaded otl id to be used with
///                 ::HAPI_GetAvailableAssetCount() and
///                 ::HAPI_GetAvailableAssets().
///
HAPI_DECL HAPI_LoadAssetLibraryFromMemory( const HAPI_Session * session,
                                           const char * library_buffer,
                                           int library_buffer_length,
                                           HAPI_Bool allow_overwrite,
                                           HAPI_AssetLibraryId * library_id );

/// @brief  Get the number of assets contained in an asset library.
///         You should call ::HAPI_LoadAssetLibraryFromFile() prior to
///         get a library_id.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      library_id
///                 Returned by ::HAPI_LoadAssetLibraryFromFile().
///                 <!-- source ::HAPI_LoadAssetLibraryFromFile -->
///
/// @param[out]     asset_count
///                 The number of assets contained in this asset library.
///
HAPI_DECL HAPI_GetAvailableAssetCount( const HAPI_Session * session,
                                       HAPI_AssetLibraryId library_id,
                                       int * asset_count );

/// @brief  Get the names of the assets contained in an asset library.
///
///         The asset names will contain additional information about
///         the type of asset, namespace, and version, along with the
///         actual asset name. For example, if you have an Object type
///         asset, in the "hapi" namespace, of version 2.0, named
///         "foo", the asset name returned here will be:
///         hapi::Object/foo::2.0
///
///         However, you should not need to worry about this detail. Just
///         pass this string directly to ::HAPI_CreateNode() to
///         create the node. You can then get the pretty name
///         using ::HAPI_GetAssetInfo().
///
///         You should call ::HAPI_LoadAssetLibraryFromFile() prior to
///         get a library_id. Then, you should call
///         ::HAPI_GetAvailableAssetCount() to get the number of assets to
///         know how large of a string handles array you need to allocate.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      library_id
///                 Returned by ::HAPI_LoadAssetLibraryFromFile().
///                 <!-- source ::HAPI_LoadAssetLibraryFromFile -->
///
/// @param[out]     asset_names_array
///                 Array of string handles (integers) that should be
///                 at least the size of asset_count.
///
/// @param[in]     asset_count
///                 Should be the same or less than the value returned by
///                 ::HAPI_GetAvailableAssetCount().
///                 <!-- max ::HAPI_GetAvailableAssetCount -->
///                 <!-- source ::HAPI_GetAvailableAssetCount -->
///
HAPI_DECL HAPI_GetAvailableAssets( const HAPI_Session * session,
                                   HAPI_AssetLibraryId library_id,
                                   HAPI_StringHandle * asset_names_array,
                                   int asset_count );

/// @brief  Fill an asset_info struct from a node.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     asset_info
///                 Returned ::HAPI_AssetInfo struct.
///
HAPI_DECL HAPI_GetAssetInfo( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_AssetInfo * asset_info );

/// @brief  Get the number of asset parameters contained in an asset 
///         library, as well as the number of parameter int, float,
///         string, and choice values. 
///
///         This does not create the asset in the session.
///         Use this for faster querying of asset parameters compared to
///         creating the asset node and querying the node's parameters.
///
///         This does require ::HAPI_LoadAssetLibraryFromFile() to be
///         called prior, in order to load the asset library and
///         acquire library_id. Then ::HAPI_GetAvailableAssetCount and
///         ::HAPI_GetAvailableAssets should be called to get the
///         asset_name.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      library_id
///                 Returned by ::HAPI_LoadAssetLibraryFromFile().
///                 <!-- source ::HAPI_LoadAssetLibraryFromFile -->
///
/// @param[in]      asset_name
///                 Name of the asset to get the parm counts for.
///
/// @param[out]     parm_count
///                 The number of parameters in the asset library.
///
/// @param[out]     int_value_count
///                 The number of int values for parameters in the asset 
///                 library.
///
/// @param[out]     float_value_count
///                 The number of float values for parameters in the asset 
///                 library.
///
/// @param[out]     string_value_count
///                 The number of string values for parameters in the asset 
///                 library.
///
/// @param[out]     choice_value_count
///                 The number of choice values for parameters in the asset 
///                 library.
///
HAPI_DECL HAPI_GetAssetDefinitionParmCounts( const HAPI_Session * session,
                                             HAPI_AssetLibraryId library_id,
                                             const char * asset_name,
                                             int * parm_count,
                                             int * int_value_count,
                                             int * float_value_count,
                                             int * string_value_count,
                                             int * choice_value_count );

/// @brief  Fill an array of ::HAPI_ParmInfo structs with parameter
///         information for the specified asset in the specified asset
///         library.
///
///         This does not create the asset in the session.
///         Use this for faster querying of asset parameters compared to
///         creating the asset node and querying the node's parameters.
///
///         This does require ::HAPI_LoadAssetLibraryFromFile() to be
///         called prior, in order to load the asset library and
///         acquire library_id. ::HAPI_GetAssetDefinitionParmCounts should
///         be called prior to acquire the count for the size of
///         parm_infos_array.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      library_id
///                 Returned by ::HAPI_LoadAssetLibraryFromFile().
///                 <!-- source ::HAPI_LoadAssetLibraryFromFile -->
///
/// @param[in]      asset_name
///                 Name of the asset to get the parm counts for.
///
/// @param[out]     parm_infos_array
///                 Array of ::HAPI_ParmInfo at least the size of
///                 length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most parm_count - 1 acquired from 
///                 ::HAPI_GetAssetInfo.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetInfo::parm_count - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most parm_count - start acquired
///                 from ::HAPI_GetAssetInfo
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_GetAssetInfo::parm_count - start -->
///                 <!-- source ::HAPI_GetAssetInfo::parm_count -->
///
HAPI_DECL HAPI_GetAssetDefinitionParmInfos( const HAPI_Session * session,
                                            HAPI_AssetLibraryId library_id,
                                            const char * asset_name,
                                            HAPI_ParmInfo * parm_infos_array,
                                            int start,
                                            int length );

/// @brief  Fill arrays of parameter int values, float values, string values,
///         and choice values for parameters in the specified asset in the
///         specified asset library.
///
///         This does not create the asset in the session.
///         Use this for faster querying of asset parameters compared to
///         creating the asset node and querying the node's parameters.
///         Note that only default values are retrieved.
///
///         This does require ::HAPI_LoadAssetLibraryFromFile() to be
///         called prior, in order to load the asset library and
///         acquire library_id. ::HAPI_GetAssetDefinitionParmCounts should
///         be called prior to acquire the counts for the sizes of
///         the values arrays.
///
/// @ingroup Assets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      library_id
///                 Returned by ::HAPI_LoadAssetLibraryFromFile().
///                 <!-- source ::HAPI_LoadAssetLibraryFromFile -->
///
/// @param[in]      asset_name
///                 Name of the asset to get the parm counts for.
///
/// @param[out]     int_values_array
///                 Array of ints at least the size of int_length.
///
/// @param[in]      int_start
///                 First index of range for int_values_array. Must be at
///                 least 0 and at most int_value_count - 1 acquired from 
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::int_value_count - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      int_length
///                 Must be at least 0 and at most int_value_count - int_start
///                 acquired from ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::int_value_count - int_start -->
///                 <!-- source ::HAPI_GetAssetDefinitionParmCounts::int_value_count - int_start -->
///
/// @param[out]     float_values_array
///                 Array of floats at least the size of float_length.
///
/// @param[in]      float_start
///                 First index of range for float_values_array. Must be at
///                 least 0 and at most float_value_count - 1 acquired from
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::float_value_count - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      float_length
///                 Must be at least 0 and at most float_value_count -
///                 float_start acquired from
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::float_value_count - float_start -->
///                 <!-- source ::HAPI_GetAssetDefinitionParmCounts::float_value_count - float_start -->
///
/// @param[in]      string_evaluate
///                 Whether or not to evaluate the string expressions.
///                 For example, the string "$F" would evaluate to the
///                 current frame number. So, passing in evaluate = false
///                 would give you back the string "$F" and passing
///                 in evaluate = true would give you back "1" (assuming
///                 the current frame is 1).
///                 <!-- default true -->
///
/// @param[out]     string_values_array
///                 Array of HAPI_StringHandle at least the size of 
///                 string_length.
///
/// @param[in]      string_start
///                 First index of range for string_values_array. Must be at
///                 least 0 and at most string_value_count - 1 acquired from
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::string_value_count - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      string_length
///                 Must be at least 0 and at most string_value_count -
///                 string_start acquired from
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::string_value_count - string_start -->
///                 <!-- source ::HAPI_GetAssetDefinitionParmCounts::string_value_count - string_start -->
///
/// @param[out]     choice_values_array
///                 Array of ::HAPI_ParmChoiceInfo at least the size of
///                 choice_length.
///
/// @param[in]      choice_start
///                 First index of range for choice_values_array. Must be at
///                 least 0 and at most choice_value_count - 1 acquired from 
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::choice_value_count - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      choice_length
///                 Must be at least 0 and at most choice_value_count -
///                 choice_start acquired from
///                 ::HAPI_GetAssetDefinitionParmCounts.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_GetAssetDefinitionParmCounts::choice_value_count - choice_start -->
///                 <!-- source ::HAPI_GetAssetDefinitionParmCounts::choice_value_count - choice_start -->
///
HAPI_DECL HAPI_GetAssetDefinitionParmValues(
    const HAPI_Session * session,
    HAPI_AssetLibraryId library_id,
    const char * asset_name,
    int * int_values_array,
    int int_start,
    int int_length,
    float * float_values_array,
    int float_start,
    int float_length,
    HAPI_Bool string_evaluate,
    HAPI_StringHandle * string_values_array,
    int string_start,
    int string_length,
    HAPI_ParmChoiceInfo * choice_values_array,
    int choice_start,
    int choice_length );

/// @defgroup HipFiles Hip Files
/// Functions for managing hip files

/// @brief  Loads a .hip file into the main Houdini scene.
///
///         @note In threaded mode, this is an _async call_!
///
///         @note This method will load the HIP file into the scene. This means
///         that any registered `hou.hipFile` event callbacks will be triggered
///         with the `hou.hipFileEventType.BeforeMerge` and
///         `hou.hipFileEventType.AfterMerge` events.
///
///         @note This method loads a HIP file, completely overwriting
///         everything that already exists in the scene. Therefore, any HAPI ids
///         (node ids, part ids, etc.) that were obtained before calling this
///         method will be invalidated.
///
/// @ingroup HipFiles
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      file_name
///                 Absolute path to the .hip file to load.
///
/// @param[in]      cook_on_load
///                 Set to true if you wish the nodes to cook as soon
///                 as they are created. Otherwise, you will have to
///                 call ::HAPI_CookNode() explicitly for each after you
///                 call this function.
///                 <!-- default false -->
///
HAPI_DECL HAPI_LoadHIPFile( const HAPI_Session * session,
                            const char * file_name,
                            HAPI_Bool cook_on_load );

/// @brief  Loads a .hip file into the main Houdini scene.
///
///         @note In threaded mode, this is an _async call_!
///
///         @note This method will merge the HIP file into the scene. This means
///         that any registered `hou.hipFile` event callbacks will be triggered
///         with the `hou.hipFileEventType.BeforeMerge` and
///         `hou.hipFileEventType.AfterMerge` events.
///
/// @ingroup HipFiles
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      file_name
///                 Absolute path to the .hip file to load.
///
/// @param[in]      cook_on_load
///                 Set to true if you wish the nodes to cook as soon
///                 as they are created. Otherwise, you will have to
///                 call ::HAPI_CookNode() explicitly for each after you
///                 call this function.
///
/// @param[out]     file_id
///                 This parameter will be set to the HAPI_HIPFileId of the
///                 loaded HIP file. This can be used to lookup nodes that were
///                 created as a result of loading this HIP file.
///
HAPI_DECL HAPI_MergeHIPFile(const HAPI_Session * session,
                            const char * file_name,
                            HAPI_Bool cook_on_load,
                            HAPI_HIPFileId * file_id);

/// @brief  Saves a .hip file of the current Houdini scene.
///
/// @ingroup HipFiles
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      file_path
///                 Absolute path to the .hip file to save to.
///
/// @param[in]      lock_nodes
///                 Specify whether to lock all SOP nodes before saving
///                 the scene file. This way, when you load the scene
///                 file you can see exactly the state of each SOP at
///                 the time it was saved instead of relying on the
///                 re-cook to accurately reproduce the state. It does,
///                 however, take a lot more space and time locking all
///                 nodes like this.
///                 <!-- default false -->
///
HAPI_DECL HAPI_SaveHIPFile( const HAPI_Session * session,
                            const char * file_path,
                            HAPI_Bool lock_nodes );

/// @brief  Gets the number of nodes that were created as a result of loading a
///         .hip file
///
/// @ingroup HipFiles
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      id
///                 The HIP file id.
///
/// @param[out]     count
///                 Pointer to an int where the HIP file node count will be
///                 stored.
HAPI_DECL HAPI_GetHIPFileNodeCount(const HAPI_Session *session,
                                   HAPI_HIPFileId id,
                                   int * count);

/// @brief  Fills an array of ::HAPI_NodeId of nodes that were created as a
///         result of loading the HIP file specified by the ::HAPI_HIPFileId
///
/// @ingroup HipFiles
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      id
///                 The HIP file id.
///
/// @param[out]     node_ids
///                 Array of ::HAPI_NodeId at least the size of length.
///
/// @param[in]      length
///                 The number of ::HAPI_NodeId to be stored. This should be at
///                 least 0 and at most the count provided by
///                 HAPI_GetHIPFileNodeCount
HAPI_DECL HAPI_GetHIPFileNodeIds(const HAPI_Session *session,
                                 HAPI_HIPFileId id,
                                 HAPI_NodeId * node_ids,
                                 int length);

/// @defgroup Nodes
/// Functions for working with nodes

/// @brief  Determine if your instance of the node actually still exists
///         inside the Houdini scene. This is what can be used to
///         determine when the Houdini scene needs to be re-populated
///         using the host application's instances of the nodes.
///         Note that this function will ALWAYS return
///         ::HAPI_RESULT_SUCCESS.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      unique_node_id
///                 The unique node id from
///                 ::HAPI_NodeInfo::uniqueHoudiniNodeId.
///                 <!-- source ::HAPI_NodeInfo::uniqueHoudiniNodeId -->
///
/// @param[out]     answer
///                 Answer to the question.
///
HAPI_DECL HAPI_IsNodeValid( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            int unique_node_id,
                            HAPI_Bool * answer );

/// @brief  Fill an ::HAPI_NodeInfo struct.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     node_info
///                 Return value - contains things like asset id.
///
HAPI_DECL HAPI_GetNodeInfo( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            HAPI_NodeInfo * node_info );

/// @brief  Get the node absolute path in the Houdini node network or a
///         relative path any other node.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      relative_to_node_id
///                 Set this to -1 to get the absolute path of the node_id.
///                 Otherwise, the path will be relative to this node id.
///
/// @param[out]     path
///                 The returned path string, valid until the next call to
///                 this function.
///
HAPI_DECL HAPI_GetNodePath( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            HAPI_NodeId relative_to_node_id,
                            HAPI_StringHandle * path );

/// @brief  Get the root node of a particular network type (ie. OBJ).
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_type
///                 The node network type.
///
/// @param[out]     node_id
///                 The node id of the root node network.
///
HAPI_DECL HAPI_GetManagerNodeId( const HAPI_Session * session,
                                 HAPI_NodeType node_type,
                                 HAPI_NodeId * node_id );

/// @brief  Compose a list of child nodes based on given filters.
///
///         This function will only compose the list of child nodes. It will
///         not return this list. After your call to this function, call
///         HAPI_GetComposedChildNodeList() to get the list of child node ids.
///
///         Note: When looking for all Display SOP nodes using this function,
///         and using recursive mode, the recursion will stop as soon as a
///         display SOP is found within each OBJ geometry network. It is
///         almost never useful to get a list of ALL display SOP nodes
///         recursively as they would all containt the same geometry. Even so,
///         this special case only comes up if the display SOP itself is a
///         subnet.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The node id of the parent node.
///
/// @param[in]      node_type_filter
///                 The node type by which to filter the children.
///
/// @param[in]      node_flags_filter
///                 The node flags by which to filter the children.
///
/// @param[in]      recursive
///                 Whether or not to compose the list recursively.
///
/// @param[out]     count
///                 The number of child nodes composed. Use this as the
///                 argument to ::HAPI_GetComposedChildNodeList().
///
HAPI_DECL HAPI_ComposeChildNodeList( const HAPI_Session * session,
                                     HAPI_NodeId parent_node_id,
                                     HAPI_NodeTypeBits node_type_filter,
                                     HAPI_NodeFlagsBits node_flags_filter,
                                     HAPI_Bool recursive,
                                     int * count );

/// @brief  Get the composed list of child node ids from the previous call
///         to HAPI_ComposeChildNodeList().
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The node id of the parent node.
///
/// @param[out]     child_node_ids_array
///                 The array of ::HAPI_NodeId for the child nodes.
///
/// @param[in]      count
///                 The number of children in the composed list. MUST match
///                 the count returned by HAPI_ComposeChildNodeList().
///                 <!-- source ::HAPI_ComposeChildNodeList -->
///                 <!-- min ::HAPI_ComposeChildNodeList -->
///                 <!-- max ::HAPI_ComposeChildNodeList -->
///
HAPI_DECL HAPI_GetComposedChildNodeList( const HAPI_Session * session,
                                         HAPI_NodeId parent_node_id,
                                         HAPI_NodeId * child_node_ids_array,
                                         int count );

/// @brief  Create a node inside a node network. Nodes created this way
///         will have their ::HAPI_NodeInfo::createdPostAssetLoad set
///         to true.
///
///         @note In threaded mode, this is an _async call_!
///
///         @note This is also when we actually check for valid licenses.
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately with a call
///         result of ::HAPI_RESULT_SUCCESS, even if fed garbage. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the creation (and, optionally, the first cook)
///         of the node has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node network's node id or -1 if the parent
///                 network is the manager (top-level) node. In that case,
///                 the manager must be identified by the table name in the
///                 operator_name.
///                 <!-- min -1 -->
///                 <!-- default -1 -->
///
/// @param[in]      operator_name
///                 The name of the node operator type.
///
///                 If you passed parent_node_id == -1, then the operator_name
///                 has to include the table name (ie. Object/ or Sop/).
///                 This is the common case for when creating asset nodes
///                 from a loaded asset library. In that case, just pass
///                 whatever ::HAPI_GetAvailableAssets() returns.
///
///                 If you have a parent_node_id then you should
///                 include only the namespace, name, and version.
///
///                 For example, lets say you have an Object type asset, in
///                 the "hapi" namespace, of version 2.0, named "foo". If
///                 you pass parent_node_id == -1, then set the operator_name
///                 as "Object/hapi::foo::2.0". Otherwise, if you have a valid
///                 parent_node_id, then just pass operator_name as
///                 "hapi::foo::2.0".
///
/// @param[in]      node_label
///                 (Optional) The label of the newly created node.
///                 <!-- default NULL -->
///
/// @param[in]      cook_on_creation
///                 Set whether the node should cook once created or not.
///                 <!-- default false -->
///
/// @param[out]     new_node_id
///                 The returned node id of the just-created node.
///
HAPI_DECL HAPI_CreateNode( const HAPI_Session * session,
                           HAPI_NodeId parent_node_id,
                           const char * operator_name,
                           const char * node_label,
                           HAPI_Bool cook_on_creation,
                           HAPI_NodeId * new_node_id );

/// @brief  Creates a simple geometry SOP node that can accept geometry input.
///         This will create a dummy OBJ node with a Null SOP inside that
///         you can set the geometry of using the geometry SET APIs.
///         You can then connect this node to any other node as a geometry
///         input.
///
///         Note that when saving the Houdini scene using
///         ::HAPI_SaveHIPFile() the nodes created with this
///         method will be green and will start with the name "input".
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     node_id
///                 Newly created node's id. Use ::HAPI_GetNodeInfo()
///                 to get more information about the node.
///
/// @param[in]      name
///                 Give this input node a name for easy debugging.
///                 The node's parent OBJ node and the Null SOP node will both
///                 get this given name with "input_" prepended.
///                 You can also pass NULL in which case the name will
///                 be "input#" where # is some number.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_CreateInputNode( const HAPI_Session * session,
                                HAPI_NodeId * node_id,
                                const char * name );

/// @brief  Helper for creating specifically creating a curve input geometry SOP.
///         This will create a dummy OBJ node with a Null SOP inside that
///         contains the the HAPI_ATTRIB_INPUT_CURVE_COORDS attribute.
///         It will setup the node as a curve part with no points.
///         In addition to creating the input node, it will also commit and cook
///         the geometry.
///
///         Note that when saving the Houdini scene using
///         ::HAPI_SaveHIPFile() the nodes created with this
///         method will be green and will start with the name "input".
///
/// @ingroup InputCurves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     node_id
///                 Newly created node's id. Use ::HAPI_GetNodeInfo()
///                 to get more information about the node.
///
/// @param[in]      name
///                 Give this input node a name for easy debugging.
///                 The node's parent OBJ node and the Null SOP node will both
///                 get this given name with "input_" prepended.
///                 You can also pass NULL in which case the name will
///                 be "input#" where # is some number.
///                 <!-- default NULL -->
///
HAPI_DECL HAPI_CreateInputCurveNode( const HAPI_Session * session,
                                HAPI_NodeId * node_id,
                                const char * name );


/// @defgroup HeightFields Height Fields
/// Functions for creating and inspecting HAPI session state.

/// @deprecated Use HAPI_CreateHeightFieldInput() instead.
///
/// @brief  Creates the required node hierarchy needed for Heightfield inputs.
///
///         Note that when saving the Houdini scene using
///         ::HAPI_SaveHIPFile() the nodes created with this
///         method will be green and will start with the name "input".
///
///         Note also that this uses center sampling. Use 
///         ::HAPI_CreateHeightFieldInput to specify corner sampling.
///
/// @ingroup HeightFields
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node network's node id or -1 if the parent
///                 network is the manager (top-level) node. In that case,
///                 the manager must be identified by the table name in the
///                 operator_name.
///                 <!-- min -1 -->
///                 <!-- default -1 -->
///
/// @param[in]      name
///                 Give this input node a name for easy debugging.
///                 The node's parent OBJ node and the Null SOP node will both
///                 get this given name with "input_" prepended.
///                 You can also pass NULL in which case the name will
///                 be "input#" where # is some number.
///                 <!-- default NULL -->
///
/// @param[in]      xsize
///                 size of the heightfield in X
///
/// @param[in]      ysize
///                 size of the heightfield in y
///
/// @param[in]      voxelsize
///                 Size of the voxel
///
/// @param[out]     heightfield_node_id
///                 Newly created node id for the Heightfield node.
///                 Use ::HAPI_GetNodeInfo() to get more information about
///                 the node.
///
/// @param[out]     height_node_id
///                 Newly created node id for the height volume.
///                 Use ::HAPI_GetNodeInfo() to get more information about the node.
///
/// @param[out]     mask_node_id
///                 Newly created node id for the mask volume.
///                 Use ::HAPI_GetNodeInfo() to get more information about the
///                 node.
///
/// @param[out]     merge_node_id
///                 Newly created merge node id. 
///                 The merge node can be used to connect additional input masks.
///                 Use ::HAPI_GetNodeInfo() to get more information about the node.
///
HAPI_DECL_DEPRECATED( 3.3.5, 18.0.162 )
HAPI_CreateHeightfieldInputNode( const HAPI_Session * session,
                                 HAPI_NodeId parent_node_id,
                                 const char * name,
                                 int xsize,
                                 int ysize,
                                 float voxelsize,
                                 HAPI_NodeId * heightfield_node_id,
                                 HAPI_NodeId * height_node_id,
                                 HAPI_NodeId * mask_node_id,
                                 HAPI_NodeId * merge_node_id );

/// @brief  Creates the required node hierarchy needed for heightfield inputs.
///
///         Note that when saving the Houdini scene using
///         ::HAPI_SaveHIPFile() the nodes created with this
///         method will be green and will start with the name "input".
///
/// @ingroup HeightFields
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node network's node id or -1 if the parent
///                 network is the manager (top-level) node. In that case,
///                 the manager must be identified by the table name in the
///                 operator_name.
///                 <!-- min -1 -->
///                 <!-- default -1 -->
///
/// @param[in]      name
///                 Give this input node a name for easy debugging.
///                 The node's parent OBJ node and the Null SOP node will both
///                 get this given name with "input_" prepended.
///                 You can also pass NULL in which case the name will
///                 be "input#" where # is some number.
///                 <!-- default NULL -->
///
/// @param[in]      xsize
///                 size of the heightfield in X
///
/// @param[in]      ysize
///                 size of the heightfield in y
///
/// @param[in]      voxelsize
///                 Size of the voxel
///
/// @param[in]      sampling
///                 Type of sampling which should be either center or corner.
///
/// @param[out]     heightfield_node_id
///                 Newly created node id for the heightfield node.
///                 Use ::HAPI_GetNodeInfo() to get more information about
///                 the node.
///
/// @param[out]     height_node_id
///                 Newly created node id for the height volume.
///                 Use ::HAPI_GetNodeInfo() to get more information about the node.
///
/// @param[out]     mask_node_id
///                 Newly created node id for the mask volume.
///                 Use ::HAPI_GetNodeInfo() to get more information about the
///                 node.
///
/// @param[out]     merge_node_id
///                 Newly created merge node id. 
///                 The merge node can be used to connect additional input masks.
///                 Use ::HAPI_GetNodeInfo() to get more information about the node.
///
HAPI_DECL HAPI_CreateHeightFieldInput( const HAPI_Session * session,
                                       HAPI_NodeId parent_node_id,
                                       const char * name,
                                       int xsize,
                                       int ysize,
                                       float voxelsize,
                                       HAPI_HeightFieldSampling sampling,
                                       HAPI_NodeId * heightfield_node_id,
                                       HAPI_NodeId * height_node_id,
                                       HAPI_NodeId * mask_node_id,
                                       HAPI_NodeId * merge_node_id );

/// @brief  Creates a volume input node that can be used with Heightfields
///
///         Note that when saving the Houdini scene using
///         ::HAPI_SaveHIPFile() the nodes created with this
///         method will be green and will start with the name "input".
///
/// @ingroup HeightFields
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node network's node id or -1 if the parent
///                 network is the manager (top-level) node. In that case,
///                 the manager must be identified by the table name in the
///                 operator_name.
///                 <!-- min -1 -->
///                 <!-- default -1 -->
///
/// @param[out]     new_node_id
///                 Newly created node id for the volume.
///                 Use ::HAPI_GetNodeInfo() to get more information about the
///                 node.
///
/// @param[in]      name
///                 The name of the volume to create.
///                 You can also pass NULL in which case the name will
///                 be "input#" where # is some number.
///                 <!-- default NULL -->
///
/// @param[in]      xsize
///                 size of the heightfield in X
///
/// @param[in]      ysize
///                 size of the heightfield in y
///
/// @param[in]      voxelsize
///                 Size of the voxel
///
HAPI_DECL HAPI_CreateHeightfieldInputVolumeNode(    const HAPI_Session * session,
                                                    HAPI_NodeId parent_node_id,
                                                    HAPI_NodeId * new_node_id,
                                                    const char * name,
                                                    int xsize,
                                                    int ysize,
                                                    float voxelsize );

/// @brief  Initiate a cook on this node. Note that this may trigger
///         cooks on other nodes if they are connected.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      cook_options
///                 The cook options. Pass in NULL to use the global
///                 cook options that you specified when calling
///                 ::HAPI_Initialize().
///
HAPI_DECL HAPI_CookNode( const HAPI_Session * session,
                         HAPI_NodeId node_id,
                         const HAPI_CookOptions * cook_options );

/// @brief  Delete a node from a node network. Only nodes with their
///         ::HAPI_NodeInfo::createdPostAssetLoad set to true can be
///         deleted this way.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node to delete.
///
HAPI_DECL HAPI_DeleteNode( const HAPI_Session * session,
                           HAPI_NodeId node_id );

/// @brief  Rename a node that you created. Only nodes with their
///         ::HAPI_NodeInfo::createdPostAssetLoad set to true can be
///         renamed this way.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node to rename.
///
/// @param[in]      new_name
///                 The new node name.
///
HAPI_DECL HAPI_RenameNode( const HAPI_Session * session,
                           HAPI_NodeId node_id,
                           const char * new_name );

/// @brief  Connect two nodes together.
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @ingroup Nodes
///
/// @param[in]      node_id
///                 The node whom's input to connect to.
///
/// @param[in]      input_index
///                 The input index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::inputCount - 1.
///                 <!-- min 0 -->
///
/// @param[in]      node_id_to_connect
///                 The node to connect to node_id's input.
///
/// @param[in]	    output_index
///                 The output index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::outputCount - 1.
///                 <!-- min 0 -->
///
HAPI_DECL HAPI_ConnectNodeInput( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 int input_index,
                                 HAPI_NodeId node_id_to_connect,
                                 int output_index );

/// @brief  Disconnect a node input.
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @ingroup Nodes
///
/// @param[in]      node_id
///                 The node whom's input to disconnect.
///
/// @param[in]      input_index
///                 The input index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::inputCount - 1.
///                 <!-- min 0 -->
///
HAPI_DECL HAPI_DisconnectNodeInput( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    int input_index );

/// @brief  Query which node is connected to another node's input.
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @ingroup Nodes
///
/// @param[in]      node_to_query
///                 The node to query.
///
/// @param[in]      input_index
///                 The input index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::inputCount - 1.
///                 <!-- min 0 -->
///
/// @param[out]     connected_node_id
///                 The node id of the connected node to this input. If
///                 nothing is connected then -1 will be returned.
///
HAPI_DECL HAPI_QueryNodeInput( const HAPI_Session * session,
                               HAPI_NodeId node_to_query,
                               int input_index,
                               HAPI_NodeId * connected_node_id );

/// @brief  Get the name of an node's input. This function will return
///         a string handle for the name which will be valid (persist)
///         until the next call to this function.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      input_idx
///                 The input index. Should be between 0 and the
///                 node_to_query's ::HAPI_NodeInfo::inputCount - 1.
///                 <!-- min 0 -->
///
/// @param[out]     name
///                 Input name string handle return value - valid until
///                 the next call to this function.
///
HAPI_DECL HAPI_GetNodeInputName( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 int input_idx,
                                 HAPI_StringHandle * name );

/// @brief  Disconnect all of the node's output connections at the output index.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node whom's outputs to disconnect.
///
/// @param[in]      output_index
///                 The output index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::outputCount.
///                 <!-- min 0 -->
///
HAPI_DECL HAPI_DisconnectNodeOutputsAt( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        int output_index );

/// @brief  Get the number of nodes currently connected to the given node at 
///	    the output index.
///
/// @ingroup Nodes
///
///         Use the @c count returned by this function to get the
///         ::HAPI_NodeId of connected nodes using
///         ::HAPI_QueryNodeOutputConnectedNodes().
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      output_idx
///                 The output index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::outputCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::outputCount - 1 -->
///
/// @param[in]      into_subnets
///                 Whether to search by diving into subnets.
///                 <!-- default true -->
///
/// @param[in]      through_dots
///                 Whether to search through dots.
///                 <!-- default true -->
///
/// @param[out]     connected_count
///                 The number of nodes currently connected to this node at 
///                 given output index. Use this count with a call to
///                 ::HAPI_QueryNodeOutputConnectedNodes() to get list of 
///                 connected nodes.
///
HAPI_DECL HAPI_QueryNodeOutputConnectedCount( const HAPI_Session * session,
                                              HAPI_NodeId node_id, 
                                              int output_idx,
                                              HAPI_Bool into_subnets, 
                                              HAPI_Bool through_dots,
                                              int * connected_count );

/// @brief  Get the ids of nodes currently connected to the given node
///	    at the output index.
///
///         Use the @c connected_count returned by 
///	    ::HAPI_QueryNodeOutputConnectedCount().
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      output_idx
///                 The output index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::outputCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::outputCount - 1 -->
///
/// @param[in]      into_subnets
///                 Whether to search by diving into subnets.
///                 <!-- default true -->
///
/// @param[in]      through_dots
///                 Whether to search through dots.
///                 <!-- default true -->
///
/// @param[out]     connected_node_ids_array
///		            Array of ::HAPI_NodeId at least the size of @c length.
///
/// @param[in]      start
///                 At least @c 0 and at most @c connected_count returned by
///                 ::HAPI_QueryNodeOutputConnectedCount().
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_QueryNodeOutputConnectedCount -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Given @c connected_count returned by
///                 ::HAPI_QueryNodeOutputConnectedCount(), @c length should 
///                 be at least @c 1 and at most <tt>connected_count - start</tt>.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_QueryNodeOutputConnectedCount - start -->
///                 <!-- source ::HAPI_QueryNodeOutputConnectedCount - start -->
///
HAPI_DECL HAPI_QueryNodeOutputConnectedNodes( const HAPI_Session * session,
                                             HAPI_NodeId node_id, 
                                             int output_idx, 
                                             HAPI_Bool into_subnets, 
                                             HAPI_Bool through_dots,
                                             HAPI_NodeId * connected_node_ids_array,
                                             int start, int length );

/// @brief  Get the name of an node's output. This function will return
///         a string handle for the name which will be valid (persist)
///         until the next call to this function.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      output_idx
///                 The output index. Should be between 0 and the
///                 to_node's ::HAPI_NodeInfo::outputCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::outputCount - 1 -->
///
/// @param[out]     name
///                 Output name string handle return value - valid until
///                 the next call to this function.
///
HAPI_DECL HAPI_GetNodeOutputName( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  int output_idx,
                                  HAPI_StringHandle * name );

/// @brief  Get the id of the node with the specified path.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 If @c path does not start with "/", search for the path
///                 relative to this node. Provide -1 if @c path is an absolute
///                 path.
///
/// @param[in]      path
///                 The path of the node. If the path does not start with "/",
///                 it is treated as a relative path from the node specified in
///                 @c parent_node_id.
///
/// @param[out]     node_id
///                 The id of the found node.
///
HAPI_DECL HAPI_GetNodeFromPath( const HAPI_Session * session,
                                const HAPI_NodeId parent_node_id,
                                const char * path,
                                HAPI_NodeId * node_id );

/// @brief Gets the node id of an output node in a SOP network.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id of a SOP node with at least one output node. The
///                 total number of node outputs can be found from the node's
///                 ::HAPI_NodeInfo::outputCount
///
/// @param[in]      output
///                 The output index. Should be between 0 and the node's
///                 ::HAPI_NodeInfo::outputCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::outputCount - 1 --> 
///
/// @param[out]     output_node_id
///                 Pointer to a HAPI_NodeId where the node id of the output
///                 node will be stored.
HAPI_DECL HAPI_GetOutputNodeId( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                int output,
                                HAPI_NodeId * output_node_id );

/// @defgroup Parms Parms
/// Functions for wroking with Node parameters (parms)

/// @brief  Fill an array of ::HAPI_ParmInfo structs with parameter
///         information from the asset instance node.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     parm_infos_array
///                 Array of ::HAPI_ParmInfo at least the size of
///                 length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::parmCount - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmCount - start.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_NodeInfo::parmCount - start -->
///                 <!-- source ::HAPI_NodeInfo::parmCount - start -->
///
HAPI_DECL HAPI_GetParameters( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_ParmInfo * parm_infos_array,
                              int start, int length );

/// @brief  Get the parm info of a parameter by parm id.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 The parm id.
///
/// @param[out]     parm_info
///                 The returned parm info.
///
HAPI_DECL HAPI_GetParmInfo( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            HAPI_ParmId parm_id,
                            HAPI_ParmInfo * parm_info );

/// @brief  All parameter APIs require a ::HAPI_ParmId but if you know the
///         parameter you wish to operate on by name than you can use
///         this function to get its ::HAPI_ParmId. If the parameter with
///         the given name is not found the parameter id returned
///         will be -1.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[out]     parm_id
///                 The return value. The parameter's ::HAPI_ParmId. If
///                 the parameter with the given name is not found the
///                 parameter id returned will be -1.
///
HAPI_DECL HAPI_GetParmIdFromName( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  const char * parm_name,
                                  HAPI_ParmId * parm_id );

/// @brief  Get the parm info of a parameter by name.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[out]     parm_info
///                 The returned parm info.
///
HAPI_DECL HAPI_GetParmInfoFromName( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    const char * parm_name,
                                    HAPI_ParmInfo * parm_info );

/// @brief  Get the tag name on a parameter given an index.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 The parm id.
///
/// @param[in]      tag_index
///                 The tag index, which should be between 0 and
///                 ::HAPI_ParmInfo::tagCount - 1.
///                 @note These indices are invalidated whenever tags are added
///                 to parameters. Do not store these or expect them to be the
///                 same if the scene is modified.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_ParmInfo::tagCount - 1 -->
///
/// @param[out]     tag_name
///                 The returned tag name. This string handle will be valid
///                 until another call to ::HAPI_GetParmTagName().
///
HAPI_DECL HAPI_GetParmTagName( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               HAPI_ParmId parm_id,
                               int tag_index,
                               HAPI_StringHandle * tag_name );

/// @brief  Get the tag value on a parameter given the tag name.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 The parm id.
///
/// @param[in]      tag_name
///                 The tag name, either known or returned by
///                 ::HAPI_GetParmTagName().
///
/// @param[out]     tag_value
///                 The returned tag value. This string handle will be valid
///                 until another call to ::HAPI_GetParmTagValue().
///
HAPI_DECL HAPI_GetParmTagValue( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                HAPI_ParmId parm_id,
                                const char * tag_name,
                                HAPI_StringHandle * tag_value );

/// @brief  See if a parameter has a specific tag.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 The parm id.
///
/// @param[in]      tag_name
///                 The tag name to look for.
///
/// @param[out]     has_tag
///                 True if the tag exists on the parameter, false otherwise.
///
HAPI_DECL HAPI_ParmHasTag( const HAPI_Session * session,
                           HAPI_NodeId node_id,
                           HAPI_ParmId parm_id,
                           const char * tag_name,
                           HAPI_Bool * has_tag );

/// @brief  See if a parameter has an expression
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 The parm index.
///
/// @param[out]     has_expression
///                 True if an expression exists on the parameter, false otherwise.
///
HAPI_DECL HAPI_ParmHasExpression( const HAPI_Session * session,
                           HAPI_NodeId node_id,
			   const char * parm_name,
                           int index,
                           HAPI_Bool * has_expression );

/// @brief  Get the first parm with a specific, ideally unique, tag on it.
///         This is particularly useful for getting the ogl parameters on a
///         material node.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      tag_name
///                 The tag name to look for.
///
/// @param[out]     parm_id
///                 The returned parm id. This will be -1 if no parm was found
///                 with this tag.
///
HAPI_DECL HAPI_GetParmWithTag( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               const char * tag_name,
                               HAPI_ParmId * parm_id );

/// @brief  Get single integer or float parm expression by name
///         or Null string if no expression is present
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
/// @param[out]     value
///                 The returned string value.
///
HAPI_DECL HAPI_GetParmExpression( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                const char * parm_name,
                                int index,
                                HAPI_StringHandle * value );

/// @brief  Revert single parm by name to default
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
HAPI_DECL HAPI_RevertParmToDefault( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                const char * parm_name,
                                int index );

/// @brief  Revert all instances of the parm by name to defaults
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
HAPI_DECL HAPI_RevertParmToDefaults( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                const char * parm_name );

/// @brief  Set (push) an expression string. We can only set a single value at
///         a time because we want to avoid fixed size string buffers.
///
///         @note Regardless of the value, when calling this function
///         on a parameter, if that parameter has a callback function
///         attached to it, that callback function will be called. For
///         example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      value
///                 The expression string.
///
/// @param[in]      parm_id
///                 Parameter id of the parameter being updated.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
HAPI_DECL HAPI_SetParmExpression( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   const char * value,
                                   HAPI_ParmId parm_id, int index );

/// @brief  Remove the expression string, leaving the value of the
///         parm at the current value of the expression
///
///         @note Regardless of the value, when calling this function
///         on a parameter, if that parameter has a callback function
///         attached to it, that callback function will be called. For
///         example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 Parameter id of the parameter being updated.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
HAPI_DECL HAPI_RemoveParmExpression( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_ParmId parm_id, int index );

/// @brief  Get single parm int value by name.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
/// @param[out]     value
///                 The returned int value.
///
HAPI_DECL HAPI_GetParmIntValue( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                const char * parm_name,
                                int index,
                                int * value );

/// @brief  Fill an array of parameter int values. This is more efficient
///         than calling ::HAPI_GetParmIntValue() individually for each
///         parameter value.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     values_array
///                 Array of ints at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmIntValueCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::parmIntValueCount - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmIntValueCount - start.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_NodeInfo::parmIntValueCount - start -->
///                 <!-- source ::HAPI_NodeInfo::parmIntValueCount - start -->
///
HAPI_DECL HAPI_GetParmIntValues( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 int * values_array,
                                 int start, int length );

/// @brief  Get single parm float value by name.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
/// @param[out]     value
///                 The returned float value.
///
HAPI_DECL HAPI_GetParmFloatValue( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  const char * parm_name,
                                  int index,
                                  float * value );

/// @brief  Fill an array of parameter float values. This is more efficient
///         than calling ::HAPI_GetParmFloatValue() individually for each
///         parameter value.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     values_array
///                 Array of floats at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmFloatValueCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::parmFloatValueCount - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmFloatValueCount - start.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_NodeInfo::parmFloatValueCount - start -->
///                 <!-- source ::HAPI_NodeInfo::parmFloatValueCount - start -->
///
HAPI_DECL HAPI_GetParmFloatValues( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   float * values_array,
                                   int start, int length );

/// @brief  Get single parm string value by name.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The name of the parameter.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
/// @param[in]      evaluate
///                 Whether or not to evaluate the string expression.
///                 For example, the string "$F" would evaluate to the
///                 current frame number. So, passing in evaluate = false
///                 would give you back the string "$F" and passing
///                 in evaluate = true would give you back "1" (assuming
///                 the current frame is 1).
///                 <!-- default true -->
///
/// @param[out]     value
///                 The returned string value.
///
HAPI_DECL HAPI_GetParmStringValue( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   const char * parm_name,
                                   int index,
                                   HAPI_Bool evaluate,
                                   HAPI_StringHandle * value );

/// @brief  Fill an array of parameter string handles. These handles must
///         be used in conjunction with ::HAPI_GetString() to get the
///         actual string values. This is more efficient than calling
///         ::HAPI_GetParmStringValue() individually for each
///         parameter value.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      evaluate
///                 Whether or not to evaluate the string expression.
///                 For example, the string "$F" would evaluate to the
///                 current frame number. So, passing in evaluate = false
///                 would give you back the string "$F" and passing
///                 in evaluate = true would give you back "1" (assuming
///                 the current frame is 1).
///
/// @param[out]     values_array
///                 Array of integers at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmStringValueCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::parmStringValueCount - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmStringValueCount - start.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_NodeInfo::parmStringValueCount - start -->
///                 <!-- source ::HAPI_NodeInfo::parmStringValueCount - start -->
///
HAPI_DECL HAPI_GetParmStringValues( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    HAPI_Bool evaluate,
                                    HAPI_StringHandle * values_array,
                                    int start, int length );

/// @brief  Get a single node id parm value of an Op Path parameter. This is
///         how you see which node is connected as an input for the current
///         node (via parameter).
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The name of the parameter.
///
/// @param[out]     value
///                 The node id of the node being pointed to by the parm.
///                 If there is no node found, -1 will be returned.
///
HAPI_DECL HAPI_GetParmNodeValue( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 const char * parm_name,
                                 HAPI_NodeId * value );

/// @brief  Extract a file specified by path on a parameter. This will copy
///         the file to the destination directory from wherever it might be,
///         inlcuding inside the asset definition or online.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The name of the parameter.
///
/// @param[in]      destination_directory
///                 The destination directory to copy the file to.
///
/// @param[in]      destination_file_name
///                 The destination file name.
///
HAPI_DECL HAPI_GetParmFile( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            const char * parm_name,
                            const char * destination_directory,
                            const char * destination_file_name );

/// @brief  Fill an array of ::HAPI_ParmChoiceInfo structs with parameter
///         choice list information from the asset instance node.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     parm_choices_array
///                 Array of ::HAPI_ParmChoiceInfo exactly the size of
///                 length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmChoiceCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::parmChoiceCount - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmChoiceCount - start.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_NodeInfo::parmChoiceCount - start -->
///                 <!-- source ::HAPI_NodeInfo::parmChoiceCount - start -->
///
HAPI_DECL HAPI_GetParmChoiceLists( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_ParmChoiceInfo * parm_choices_array,
                                   int start, int length );

/// @brief  Set single parm int value by name.
///
///         @note Regardless of the value, when calling this function
///         on a parameter, if that parameter has a callback function
///         attached to it, that callback function will be called. For
///         example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
/// @param[in]      value
///                 The int value.
///
HAPI_DECL HAPI_SetParmIntValue( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                const char * parm_name,
                                int index,
                                int value );

/// @brief  Set (push) an array of parameter int values.
///
///         @note Regardless of the values, when calling this function
///         on a set of parameters, if any parameter has a callback
///         function attached to it, that callback function will be called.
///         For example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      values_array
///                 Array of integers at least the size of length.
///                 <!-- min length -->
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmIntValueCount - 1.
///                 <!-- min 0 -->
///                 <!-- max ::HAPI_NodeInfo::parmIntValueCount - 1 -->
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmIntValueCount - start.
///                 <!-- min 1 -->
///                 <!-- max ::HAPI_NodeInfo::parmIntValueCount - start -->
///                 <!-- source ::HAPI_NodeInfo::parmIntValueCount - start -->
///
HAPI_DECL HAPI_SetParmIntValues( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 const int * values_array,
                                 int start, int length );

/// @brief  Set single parm float value by name.
///
///         @note Regardless of the value, when calling this function
///         on a parameter, if that parameter has a callback function
///         attached to it, that callback function will be called. For
///         example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The parm name.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
/// @param[in]      value
///                 The float value.
///
HAPI_DECL HAPI_SetParmFloatValue( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  const char * parm_name,
                                  int index,
                                  float value );

/// @brief  Set (push) an array of parameter float values.
///
///         @note Regardless of the values, when calling this function
///         on a set of parameters, if any parameter has a callback
///         function attached to it, that callback function will be called.
///         For example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      values_array
///                 Array of floats at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_NodeInfo::parmFloatValueCount - 1.
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_NodeInfo::parmFloatValueCount - start.
///
HAPI_DECL HAPI_SetParmFloatValues( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   const float * values_array,
                                   int start, int length );

/// @brief  Set (push) a string value. We can only set a single value at
///         a time because we want to avoid fixed size string buffers.
///
///         @note Regardless of the value, when calling this function
///         on a parameter, if that parameter has a callback function
///         attached to it, that callback function will be called. For
///         example, if the parameter is a button the button will be
///         pressed.
///
///         @note In threaded mode, this is an _async call_!
///
///         This API will invoke the cooking thread if threading is
///         enabled. This means it will return immediately. Use
///         the status and cooking count APIs under DIAGNOSTICS to get
///         a sense of the progress. All other API calls will block
///         until the cook operation has finished.
///
///         Also note that the cook result won't be of type
///         ::HAPI_STATUS_CALL_RESULT like all calls (including this one).
///         Whenever the threading cook is done it will fill the
///         @a cook result which is queried using
///         ::HAPI_STATUS_COOK_RESULT.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      value
///                 The string value.
///
/// @param[in]      parm_id
///                 Parameter id of the parameter being updated.
///
/// @param[in]      index
///                 Index within the parameter's values tuple.
///
HAPI_DECL HAPI_SetParmStringValue( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   const char * value,
                                   HAPI_ParmId parm_id, int index );

/// @brief  Set a node id parm value of an Op Path parameter. For example,
///         This is how you connect the geometry output of an asset to the
///         geometry input of another asset - whether the input is a parameter
///         or a node input (the top of the node). Node inputs get converted
///         top parameters in HAPI.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_name
///                 The name of the parameter.
///
/// @param[in]      value
///                 The node id of the node being connected. Pass -1 to
///                 disconnect.
///
HAPI_DECL HAPI_SetParmNodeValue( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 const char * parm_name,
                                 HAPI_NodeId value );

/// @brief Insert an instance of a multiparm before instance_position.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 A parm id given by a ::HAPI_ParmInfo struct that
///                 has type ::HAPI_PARMTYPE_MULTIPARMLIST.
///
/// @param[in]      instance_position
///                 The new instance will be inserted at this position
///                 index. Do note the multiparms can start at position
///                 1 or 0. Use ::HAPI_ParmInfo::instanceStartOffset to
///                 distinguish.
///
HAPI_DECL HAPI_InsertMultiparmInstance( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_ParmId parm_id,
                                        int instance_position );

/// @brief Remove the instance of a multiparm given by instance_position.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      parm_id
///                 A parm id given by a ::HAPI_ParmInfo struct that
///                 has type ::HAPI_PARMTYPE_MULTIPARMLIST.
///
/// @param[in]      instance_position
///                 The instance at instance_position will removed.
///
HAPI_DECL HAPI_RemoveMultiparmInstance( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_ParmId parm_id,
                                        int instance_position );

// HANDLES ------------------------------------------------------------------

/// @brief  Fill an array of ::HAPI_HandleInfo structs with information
///         about every exposed user manipulation handle on the node.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     handle_infos_array
///                 Array of ::HAPI_HandleInfo at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AssetInfo::handleCount - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_AssetInfo::handleCount - start.
///                 <!-- source ::HAPI_AssetInfo::handleCount - start -->
///
HAPI_DECL HAPI_GetHandleInfo( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_HandleInfo * handle_infos_array,
                              int start, int length );

/// @brief  Fill an array of ::HAPI_HandleBindingInfo structs with information
///         about the binding of a particular handle on the given node.
///
/// @ingroup Parms
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      handle_index
///                 The index of the handle, from 0 to handleCount - 1
///                 from the call to ::HAPI_GetAssetInfo().
///
/// @param[out]     handle_binding_infos_array
///                 Array of ::HAPI_HandleBindingInfo at least the size
///                 of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_HandleInfo::bindingsCount - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 1 and at most
///                 ::HAPI_HandleInfo::bindingsCount - start.
///                 <!-- source ::HAPI_AssetInfo::bindingsCount - start -->
///
HAPI_DECL HAPI_GetHandleBindingInfo(
                        const HAPI_Session * session,
                        HAPI_NodeId node_id,
                        int handle_index,
                        HAPI_HandleBindingInfo * handle_binding_infos_array,
                        int start, int length );

/// @defgroup Presets Presets
/// Functions for working with Node presets

/// @brief  Generate a preset blob of the current state of all the
///         parameter values, cache it, and return its size in bytes.
///
/// @ingroup Presets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The exposed node id.
///
/// @param[in]      preset_type
///                 The preset type.
///
/// @param[in]      preset_name
///                 Optional. This is only used if the @p preset_type is
///                 ::HAPI_PRESETTYPE_IDX. If NULL is given, the preset
///                 name will be the same as the name of the node with
///                 the given @p node_id.
///
/// @param[out]     buffer_length
///                 Size of the buffer.
///
HAPI_DECL HAPI_GetPresetBufLength( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PresetType preset_type,
                                   const char * preset_name,
                                   int * buffer_length );

/// @brief  Generates a preset for the given asset.
///
/// @ingroup Presets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The exposed node id.
///
/// @param[out]     buffer
///                 Buffer to hold the preset data.
///
/// @param[in]      buffer_length
///                 Size of the buffer. Should be the same as the length
///                 returned by ::HAPI_GetPresetBufLength().
///
HAPI_DECL HAPI_GetPreset( const HAPI_Session * session,
                          HAPI_NodeId node_id,
                          char * buffer,
                          int buffer_length );

/// @brief  Sets a particular asset to a given preset.
///
/// @ingroup Presets
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The exposed node id.
///
/// @param[in]      preset_type
///                 The preset type.
///
/// @param[in]      preset_name
///                 Optional. This is only used if the @p preset_type is
///                 ::HAPI_PRESETTYPE_IDX. If NULL is give, the first
///                 preset in the IDX file will be chosen.
///                 <!-- default NULL -->
///
/// @param[in]      buffer
///                 Buffer to hold the preset data.
///
/// @param[in]      buffer_length
///                 Size of the buffer.
///
HAPI_DECL HAPI_SetPreset( const HAPI_Session * session,
                          HAPI_NodeId node_id,
                          HAPI_PresetType preset_type,
                          const char * preset_name,
                          const char * buffer,
                          int buffer_length );

/// @defgroup Objects
/// Functions for working with OBJ Nodes

/// @brief  Get the object info on an OBJ node.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     object_info
///                 The output ::HAPI_ObjectInfo.
///
HAPI_DECL HAPI_GetObjectInfo( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_ObjectInfo * object_info );

/// @brief  Get the tranform of an OBJ node.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The object node id.
///
/// @param[in]      relative_to_node_id
///                 The object node id for the object to which the returned
///                 transform will be relative to. Pass -1 or the node_id
///                 to just get the object's local transform.
///
/// @param[in]      rst_order
///                 The order of application of translation, rotation and
///                 scale.
///
/// @param[out]     transform
///                 The output ::HAPI_Transform transform.
///
HAPI_DECL HAPI_GetObjectTransform( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_NodeId relative_to_node_id,
                                   HAPI_RSTOrder rst_order,
                                   HAPI_Transform * transform );

/// @brief  Compose a list of child object nodes given a parent node id.
///
///         Use the @c object_count returned by this function to get the
///         ::HAPI_ObjectInfo structs for each child object using
///         ::HAPI_GetComposedObjectList().
///
///         Note, if not using the @c categories arg, this is equivalent to:
///         @code
///         HAPI_ComposeChildNodeList(
///             session, parent_node_id,
///             HAPI_NODETYPE_OBJ,
///             HAPI_NODEFLAGS_OBJ_GEOMETRY,
///             true, &object_count );
///         @endcode
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node id.
///
/// @param[in]      categories
///                 (Optional) Lets you filter object nodes by their render
///                 categories. This is a standard OBJ parameter, usually
///                 under the Render > Shading tab. If an OBJ node does not
///                 have this parameter, one can always add it as a spare.
///
///                 The value of this string argument should be NULL if not
///                 used or a space-separated list of category names.
///                 Multiple category names will be treated as an AND op.
///                 <!-- default NULL -->
///
/// @param[out]     object_count
///                 The number of object nodes currently under the parent.
///                 Use this count with a call to
///                 ::HAPI_GetComposedObjectList() to get the object infos.
///
HAPI_DECL HAPI_ComposeObjectList( const HAPI_Session * session,
                                  HAPI_NodeId parent_node_id,
                                  const char * categories,
                                  int * object_count );

/// @brief  Fill an array of ::HAPI_ObjectInfo structs.
///
///         This is best used with ::HAPI_ComposeObjectList() with.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node id.
///
/// @param[out]     object_infos_array
///                 Array of ::HAPI_ObjectInfo at least the size of
///                 @c length.
///
/// @param[in]      start
///                 At least @c 0 and at most @c object_count returned by
///                 ::HAPI_ComposeObjectList().
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Given @c object_count returned by
///                 ::HAPI_ComposeObjectList(), @c length should be at least
///                 @c 0 and at most <tt>object_count - start</tt>.
///                 <!-- source ::HAPI_ComposeObjectList - start -->
///
HAPI_DECL HAPI_GetComposedObjectList( const HAPI_Session * session,
                                      HAPI_NodeId parent_node_id,
                                      HAPI_ObjectInfo * object_infos_array,
                                      int start, int length );

/// @brief  Fill an array of ::HAPI_Transform structs.
///
///         This is best used with ::HAPI_ComposeObjectList() with.
///
///         Note that these transforms will be relative to the
///         @c parent_node_id originally given to ::HAPI_ComposeObjectList()
///         and expected to be the same with this call. If @c parent_node_id
///         is not an OBJ node, the transforms will be given as they are on
///         the object node itself.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      parent_node_id
///                 The parent node id. The object transforms will be
///                 relative to this node unless this node is not an OBJ.
///
/// @param[in]      rst_order
///                 The order of application of translation, rotation and
///                 scale.
///
/// @param[out]     transform_array
///                 Array of ::HAPI_Transform at least the size of
///                 length.
///
/// @param[in]      start
///                 At least @c 0 and at most @c object_count returned by
///                 ::HAPI_ComposeObjectList().
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Given @c object_count returned by
///                 ::HAPI_ComposeObjectList(), @c length should be at least
///                 @c 0 and at most <tt>object_count - start</tt>.
///                 <!-- source ::HAPI_ComposeObjectList - start -->
///
HAPI_DECL HAPI_GetComposedObjectTransforms( const HAPI_Session * session,
                                            HAPI_NodeId parent_node_id,
                                            HAPI_RSTOrder rst_order,
                                            HAPI_Transform * transform_array,
                                            int start, int length );

/// @brief  Get the node ids for the objects being instanced by an
///         Instance OBJ node.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      object_node_id
///                 The object node id.
///
/// @param[out]     instanced_node_id_array
///                 Array of ::HAPI_NodeId at least the size of length.
///
/// @param[in]      start
///                 At least @c 0 and at most @c object_count returned by
///                 ::HAPI_ComposeObjectList().
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Given @c object_count returned by
///                 ::HAPI_ComposeObjectList(), @c length should be at least
///                 @c 0 and at most <tt>object_count - start</tt>.
///                 <!-- source ::HAPI_ComposeObjectList - start -->
///
HAPI_DECL HAPI_GetInstancedObjectIds( const HAPI_Session * session,
                                      HAPI_NodeId object_node_id,
                                      HAPI_NodeId * instanced_node_id_array,
                                      int start, int length );

/// @deprecated Use HAPI_GetInstanceTransformsOnPart() instead (using Part 0 for
///             previous behaviour).
///
/// @brief  Fill an array of ::HAPI_Transform structs with the transforms
///         of each instance of this instancer object.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      object_node_id
///                 The object node id.
///
/// @param[in]      rst_order
///                 The order of application of translation, rotation and
///                 scale.
///
/// @param[out]     transforms_array
///                 Array of ::HAPI_Transform at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_PartInfo::pointCount - 1. This is the 0th
///                 part of the display geo of the instancer object node.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_PartInfo::pointCount - @p start. This is the 0th
///                 part of the display geo of the instancer object node.
///                 <!-- source ::HAPI_PartInfo::pointCount - start -->
///
HAPI_DECL_DEPRECATED( 3.2.42, 18.0.150 )
HAPI_GetInstanceTransforms( const HAPI_Session * session,
                            HAPI_NodeId object_node_id,
                            HAPI_RSTOrder rst_order,
                            HAPI_Transform * transforms_array,
                            int start, int length );

/// @brief  Fill an array of ::HAPI_Transform structs with the transforms
///         of each instance of this instancer object for a given part.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The object node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      rst_order
///                 The order of application of translation, rotation and
///                 scale.
///
/// @param[out]     transforms_array
///                 Array of ::HAPI_Transform at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_PartInfo::pointCount - 1. This is the 0th
///                 part of the display geo of the instancer object node.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_PartInfo::pointCount - @p start. This is the 0th
///                 part of the display geo of the instancer object node.
///                 <!-- source ::HAPI_PartInfo::pointCount - start -->
///
HAPI_DECL HAPI_GetInstanceTransformsOnPart( const HAPI_Session * session,
                                            HAPI_NodeId node_id,
                                            HAPI_PartId part_id,
                                            HAPI_RSTOrder rst_order,
                                            HAPI_Transform * transforms_array,
                                            int start, int length );

/// @brief  Set the transform of an individual object. Note that the object
///         nodes have to either be editable or have their transform
///         parameters exposed at the asset level. This won't work otherwise.
///
/// @ingroup Objects
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The object node id.
///
/// @param[in]      trans
///                 A ::HAPI_TransformEuler that stores the transform.
///
HAPI_DECL HAPI_SetObjectTransform( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   const HAPI_TransformEuler * trans );

/// @defgroup GeometryGetters Geometry Getters
/// Functions for reading Geometry (SOP) data

/// @brief  Get the display geo (SOP) node inside an Object node. If there
///         there are multiple display SOP nodes, only the first one is
///         returned. If the node is a display SOP itself, even if a network,
///         it will return its own geo info. If the node is a SOP but not
///         a network and not the display SOP, this function will fail.
///
///         The above implies that you can safely call this function on both
///         OBJ and SOP asset nodes and get the same (equivalent) geometry
///         display node back. SOP asset nodes will simply return themselves.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      object_node_id
///                 The object node id.
///
/// @param[out]     geo_info
///                 ::HAPI_GeoInfo return value.
///
HAPI_DECL HAPI_GetDisplayGeoInfo( const HAPI_Session * session,
                                  HAPI_NodeId object_node_id,
                                  HAPI_GeoInfo * geo_info );

/// @brief  A helper method that gets the number of main geometry outputs inside
///         an Object node or SOP node. If the node is an Object node, this
///         method will return the cumulative number of geometry outputs for all
///         geometry nodes that it contains. When searching for output geometry,
///         this method will only consider subnetworks that have their display
///         flag enabled.
///
///         This method must be called before HAPI_GetOutputGeoInfos() is
///         called.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id of the Object or SOP node to get the geometry
///                 output count of.
///
/// @param[out]     count
///                 The number of geometry (SOP) outputs.
HAPI_DECL HAPI_GetOutputGeoCount( const HAPI_Session* session,
                                  HAPI_NodeId node_id,
                                  int* count);

/// @brief  Gets the geometry info structs (::HAPI_GeoInfo) for a node's
///         main geometry outputs. This method can only be called after
///         HAPI_GetOutputGeoCount() has been called with the same node id.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id of the Object or SOP node to get the output
///                 geometry info structs (::HAPI_GeoInfo) for.
///
/// @param[out]     geo_infos_array
///                 Output array where the output geometry info structs will be
///                 stored. The size of the array must match the count argument
///                 returned by the HAPI_GetOutputGeoCount() method.
///
/// @param[in]      count
///                 Sanity check count. The count must be equal to the count
///                 returned by the HAPI_GetOutputGeoCount() method. 
HAPI_DECL HAPI_GetOutputGeoInfos( const HAPI_Session* session,
                                  HAPI_NodeId node_id,
                                  HAPI_GeoInfo* geo_infos_array,
                                  int count );

/// @brief  Get the geometry info struct (::HAPI_GeoInfo) on a SOP node.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     geo_info
///                 ::HAPI_GeoInfo return value.
///
HAPI_DECL HAPI_GetGeoInfo( const HAPI_Session * session,
                           HAPI_NodeId node_id,
                           HAPI_GeoInfo * geo_info );

/// @brief  Get a particular part info struct.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     part_info
///                 ::HAPI_PartInfo return value.
///
HAPI_DECL HAPI_GetPartInfo( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            HAPI_PartId part_id,
                            HAPI_PartInfo * part_info );


/// @brief  Gets the number of edges that belong to an edge group on a geometry
///         part.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      group_name
///                 The name of the edge group.
///
/// @param[out]     edge_count
///                 The number of edges that belong to the group.
///
HAPI_DECL HAPI_GetEdgeCountOfEdgeGroup( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_PartId part_id,
                                        const char * group_name,
                                        int * edge_count );

/// @brief  Get the array of faces where the nth integer in the array is
///         the number of vertices the nth face has.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     face_counts_array
///                 An integer array at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_PartInfo::faceCount - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_PartInfo::faceCount - @p start.
///                 <!-- source ::HAPI_PartInfo::faceCount - start -->
///
HAPI_DECL HAPI_GetFaceCounts( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              int * face_counts_array,
                              int start, int length );

/// @brief  Get array containing the vertex-point associations where the
///         ith element in the array is the point index the ith vertex
///         associates with.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     vertex_list_array
///                 An integer array at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_PartInfo::vertexCount - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_PartInfo::vertexCount - @p start.
///                 <!-- source ::HAPI_PartInfo::vertexCount - start -->
///
HAPI_DECL HAPI_GetVertexList( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              int * vertex_list_array,
                              int start, int length );

/// @defgroup Attributes
/// Functions for working with attributes.

/// @brief  Get the attribute info struct for the attribute specified by name.
///
/// @ingroup Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      owner
///                 Attribute owner.
///
/// @param[out]     attr_info
///                 ::HAPI_AttributeInfo to be filled. Check
///                 ::HAPI_AttributeInfo::exists to see if this attribute
///                 exists.
///
HAPI_DECL HAPI_GetAttributeInfo( const HAPI_Session * session,
                                 HAPI_NodeId node_id,
                                 HAPI_PartId part_id,
                                 const char * name,
                                 HAPI_AttributeOwner owner,
                                 HAPI_AttributeInfo * attr_info );

/// @brief  Get list of attribute names by attribute owner. Note that the
///         name string handles are only valid until the next time this
///         function is called.
///
/// @ingroup Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      owner
///                 The ::HAPI_AttributeOwner enum value specifying the
///                 owner of the attribute.
///
/// @param[out]     attribute_names_array
///                 Array of ints (string handles) to house the
///                 attribute names. Should be exactly the size of the
///                 appropriate attribute owner type count
///                 in ::HAPI_PartInfo.
///
/// @param[in]      count
///                 Sanity check count. Must be equal to the appropriate
///                 attribute owner type count in ::HAPI_PartInfo.
///
HAPI_DECL HAPI_GetAttributeNames( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  HAPI_PartId part_id,
                                  HAPI_AttributeOwner owner,
                                  HAPI_StringHandle * attribute_names_array,
                                  int count );

/// @brief  Get attribute integer data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeIntData( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    HAPI_PartId part_id,
                                    const char * name,
                                    HAPI_AttributeInfo * attr_info,
                                    int stride,
                                    int * data_array,
                                    int start, int length );

/// @brief  Get array attribute integer data.
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An integer array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///                 <!-- source ::HAPI_AttributeInfo::count -->
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeIntArrayData( const HAPI_Session * session,
                                         HAPI_NodeId node_id,
                                         HAPI_PartId part_id,
                                         const char * name,
                                         HAPI_AttributeInfo * attr_info,
                                         int * data_fixed_array,
                                         int data_fixed_length,
                                         int * sizes_fixed_array,
                                         int start, int sizes_fixed_length );

/// @brief  Get attribute unsigned 8-bit integer data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An unsigned 8-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeUInt8Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      HAPI_AttributeInfo * attr_info,
                                      int stride,
                                      HAPI_UInt8 * data_array,
                                      int start, int length );

/// @brief  Get array attribute unsigned 8-bit integer data.
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An unsigned 8-bit integer array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeUInt8ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           HAPI_AttributeInfo * attr_info,
                                           HAPI_UInt8 * data_fixed_array,
                                           int data_fixed_length,
                                           int * sizes_fixed_array,
                                           int start, int sizes_fixed_length );

/// @brief  Get attribute 8-bit integer data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An 8-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeInt8Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      HAPI_AttributeInfo * attr_info,
                                      int stride,
                                      HAPI_Int8 * data_array,
                                      int start, int length );

/// @brief  Get array attribute 8-bit integer data.
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An 8-bit integer array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeInt8ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           HAPI_AttributeInfo * attr_info,
                                           HAPI_Int8 * data_fixed_array,
                                           int data_fixed_length,
                                           int * sizes_fixed_array,
                                           int start, int sizes_fixed_length );

/// @brief  Get attribute 16-bit integer data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An 16-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeInt16Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      HAPI_AttributeInfo * attr_info,
                                      int stride,
                                      HAPI_Int16 * data_array,
                                      int start, int length );

/// @brief  Get array attribute 16-bit integer data. 
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An 16-bit integer array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeInt16ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           HAPI_AttributeInfo * attr_info,
                                           HAPI_Int16 * data_fixed_array,
                                           int data_fixed_length,
                                           int * sizes_fixed_array,
                                           int start, int sizes_fixed_length );

/// @brief  Get attribute 64-bit integer data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An 64-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeInt64Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      HAPI_AttributeInfo * attr_info,
                                      int stride,
                                      HAPI_Int64 * data_array,
                                      int start, int length );

/// @brief  Get array attribute 64-bit integer data.
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An 64-bit integer array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeInt64ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           HAPI_AttributeInfo * attr_info,
                                           HAPI_Int64 * data_fixed_array,
                                           int data_fixed_length,
                                           int * sizes_fixed_array,
                                           int start, int sizes_fixed_length );

/// @brief  Get attribute float data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An float array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeFloatData( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      HAPI_AttributeInfo * attr_info,
                                      int stride,
                                      float * data_array,
                                      int start, int length );

/// @brief  Get array attribute float data.
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An float array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeFloatArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           HAPI_AttributeInfo * attr_info,
                                           float * data_fixed_array,
                                           int data_fixed_length,
                                           int * sizes_fixed_array,
                                           int start, int sizes_fixed_length );

/// @brief  Get 64-bit attribute float data.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      stride
///                 Specifies how many items to skip over for each element.
///                 With a stride of -1, the stride will be set to
///                 @c attr_info->tuple_size. Otherwise, the stride will be
///                 set to the maximum of @c attr_info->tuple_size and
///                 @c stride.
///
/// @param[out]     data_array
///                 An 64-bit float array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeFloat64Data( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_PartId part_id,
                                        const char * name,
                                        HAPI_AttributeInfo * attr_info,
                                        int stride,
                                        double * data_array,
                                        int start, int length );

/// @brief  Get array attribute 64-bit float data.
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for the.
///                 totalArrayElements. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An 64-bit float array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeFloat64ArrayData( const HAPI_Session * session,
                                             HAPI_NodeId node_id,
                                             HAPI_PartId part_id,
                                             const char * name,
                                             HAPI_AttributeInfo * attr_info,
                                             double * data_fixed_array,
                                             int data_fixed_length,
                                             int * sizes_fixed_array,
                                             int start, int sizes_fixed_length );

/// @brief  Get attribute string data. Note that the string handles
///         returned are only valid until the next time this function
///         is called.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_array
///                 An ::HAPI_StringHandle array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeStringData( const HAPI_Session * session,
                                       HAPI_NodeId node_id,
                                       HAPI_PartId part_id,
                                       const char * name,
                                       HAPI_AttributeInfo * attr_info,
                                       HAPI_StringHandle * data_array,
                                       int start, int length );

/// @brief  Get array attribute string data. 
///         Each entry in an array attribute can have varying array lengths. 
///         Therefore the array values are returned as a flat array, with 
///         another sizes array containing the lengths of each array entry. 
///         Note that the string handles returned are only valid until 
///         the next time this function is called.
///
/// @ingroup GeometryGetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for the.
///                 totalArrayElements. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[out]     data_fixed_array
///                 An ::HAPI_StringHandle array at least the size of
///                 <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///
/// @param[in]      data_fixed_length
///                 Must be <tt>::HAPI_AttributeInfo::totalArrayElements</tt>.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements -->
///
/// @param[out]     sizes_fixed_array
///                 An integer array at least the size of
///                 <tt>sizes_fixed_length</tt> to hold the size of each entry.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 Note, if 0 is passed for length, the function will just
///                 do nothing and return ::HAPI_RESULT_SUCCESS.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_GetAttributeStringArrayData( const HAPI_Session * session,
                                            HAPI_NodeId node_id,
                                            HAPI_PartId part_id,
                                            const char * name,
                                            HAPI_AttributeInfo * attr_info,
                                            HAPI_StringHandle * data_fixed_array,
                                            int data_fixed_length,
                                            int * sizes_fixed_array,
                                            int start, int sizes_fixed_length );

/// @brief  Get group names for an entire geo. Please note that this
///         function is NOT per-part, but it is per-geo. The companion
///         function ::HAPI_GetGroupMembership() IS per-part. Also keep
///         in mind that the name string handles are only
///         valid until the next time this function is called.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[out]     group_names_array
///                 The array of names to be filled. Should be the size
///                 given by ::HAPI_GeoInfo_GetGroupCountByType() with
///                 @p group_type and the ::HAPI_GeoInfo of @p geo_id.
///                 @note These string handles are only valid until the
///                 next call to ::HAPI_GetGroupNames().
///
/// @param[in]      group_count
///                 Sanity check. Should be less than or equal to the size
///                 of @p group_names.
///
HAPI_DECL HAPI_GetGroupNames( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_GroupType group_type,
                              HAPI_StringHandle * group_names_array,
                              int group_count );

/// @brief  Get group membership.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[in]      group_name
///                 The group name.
///
/// @param[out]     membership_array_all_equal
///                 (optional) Quick way to determine if all items are in
///                 the given group or all items our not in the group.
///                 If you do not need this information or you are getting edge
///                 group information, you can just pass NULL for this argument.
///
/// @param[out]     membership_array
///                 Array of ints that represent the membership of this
///                 group. When getting membership information for a point or
///                 primitive group, the size of the array should be the size
///                 given by ::HAPI_PartInfo_GetElementCountByGroupType() with
///                 @p group_type and the ::HAPI_PartInfo of @p part_id. When
///                 retrieving the edges belonging to an edge group, the
///                 membership array will be filled with point numbers that
///                 comprise the edges of the edge group. Each edge is specified
///                 by two points, which means that the size of the array should
///                 be the size given by ::HAPI_GetEdgeCountOfEdgeGroup() * 2.
///
/// @param[in]      start
///                 Start offset into the membership array. Must be
///                 less than ::HAPI_PartInfo_GetElementCountByGroupType() when
///                 it is a point or primitive group. When getting the
///                 membership information for an edge group, this argument must
///                 be less than the size returned by
///                 ::HAPI_GetEdgeCountOfEdgeGroup() * 2 - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Should be less than or equal to the size
///                 of @p membership_array.
///
HAPI_DECL HAPI_GetGroupMembership( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PartId part_id,
                                   HAPI_GroupType group_type,
                                   const char * group_name,
                                   HAPI_Bool * membership_array_all_equal,
                                   int * membership_array,
                                   int start, int length );

/// @brief  Get group counts for a specific packed instanced part.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id. (should be a packed primitive)
///
/// @param[out]     pointGroupCount
///                 Number of point groups on the packed instance part.
///                 Will be set to -1 if the part is not a valid packed part.
///
/// @param[out]     primitiveGroupCount
///                 Number of primitive groups on the instanced part.
///                 Will be set to -1 if the part is not a valid instancer
///
HAPI_DECL HAPI_GetGroupCountOnPackedInstancePart( const HAPI_Session * session,
                                                  HAPI_NodeId node_id,
                                                  HAPI_PartId part_id,
                                                  int * pointGroupCount,
                                                  int * primitiveGroupCount );

/// @brief  Get the group names for a packed instance part
///         This functions allows you to get the group name for a specific
///         packed primitive part.
///         Keep in mind that the name string handles are only
///         valid until the next time this function is called.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id. (should be a packed primitive)
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[out]     group_names_array
///                 The array of names to be filled. Should be the size
///                 given by ::HAPI_GetGroupCountOnPackedInstancePart() with
///                 @p group_type and the ::HAPI_PartInfo of @p part_id.
///                 @note These string handles are only valid until the
///                 next call to ::HAPI_GetGroupNamesOnPackedInstancePart().
///
/// @param[in]      group_count
///                 Sanity check. Should be less than or equal to the size
///                 of @p group_names.
///
HAPI_DECL HAPI_GetGroupNamesOnPackedInstancePart( const HAPI_Session * session,
                                                  HAPI_NodeId node_id,
                                                  HAPI_PartId part_id,
                                                  HAPI_GroupType group_type,
                                                  HAPI_StringHandle * group_names_array,
                                                  int group_count );

/// @brief  Get group membership for a packed instance part
///         This functions allows you to get the group membership for a specific
///         packed primitive part.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id. (should be a packed primitive)
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[in]      group_name
///                 The group name.
///
/// @param[out]     membership_array_all_equal
///                 (optional) Quick way to determine if all items are in
///                 the given group or all items our not in the group.
///                 You can just pass NULL here if not interested.
///
/// @param[out]     membership_array
///                 Array of ints that represent the membership of this
///                 group. Should be the size given by
///                 ::HAPI_PartInfo_GetElementCountByGroupType() with
///                 @p group_type and the ::HAPI_PartInfo of @p part_id.
///
/// @param[in]      start
///                 Start offset into the membership array. Must be
///                 less than ::HAPI_PartInfo_GetElementCountByGroupType().
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Should be less than or equal to the size
///                 of @p membership_array.
///		    <!-- source ::HAPI_PartInfo_GetElementCountByGroupType -->
///
HAPI_DECL HAPI_GetGroupMembershipOnPackedInstancePart( const HAPI_Session * session,
                                                       HAPI_NodeId node_id,
                                                       HAPI_PartId part_id,
                                                       HAPI_GroupType group_type,
                                                       const char * group_name,
                                                       HAPI_Bool * membership_array_all_equal,
                                                       int * membership_array,
                                                       int start, int length );

/// @brief  Get the part ids that this instancer part is instancing.
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The instancer part id.
///
/// @param[out]     instanced_parts_array
///                 Array of ::HAPI_PartId's to instance.
///
/// @param[in]      start
///                 Should be less than @p part_id's
///                 ::HAPI_PartInfo::instancedPartCount but more than or
///                 equal to 0.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Should be less than @p part_id's
///                 ::HAPI_PartInfo::instancedPartCount - @p start.
///                 <!-- source ::HAPI_PartInfo::instancedPartCount - start -->
///
HAPI_DECL HAPI_GetInstancedPartIds( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    HAPI_PartId part_id,
                                    HAPI_PartId * instanced_parts_array,
                                    int start, int length );

/// @brief  Get the instancer part's list of transforms on which to
///         instance the instanced parts you got from
///         ::HAPI_GetInstancedPartIds().
///
/// @ingroup GeometryGetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The instancer part id.
///
/// @param[in]      rst_order
///                 The order of application of translation, rotation and
///                 scale.
///
/// @param[out]     transforms_array
///                 Array of ::HAPI_PartId's to instance.
///
/// @param[in]      start
///                 Should be less than @p part_id's
///                 ::HAPI_PartInfo::instanceCount but more than or
///                 equal to 0.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Should be less than @p part_id's
///                 ::HAPI_PartInfo::instanceCount - @p start.
///                 <!-- source ::HAPI_PartInfo::instanceCount - start -->
///
HAPI_DECL HAPI_GetInstancerPartTransforms( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           HAPI_RSTOrder rst_order,
                                           HAPI_Transform * transforms_array,
                                           int start, int length );

/// @defgroup GeometrySetters Geometry Setters
/// Functions for setting geometry (SOP) data

/// @brief  Set the main part info struct (::HAPI_PartInfo).
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///                 <!-- default 0 -->
///
/// @param[in]      part_info
///                 ::HAPI_PartInfo value that describes the input
///                 geometry.
///
HAPI_DECL HAPI_SetPartInfo( const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            HAPI_PartId part_id,
                            const HAPI_PartInfo * part_info );

/// @brief  Set the array of faces where the nth integer in the array is
///         the number of vertices the nth face has.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///                 <!-- default 0 -->
///
/// @param[in]      face_counts_array
///                 An integer array at least the size of @p length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_PartInfo::faceCount - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_PartInfo::faceCount - @p start.
///                 <!-- source ::HAPI_PartInfo::faceCount - start -->
///
HAPI_DECL HAPI_SetFaceCounts( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              const int * face_counts_array,
                              int start, int length );

/// @brief  Set array containing the vertex-point associations where the
///         ith element in the array is the point index the ith vertex
///         associates with.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      vertex_list_array
///                 An integer array at least the size of length.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_PartInfo::vertexCount - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_PartInfo::vertexCount - @p start.
///                 <!-- source ::HAPI_PartInfo::vertexCount - start -->
///
HAPI_DECL HAPI_SetVertexList( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              const int * vertex_list_array,
                              int start, int length );

/// @brief  Add an attribute.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo stores attribute properties.
///
HAPI_DECL HAPI_AddAttribute( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             const char * name,
                             const HAPI_AttributeInfo * attr_info );

/// @brief  Delete an attribute from an input geo
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo stores attribute properties.
///
HAPI_DECL HAPI_DeleteAttribute( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             const char * name,
                             const HAPI_AttributeInfo * attr_info );

/// @brief  Set attribute integer data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeIntData( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    HAPI_PartId part_id,
                                    const char * name,
                                    const HAPI_AttributeInfo * attr_info,
                                    const int * data_array,
                                    int start, int length );

/// @brief  Set unsigned 8-bit attribute integer data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An unsigned 8-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeUInt8Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      const HAPI_AttributeInfo * attr_info,
                                      const HAPI_UInt8 * data_array,
                                      int start, int length );

/// @brief  Set 8-bit attribute integer data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An 8-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeInt8Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      const HAPI_AttributeInfo * attr_info,
                                      const HAPI_Int8 * data_array,
                                      int start, int length );

/// @brief  Set 16-bit attribute integer data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An 16-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeInt16Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      const HAPI_AttributeInfo * attr_info,
                                      const HAPI_Int16 * data_array,
                                      int start, int length );

/// @brief  Set 64-bit attribute integer data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An 64-bit integer array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeInt64Data( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      const HAPI_AttributeInfo * attr_info,
                                      const HAPI_Int64 * data_array,
                                      int start, int length );

/// @brief  Set attribute float data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An float array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeFloatData( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      const char * name,
                                      const HAPI_AttributeInfo * attr_info,
                                      const float * data_array,
                                      int start, int length );

/// @brief  Set 64-bit attribute float data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An 64-bit float array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeFloat64Data( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_PartId part_id,
                                        const char * name,
                                        const HAPI_AttributeInfo * attr_info,
                                        const double * data_array,
                                        int start, int length );

/// @brief  Set attribute string data.
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo used as input for what tuple size.
///                 you want. Also contains some sanity checks like
///                 data type. Generally should be the same struct
///                 returned by ::HAPI_GetAttributeInfo().
///
/// @param[in]      data_array
///                 An ::HAPI_StringHandle array at least the size of
///                 <tt>length * ::HAPI_AttributeInfo::tupleSize</tt>.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at
///                 most ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
///
HAPI_DECL HAPI_SetAttributeStringData( const HAPI_Session * session,
                                       HAPI_NodeId node_id,
                                       HAPI_PartId part_id,
                                       const char * name,
                                       const HAPI_AttributeInfo * attr_info,
                                       const char ** data_array,
                                       int start, int length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the int values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeIntArrayData( const HAPI_Session * session,
                                         HAPI_NodeId node_id,
                                         HAPI_PartId part_id,
                                         const char * name,
                                         const HAPI_AttributeInfo * attr_info,
                                         const int * data_fixed_array,
                                         int data_fixed_length,
                                         const int * sizes_fixed_array,
                                         int start,
                                         int sizes_fixed_length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the HAPI_UInt8 values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeUInt8ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           const HAPI_AttributeInfo * attr_info,
                                           const HAPI_UInt8 * data_fixed_array,
                                           int data_fixed_length,
                                           const int * sizes_fixed_array,
                                           int start,
                                           int sizes_fixed_length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the HAPI_Int8 values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeInt8ArrayData( const HAPI_Session * session,
                                          HAPI_NodeId node_id,
                                          HAPI_PartId part_id,
                                          const char * name,
                                          const HAPI_AttributeInfo * attr_info,
                                          const HAPI_Int8 * data_fixed_array,
                                          int data_fixed_length,
                                          const int * sizes_fixed_array,
                                          int start,
                                          int sizes_fixed_length);

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the HAPI_Int16 values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeInt16ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           const HAPI_AttributeInfo * attr_info,
                                           const HAPI_Int16 * data_fixed_array,
                                           int data_fixed_length,
                                           const int * sizes_fixed_array,
                                           int start,
                                           int sizes_fixed_length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the HAPI_Int64 values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeInt64ArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           const HAPI_AttributeInfo * attr_info,
                                           const HAPI_Int64 * data_fixed_array,
                                           int data_fixed_length,
                                           const int * sizes_fixed_array,
                                           int start,
                                           int sizes_fixed_length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the float values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeFloatArrayData( const HAPI_Session * session,
                                           HAPI_NodeId node_id,
                                           HAPI_PartId part_id,
                                           const char * name,
                                           const HAPI_AttributeInfo * attr_info,
                                           const float * data_fixed_array,
                                           int data_fixed_length,
                                           const int * sizes_fixed_array,
                                           int start,
                                           int sizes_fixed_length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the double values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeFloat64ArrayData( const HAPI_Session * session,
                                             HAPI_NodeId node_id,
                                             HAPI_PartId part_id,
                                             const char * name,
                                             const HAPI_AttributeInfo * attr_info,
                                             const double * data_fixed_array,
                                             int data_fixed_length,
                                             const int * sizes_fixed_array,
                                             int start,
                                             int sizes_fixed_length );

/// @brief  
///
/// @ingroup GeometrySetters Attributes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      name
///                 Attribute name.
///
/// @param[in]      attr_info
///                 ::HAPI_AttributeInfo that contains the description for the
///                 attribute that is being set.
///
/// @param[in]      data_fixed_array
///                 An array containing the string values of the attribute.
///
/// @param[in]      data_fixed_length
///                 The total size of the data array. The size can be no greater
///                 than the <tt>::HAPI_AttributeInfo::totalArrayElements</tt>
///                 of the attribute.
///                 <!-- source ::HAPI_AttributeInfo::totalArrayElements --> 
///
/// @param[in]      sizes_fixed_array
///                 An array of integers that contains the sizes of each
///                 attribute array. This is required because the attribute
///                 array for each geometry component can be of variable size.
///
/// @param[in]      start
///                 First index of range. Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - 1.
///                 <!-- default 0 -->
///
/// @param[in]      sizes_fixed_length
///                 Must be at least 0 and at most
///                 ::HAPI_AttributeInfo::count - @p start.
///                 <!-- source ::HAPI_AttributeInfo::count - start -->
HAPI_DECL HAPI_SetAttributeStringArrayData( const HAPI_Session * session,
                                            HAPI_NodeId node_id,
                                            HAPI_PartId part_id,
                                            const char * name,
                                            const HAPI_AttributeInfo * attr_info,
                                            const char ** data_fixed_array,
                                            int data_fixed_length,
                                            const int * sizes_fixed_array,
                                            int start,
                                            int sizes_fixed_length );

/// @brief  Add a group to the input geo with the given type and name.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[in]      group_name
///                 Name of new group to be added.
///
HAPI_DECL HAPI_AddGroup( const HAPI_Session * session,
                         HAPI_NodeId node_id,
                         HAPI_PartId part_id,
                         HAPI_GroupType group_type,
                         const char * group_name );

/// @brief  Remove a group from the input geo with the given type and name.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[in]      group_name
///                 Name of the group to be removed
///
HAPI_DECL HAPI_DeleteGroup( const HAPI_Session * session,
                         HAPI_NodeId node_id,
                         HAPI_PartId part_id,
                         HAPI_GroupType group_type,
                         const char * group_name );

/// @brief  Set group membership.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
/// @param[in]      part_id
///                 Currently not used. Just pass 0.
///                 <!-- default 0 -->
///
/// @param[in]      group_type
///                 The group type.
///
/// @param[in]      group_name
///                 The group name.
///
/// @param[in]      membership_array
///                 Array of ints that represent the membership of this
///                 group. Should be the size given by
///                 ::HAPI_PartInfo_GetElementCountByGroupType() with
///                 @p group_type and the ::HAPI_PartInfo of @p part_id.
///
/// @param[in]      start
///                 Start offset into the membership array. Must be
///                 less than ::HAPI_PartInfo_GetElementCountByGroupType().
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 Should be less than or equal to the size
///                 of @p membership_array. When setting edge group membership,
///                 this parameter should be set to the number of points (which
///                 are used to implictly define the edges), not to the number
///                 edges in the group.
///		    <!-- source ::HAPI_PartInfo_GetElementCountByGroupType -->
///
HAPI_DECL HAPI_SetGroupMembership( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PartId part_id,
                                   HAPI_GroupType group_type,
                                   const char * group_name,
                                   const int * membership_array,
                                   int start, int length );

/// @brief  Commit the current input geometry to the cook engine. Nodes
///         that use this geometry node will re-cook using the input
///         geometry given through the geometry setter API calls.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
HAPI_DECL HAPI_CommitGeo( const HAPI_Session * session,
                          HAPI_NodeId node_id );

/// @brief  Remove all changes that have been committed to this
///         geometry. If this is an intermediate result node (Edit SOP), all
///         deltas will be removed. If it's any other type of node, the node
///         will be unlocked if it is locked.
///
/// @ingroup GeometrySetters
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The SOP node id.
///
HAPI_DECL HAPI_RevertGeo( const HAPI_Session * session,
                          HAPI_NodeId node_id );

/// @defgroup Materials
/// Functions for working with materials

/// @brief  Get material ids by face/primitive. The material ids returned
///         will be valid as long as the asset is alive. You should query
///         this list after every cook to see if the material assignments
///         have changed. You should also query each material individually
///         using ::HAPI_GetMaterialInfo() to see if it is dirty and needs
///         to be re-imported.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      geometry_node_id
///                 The geometry node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     are_all_the_same
///                 (optional) If true, all faces on this part have the
///                 same material assignment. You can pass NULL here.
///
/// @param[out]     material_ids_array
///                 An array of ::HAPI_NodeId at least the size of
///                 @p length and at most the size of
///                 ::HAPI_PartInfo::faceCount.
///
/// @param[in]      start
///                 The starting index into the list of faces from which
///                 you wish to get the material ids from. Note that
///                 this should be less than ::HAPI_PartInfo::faceCount.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of material ids you wish to get. Note that
///                 this should be at most:
///                 ::HAPI_PartInfo::faceCount - @p start.
///                 <!-- source ::HAPI_PartInfo::faceCount - start -->
///
HAPI_DECL HAPI_GetMaterialNodeIdsOnFaces( const HAPI_Session * session,
                                          HAPI_NodeId geometry_node_id,
                                          HAPI_PartId part_id,
                                          HAPI_Bool * are_all_the_same,
                                          HAPI_NodeId * material_ids_array,
                                          int start, int length );

/// @brief  Get the material info.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[out]     material_info
///                 The returned material info.
///
HAPI_DECL HAPI_GetMaterialInfo( const HAPI_Session * session,
                                HAPI_NodeId material_node_id,
                                HAPI_MaterialInfo * material_info );

/// @brief  Render a single texture from a COP to an image for
///         later extraction.
///
/// @ingroup Materials
///
///         Note that you must call this first for any of the other material
///         APIs to work.
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      cop_node_id
///                 The COP node id.
///
HAPI_DECL HAPI_RenderCOPToImage( const HAPI_Session * session,
                                 HAPI_NodeId cop_node_id );

/// @brief  Render only a single texture to an image for later extraction.
///         An example use of this method might be to render the diffuse,
///         normal, and bump texture maps of a material to individual
///         texture files for use within the client application.
///
///         Note that you must call this first for any of the other material
///         APIs to work.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[in]      parm_id
///                 This is the index in the parameter list of the
///                 material_id's node of the parameter containing the
///                 texture map file path.
///
HAPI_DECL HAPI_RenderTextureToImage( const HAPI_Session * session,
                                     HAPI_NodeId material_node_id,
                                     HAPI_ParmId parm_id );

/// @brief  Get information about the image that was just rendered, like
///         resolution and default file format. This information will be
///         used when extracting planes to an image.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[out]     image_info
///                 The struct containing the image information.
///
HAPI_DECL HAPI_GetImageInfo( const HAPI_Session * session,
                             HAPI_NodeId material_node_id,
                             HAPI_ImageInfo * image_info );

/// @brief  Set image information like resolution and file format.
///         This information will be used when extracting planes to
///         an image.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
///         You should also first call ::HAPI_GetImageInfo() to get the
///         current Image Info and change only the properties
///         you don't like.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[in]      image_info
///                 The struct containing the new image information.
///
HAPI_DECL HAPI_SetImageInfo( const HAPI_Session * session,
                             HAPI_NodeId material_node_id,
                             const HAPI_ImageInfo * image_info );

/// @brief  Get the number of image planes for the just rendered image.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[out]     image_plane_count
///                 The number of image planes.
///
HAPI_DECL HAPI_GetImagePlaneCount( const HAPI_Session * session,
                                   HAPI_NodeId material_node_id,
                                   int * image_plane_count );

/// @brief  Get the names of the image planes of the just rendered image.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
///         You should also call ::HAPI_GetImagePlaneCount() first to get
///         the total number of image planes so you know how large the
///         image_planes string handle array should be.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[out]     image_planes_array
///                 The image plane names.
///
/// @param[in]      image_plane_count
///                 The number of image planes to get names for. This
///                 must be less than or equal to the count returned
///                 by ::HAPI_GetImagePlaneCount().
///                 <!-- source ::HAPI_GetImagePlaneCount -->
///
HAPI_DECL HAPI_GetImagePlanes( const HAPI_Session * session,
                               HAPI_NodeId material_node_id,
                               HAPI_StringHandle * image_planes_array,
                               int image_plane_count );

/// @brief  Extract a rendered image to a file.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[in]      image_file_format_name
///                 The image file format name you wish the image to be
///                 extracted as. You can leave this parameter NULL to
///                 get the image in the original format if it comes from
///                 another texture file or in the default HAPI format,
///                 which is ::HAPI_DEFAULT_IMAGE_FORMAT_NAME, if the image
///                 is generated.
///
///                 You can get some of the very common standard image
///                 file format names from HAPI_Common.h under the
///                 "Defines" section.
///
///                 You can also get a list of all supported file formats
///                 (and the exact names this parameter expects)
///                 by using ::HAPI_GetSupportedImageFileFormats(). This
///                 list will include custom file formats you created via
///                 custom DSOs (see HDK docs about IMG_Format). You will
///                 get back a list of ::HAPI_ImageFileFormat. This
///                 parameter expects the ::HAPI_ImageFileFormat::nameSH
///                 of a given image file format.
///
/// @param[in]      image_planes
///                 The image planes you wish to extract into the file.
///                 Multiple image planes should be separated by spaces.
///
/// @param[in]      destination_folder_path
///                 The folder where the image file should be created.
///
/// @param[in]      destination_file_name
///                 Optional parameter to overwrite the name of the
///                 extracted texture file. This should NOT include
///                 the extension as the file type will be decided
///                 by the ::HAPI_ImageInfo you can set using
///                 ::HAPI_SetImageInfo(). You still have to use
///                 destination_file_path to get the final file path.
///
///                 Pass in NULL to have the file name be automatically
///                 generated from the name of the material SHOP node,
///                 the name of the texture map parameter if the
///                 image was rendered from a texture, and the image
///                 plane names specified.
///
/// @param[out]     destination_file_path
///                 The full path string handle, including the
///                 destination_folder_path and the texture file name,
///                 to the extracted file. Note that this string handle
///                 will only be valid until the next call to
///                 this function.
///
HAPI_DECL HAPI_ExtractImageToFile( const HAPI_Session * session,
                                   HAPI_NodeId material_node_id,
                                   const char * image_file_format_name,
                                   const char * image_planes,
                                   const char * destination_folder_path,
                                   const char * destination_file_name,
                                   int * destination_file_path );
/// @brief  Get the file name that this image would be saved to
///
///         Check to see what file path HAPI_ExtractImageToFile would have
///         saved to given the same parms. Perhaps you might wish to see
///         if it already exists before extracting.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[in]      image_file_format_name
///                 The image file format name you wish the image to be
///                 extracted as. See HAPI_ExtractImageToFile for more information.
///
/// @param[in]      image_planes
///                 The image planes you wish to extract into the file.
///                 Multiple image planes should be separated by spaces.
///
/// @param[in]      destination_folder_path
///                 The folder where the image file sould be created.
///
/// @param[in]      destination_file_name
///                 Optional parameter to overwrite the name of the
///                 extracted texture file. See HAPI_ExtractImageToFile for more information.
///
/// @param[in]      texture_parm_id
///                 The index in the parameter list of the material node.
///                 of the parameter containing the texture map file path
///
/// @param[out]     destination_file_path
///                 The full path string handle, including the
///                 destination_folder_path and the texture file name,
///                 to the extracted file. Note that this string handle
///                 will only be valid until the next call to
///                 this function.
///
HAPI_DECL HAPI_GetImageFilePath( const HAPI_Session * session,
                                   HAPI_NodeId material_node_id,
                                   const char * image_file_format_name,
                                   const char * image_planes,
                                   const char * destination_folder_path,
                                   const char * destination_file_name,
				   HAPI_ParmId texture_parm_id,
                                   int * destination_file_path );

/// @brief  Extract a rendered image to memory.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
///         Also note that this function will do all the work of
///         extracting and compositing the image into a memory buffer
///         but will not return to you that buffer, only its size. Use
///         the returned size to allocated a sufficiently large buffer
///         and call ::HAPI_GetImageMemoryBuffer() to fill your buffer
///         with the just extracted image.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[in]      image_file_format_name
///                 The image file format name you wish the image to be
///                 extracted as. You can leave this parameter NULL to
///                 get the image in the original format if it comes from
///                 another texture file or in the default HAPI format,
///                 which is ::HAPI_DEFAULT_IMAGE_FORMAT_NAME, if the image
///                 is generated.
///
///                 You can get some of the very common standard image
///                 file format names from HAPI_Common.h under the
///                 "Defines" section.
///
///                 You can also get a list of all supported file formats
///                 (and the exact names this parameter expects)
///                 by using ::HAPI_GetSupportedImageFileFormats(). This
///                 list will include custom file formats you created via
///                 custom DSOs (see HDK docs about IMG_Format). You will
///                 get back a list of ::HAPI_ImageFileFormat. This
///                 parameter expects the ::HAPI_ImageFileFormat::nameSH
///                 of a given image file format.
///
/// @param[in]      image_planes
///                 The image planes you wish to extract into the file.
///                 Multiple image planes should be separated by spaces.
///
/// @param[out]     buffer_size
///                 The extraction will be done to an internal buffer
///                 who's size you get via this parameter. Use the
///                 returned buffer_size when calling
///                 ::HAPI_GetImageMemoryBuffer() to get the image
///                 buffer you just extracted.
///
HAPI_DECL HAPI_ExtractImageToMemory( const HAPI_Session * session,
                                     HAPI_NodeId material_node_id,
                                     const char * image_file_format_name,
                                     const char * image_planes,
                                     int * buffer_size );

/// @brief  Fill your allocated buffer with the just extracted
///         image buffer.
///
///         Note that you must call ::HAPI_RenderTextureToImage() first for
///         this method call to make sense.
///
///         Also note that you must call ::HAPI_ExtractImageToMemory()
///         first in order to perform the extraction and get the
///         extracted image buffer size that you need to know how much
///         memory to allocated to fit your extracted image.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      material_node_id
///                 The material node id.
///
/// @param[out]     buffer
///                 The buffer passed in here will be filled with the
///                 image buffer created during the call to
///                 ::HAPI_ExtractImageToMemory().
///
/// @param[in]      length
///                 Sanity check. This size should be the same as the
///                 size allocated for the buffer passed in and should
///                 be at least as large as the buffer_size returned by
///                 the call to ::HAPI_ExtractImageToMemory().
///                 <!-- source ::HAPI_ExtractImageToMemory -->
///
HAPI_DECL HAPI_GetImageMemoryBuffer( const HAPI_Session * session,
                                     HAPI_NodeId material_node_id,
                                     char * buffer, int length );

/// @brief  Get the number of supported texture file formats.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     file_format_count
///                 The number of supported texture file formats.
///
HAPI_DECL HAPI_GetSupportedImageFileFormatCount( const HAPI_Session * session,
                                                 int * file_format_count );

/// @brief  Get a list of support image file formats - their names,
///         descriptions and a list of recognized extensions.
///
///         Note that you MUST call
///         ::HAPI_GetSupportedImageFileFormatCount()
///         before calling this function for the first time.
///
/// @ingroup Materials
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     formats_array
///                 The list of ::HAPI_ImageFileFormat structs to
///                 be filled.
///
/// @param[in]      file_format_count
///                 The number of supported texture file formats. This
///                 should be at least as large as the count returned
///                 by ::HAPI_GetSupportedImageFileFormatCount().
///                 <!-- source ::HAPI_GetSupportedImageFileFormatCount -->
///
HAPI_DECL HAPI_GetSupportedImageFileFormats(
                                        const HAPI_Session * session,
                                        HAPI_ImageFileFormat * formats_array,
                                        int file_format_count );

/// @defgroup Animation
/// Functions for working with animation.

/// @brief  Set an animation curve on a parameter of an exposed node.
///
/// @ingroup Animation
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The exposed node id.
///
/// @param[in]      parm_id
///                 The id of an exposed parameter within the node.
/// @param[in]      parm_index
///                 The index of the parameter, if it is for example
///                 a 3 tuple
///
/// @param[in]      curve_keyframes_array
///                 An array of ::HAPI_Keyframe structs that describes
///                 the keys on this curve.
///
/// @param[in]      keyframe_count
///                 The number of keys on the curve.
///
HAPI_DECL HAPI_SetAnimCurve( const HAPI_Session * session,
                             HAPI_NodeId node_id, HAPI_ParmId parm_id,
                             int parm_index,
                             const HAPI_Keyframe * curve_keyframes_array,
                             int keyframe_count );

/// @brief  A specialized convenience function to set the T,R,S values
///         on an exposed node.
///
/// @ingroup Animation
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The exposed node id.
///
/// @param[in]      trans_comp
///                 A value of ::HAPI_TransformComponent that
///                 identifies the particular component of the
///                 transform to attach the curve to, for example
///                 ::HAPI_TRANSFORM_TX.
///
/// @param[in]      curve_keyframes_array
///                 An array of ::HAPI_Keyframe structs that describes
///                 the keys on this curve.
///
/// @param[in]      keyframe_count
///                 The number of keys on the curve.
///
HAPI_DECL HAPI_SetTransformAnimCurve(
                            const HAPI_Session * session,
                            HAPI_NodeId node_id,
                            HAPI_TransformComponent trans_comp,
                            const HAPI_Keyframe * curve_keyframes_array,
                            int keyframe_count );

/// @brief  Resets the simulation cache of the asset.  This is very useful
///         for assets that use dynamics, to be called after some
///         setup has changed for the asset - for example, asset inputs
///
/// @ingroup Time
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The asset node id.
///
HAPI_DECL HAPI_ResetSimulation( const HAPI_Session * session,
                                HAPI_NodeId node_id );

/// @defgroup Volumes
/// Functions for working with Volume data

/// @brief  Retrieve any meta-data about the volume primitive, including
///         its transform, location, scale, taper, resolution.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     volume_info
///                 The meta-data associated with the volume on the
///                 part specified by the previous parameters.
///
HAPI_DECL HAPI_GetVolumeInfo( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              HAPI_VolumeInfo * volume_info );

/// @brief  Iterate through a volume based on 8x8x8 sections of the volume
///         Start iterating through the value of the volume at part_id.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     tile
///                 The tile info referring to the first tile in the
///                 volume at part_id.
///
HAPI_DECL HAPI_GetFirstVolumeTile( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PartId part_id,
                                   HAPI_VolumeTileInfo * tile );

/// @brief  Iterate through a volume based on 8x8x8 sections of the volume
///         Continue iterating through the value of the volume at part_id.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     tile
///                 The tile info referring to the next tile in the
///                 set of tiles associated with the volume at this part.
///
HAPI_DECL HAPI_GetNextVolumeTile( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  HAPI_PartId part_id,
                                  HAPI_VolumeTileInfo * tile );

/// @brief  Retrieve floating point values of the voxel at a specific
///         index. Note that you must call ::HAPI_GetVolumeInfo() prior
///         to this call.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      x_index
///                 The x index/coordinate of the voxel.
///
/// @param[in]      y_index
///                 The y index/coordinate of the voxel.
///
/// @param[in]      z_index
///                 The z index/coordinate of the voxel.
///
/// @param[out]     values_array
///                 The values of the voxel.
///
/// @param[in]      value_count
///                 Should be equal to the volume's
///                 ::HAPI_VolumeInfo::tupleSize.
///                 <!-- source ::HAPI_VolumeInfo::tupleSize -->
///
HAPI_DECL HAPI_GetVolumeVoxelFloatData( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_PartId part_id,
                                        int x_index,
                                        int y_index,
                                        int z_index,
                                        float * values_array,
                                        int value_count );

/// @brief  Retrieve floating point values of the voxels pointed to
///         by a tile. Note that a tile may extend beyond the limits
///         of the volume so not all values in the given buffer will
///         be written to. Voxels outside the volume will be initialized
///         to the given fill value.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      fill_value
///                 Value that will be used to fill the @p values_array.
///                 This is useful so that you can see what values
///                 have actually been written to.
///
/// @param[in]      tile
///                 The tile to retrieve.
///
/// @param[out]     values_array
///                 The values of the tile.
///
/// @param[in]      length
///                 The length should be ( 8 ^ 3 ) * tupleSize.
///
HAPI_DECL HAPI_GetVolumeTileFloatData( const HAPI_Session * session,
                                       HAPI_NodeId node_id,
                                       HAPI_PartId part_id,
                                       float fill_value,
                                       const HAPI_VolumeTileInfo * tile,
                                       float * values_array,
                                       int length );

/// @brief  Retrieve integer point values of the voxel at a specific
///         index. Note that you must call ::HAPI_GetVolumeInfo() prior
///         to this call.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      x_index
///                 The x index/coordinate of the voxel.
///
/// @param[in]      y_index
///                 The y index/coordinate of the voxel.
///
/// @param[in]      z_index
///                 The z index/coordinate of the voxel.
///
/// @param[out]     values_array
///                 The values of the voxel.
///
/// @param[in]      value_count
///                 Should be equal to the volume's
///                 ::HAPI_VolumeInfo::tupleSize.
///                 <!-- source ::HAPI_VolumeInfo::tupleSize -->
///
HAPI_DECL HAPI_GetVolumeVoxelIntData( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PartId part_id,
                                      int x_index,
                                      int y_index,
                                      int z_index,
                                      int * values_array,
                                      int value_count );

/// @brief  Retrieve integer point values of the voxels pointed to
///         by a tile. Note that a tile may extend beyond the limits
///         of the volume so not all values in the given buffer will
///         be written to. Voxels outside the volume will be initialized
///         to the given fill value.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      fill_value
///                 Value that will be used to fill the @p values_array.
///                 This is useful so that you can see what values
///                 have actually been written to.
///
/// @param[in]      tile
///                 The tile to retrieve.
///
/// @param[out]     values_array
///                 The values of the tile.
///
/// @param[in]      length
///                 The length should be ( 8 ^ 3 ) * tupleSize.
///
HAPI_DECL HAPI_GetVolumeTileIntData( const HAPI_Session * session,
                                     HAPI_NodeId node_id,
                                     HAPI_PartId part_id,
                                     int fill_value,
                                     const HAPI_VolumeTileInfo * tile,
                                     int * values_array,
                                     int length );

/// @brief  Get the height field data for a terrain volume as a flattened
///         2D array of float heights. Should call ::HAPI_GetVolumeInfo()
///         first to make sure the volume info is initialized.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     values_array
///                 Heightfield flattened array. Should be at least the size of
///                 @p start + @p length.
///
/// @param[in]      start
///                 The start at least 0 and at most
///                 ( ::HAPI_VolumeInfo::xLength * ::HAPI_VolumeInfo::yLength )
///                 - @p length.
///
/// @param[in]      length
///                 The length should be at least 1 or at most
///                 ( ::HAPI_VolumeInfo::xLength * ::HAPI_VolumeInfo::yLength )
///                 - @p start.
///
HAPI_DECL HAPI_GetHeightFieldData( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PartId part_id,
                                   float * values_array,
                                   int start, int length );

/// @brief  Set the volume info of a geo on a geo input.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      volume_info
///                 All volume information that can be specified per
///                 volume. This includes the position, orientation, scale,
///                 data format, tuple size, and taper. The tile size is
///                 always 8x8x8.
///
HAPI_DECL HAPI_SetVolumeInfo( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              const HAPI_VolumeInfo * volume_info );

/// @brief  Set the values of a float tile: this is an 8x8x8 subsection of
///         the volume.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      tile
///                 The tile that the volume will be input into.
///
/// @param[in]      values_array
///                 The values of the individual voxel tiles in the
///                 volume. The length of this array should
///                 be ( 8 ^ 3 ) * tupleSize.
///
/// @param[in]      length
///                 The length should be ( 8 ^ 3 ) * tupleSize.
///
HAPI_DECL HAPI_SetVolumeTileFloatData( const HAPI_Session * session,
                                       HAPI_NodeId node_id,
                                       HAPI_PartId part_id,
                                       const HAPI_VolumeTileInfo * tile,
                                       const float * values_array,
                                       int length );

/// @brief  Set the values of an int tile: this is an 8x8x8 subsection of
///         the volume.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      tile
///                 The tile that the volume will be input into.
///
/// @param[in]      values_array
///                 The values of the individual voxel tiles in the
///                 volume. The length of this array should
///                 be ( 8 ^ 3 ) * tupleSize.
///
/// @param[in]      length
///                 The length should be ( 8 ^ 3 ) * tupleSize.
///
HAPI_DECL HAPI_SetVolumeTileIntData( const HAPI_Session * session,
                                     HAPI_NodeId node_id,
                                     HAPI_PartId part_id,
                                     const HAPI_VolumeTileInfo * tile,
                                     const int * values_array,
                                     int length );

/// @brief  Set the values of a float voxel in the volume.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      x_index
///                 The x index/coordinate of the voxel.
///
/// @param[in]      y_index
///                 The y index/coordinate of the voxel.
///
/// @param[in]      z_index
///                 The z index/coordinate of the voxel.
///
/// @param[in]     values_array
///                 The values of the voxel.
///
/// @param[in]      value_count
///                 Should be equal to the volume's
///                 ::HAPI_VolumeInfo::tupleSize.
///                 <!-- source ::HAPI_VolumeInfo::tupleSize -->
///
HAPI_DECL HAPI_SetVolumeVoxelFloatData( const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_PartId part_id,
                                        int x_index,
                                        int y_index,
                                        int z_index,
                                        const float * values_array,
                                        int value_count );

/// @brief  Set the values of a integer voxel in the volume.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]      x_index
///                 The x index/coordinate of the voxel.
///
/// @param[in]      y_index
///                 The y index/coordinate of the voxel.
///
/// @param[in]      z_index
///                 The z index/coordinate of the voxel.
///
/// @param[in]     values_array
///                 The values of the voxel.
///
/// @param[in]      value_count
///                 Should be equal to the volume's
///                 ::HAPI_VolumeInfo::tupleSize.
///                 <!-- source ::HAPI_VolumeInfo::tupleSize -->
///
HAPI_DECL HAPI_SetVolumeVoxelIntData(   const HAPI_Session * session,
                                        HAPI_NodeId node_id,
                                        HAPI_PartId part_id,
                                        int x_index,
                                        int y_index,
                                        int z_index,
                                        const int * values_array,
                                        int value_count );

/// @brief  Get the bounding values of a volume.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     x_min
///                 The minimum x value of the volume's bounding box.
///                 Can be null if you do not want this value.
///
/// @param[out]     y_min
///                 The minimum y value of the volume's bounding box.
///                 Can be null if you do not want this value.
///
/// @param[out]     z_min
///                 The minimum z value of the volume's bounding box.
///                 Can be null if you do not want this value.
///
/// @param[out]     x_max
///                 The maximum x value of the volume's bounding box.
///                 Can be null if you do not want this value.
///
/// @param[out]     y_max
///                 The maximum y value of the volume's bounding box.
///                 Can be null if you do not want this value.
///
/// @param[out]     z_max
///                 The maximum z value of the volume's bounding box.
///                 Can be null if you do not want this value.
///
/// @param[out]     x_center
///                 The x value of the volume's bounding box center.
///                 Can be null if you do not want this value.
///
/// @param[out]     y_center
///                 The y value of the volume's bounding box center.
///                 Can be null if you do not want this value.
///
/// @param[out]     z_center
///                 The z value of the volume's bounding box center.
///                 Can be null if you do not want this value.
///
HAPI_DECL HAPI_GetVolumeBounds( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                HAPI_PartId part_id,
                                float * x_min, float * y_min, float * z_min,
                                float * x_max, float * y_max, float * z_max,
                                float * x_center, float * y_center, float * z_center );

/// @brief  Set the height field data for a terrain volume with the values from
///         a flattened 2D array of float.
///         ::HAPI_SetVolumeInfo() should be called first to make sure that the 
///         volume and its info are initialized.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[in]     values_array
///                 Heightfield flattened array. Should be at least the size of
///                 @p start + @p length.
///
/// @param[in]      start
///                 The start at least 0 and at most
///                 ( ::HAPI_VolumeInfo::xLength * ::HAPI_VolumeInfo::yLength ) - @p length.
///
/// @param[in]      length
///                 The length should be at least 1 or at most
///                 ( ::HAPI_VolumeInfo::xLength * ::HAPI_VolumeInfo::yLength ) - @p start.
///
/// @param[in]      name
///                 The name of the volume used for the heightfield.
///                 If set to "height" the values will be used for height information,
///                 if not, the data will used as a mask.
///
HAPI_DECL HAPI_SetHeightFieldData(  const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    HAPI_PartId part_id,
                                    const char * name,
                                    const float * values_array,
                                    int start, int length );

/// @brief  Retrieve the visualization meta-data of the volume.
///
/// @ingroup Volumes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     visual_info
///                 The meta-data associated with the visualization
///                 settings of the part specified by the previous 
///                 parameters.
///
HAPI_DECL HAPI_GetVolumeVisualInfo( const HAPI_Session * session,
                                    HAPI_NodeId node_id,
                                    HAPI_PartId part_id,
                                    HAPI_VolumeVisualInfo * visual_info );

/// @defgroup Curves
/// Functions for working with curves

/// @brief  Retrieve any meta-data about the curves, including the
///         curve's type, order, and periodicity.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     info
///                 The curve info represents the meta-data about
///                 the curves, including the type, order,
///                 and periodicity.
///
HAPI_DECL HAPI_GetCurveInfo( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             HAPI_CurveInfo * info );

/// @brief  Retrieve the number of vertices for each curve in the part.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     counts_array
///                 The number of cvs each curve contains
///
/// @param[in]      start
///                 The index of the first curve.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of curves' counts to retrieve.
///
HAPI_DECL HAPI_GetCurveCounts( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               HAPI_PartId part_id,
                               int * counts_array,
                               int start, int length );

/// @brief  Retrieve the orders for each curve in the part if the
///         curve has varying order.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     orders_array
///                 The order of each curve will be returned in this
///                 array.
///
/// @param[in]      start
///                 The index of the first curve.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of curves' orders to retrieve.
///
HAPI_DECL HAPI_GetCurveOrders( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               HAPI_PartId part_id,
                               int * orders_array,
                               int start, int length );

/// @brief  Retrieve the knots of the curves in this part.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     knots_array
///                 The knots of each curve will be returned in this
///                 array.
///
/// @param[in]      start
///                 The index of the first curve.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of curves' knots to retrieve. The
///                 length of all the knots on a single curve is
///                 the order of that curve plus the number of
///                 vertices (see ::HAPI_GetCurveOrders(),
///                 and ::HAPI_GetCurveCounts()).
///
HAPI_DECL HAPI_GetCurveKnots( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              float * knots_array,
                              int start, int length );

/// @brief  Set meta-data for the curve mesh, including the
///         curve type, order, and periodicity.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      info
///                 The curve info represents the meta-data about
///                 the curves, including the type, order,
///                 and periodicity.
///
HAPI_DECL HAPI_SetCurveInfo( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             const HAPI_CurveInfo * info );

/// @brief  Set the number of vertices for each curve in the part.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      counts_array
///                 The number of cvs each curve contains.
///
/// @param[in]      start
///                 The index of the first curve.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of curves' counts to set.
///                 <!-- source arglength(counts_array) -->
///
HAPI_DECL HAPI_SetCurveCounts( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               HAPI_PartId part_id,
                               const int * counts_array,
                               int start, int length );

/// @brief  Set the orders for each curve in the part if the
///         curve has varying order.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      orders_array
///                 The orders of each curve.
///
/// @param[in]      start
///                 The index of the first curve.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of curves' orders to retrieve.
///                 <!-- source arglength(orders_array) -->
///
HAPI_DECL HAPI_SetCurveOrders( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               HAPI_PartId part_id,
                               const int * orders_array,
                               int start, int length );

/// @brief  Set the knots of the curves in this part.
///
/// @ingroup Curves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      knots_array
///                 The knots of each curve.
///
/// @param[in]      start
///                 The index of the first curve.
///                 <!-- default 0 -->
///
/// @param[in]      length
///                 The number of curves' knots to set. The
///                 length of all the knots on a single curve is
///                 the order of that curve plus the number of
///                 vertices (see ::HAPI_SetCurveOrders(),
///                 and ::HAPI_SetCurveCounts()).
///
HAPI_DECL HAPI_SetCurveKnots( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              HAPI_PartId part_id,
                              const float * knots_array,
                              int start, int length );

// INPUT CURVE INFO ---------------------------------------------------------

/// @defgroup InputCurves
/// Functions for working with curves

/// @brief  Retrieve meta-data about the input curves, including the
///         curve's type, order, and whether or not the curve is closed and reversed.
///
/// @ingroup InputCurves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 The part id.
///
/// @param[out]     info
///                 The curve info represents the meta-data about
///                 the curves, including the type, order,
///                 and closed and reversed values.
///
HAPI_DECL HAPI_GetInputCurveInfo( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             HAPI_InputCurveInfo * info );

/// @brief  Set meta-data for the input curves, including the
///         curve type, order, reverse and closed properties.
///
/// @ingroup InputCurves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      info
///                 The curve info represents the meta-data about
///                 the curves, including the type, order,
///                 and closed and reverse properties.
///
HAPI_DECL HAPI_SetInputCurveInfo( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             const HAPI_InputCurveInfo * info );

/// @brief  Sets the positions for input curves, doing checks for
/// curve validity, and adjusting the curve settings accordingly.
/// Will also cook the node.
///
/// @ingroup InputCurves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      positions_array
///                 A float array representing the positions attribute.
///                 It will read the array assuming a tuple size of 3.
///                 Note that this function does not do any coordinate axes 
///                 conversion.
/// 
/// @param[in]      start
///                 The index of the first position in positions_array.
///                 <!-- default 0 -->
/// 
/// @param[in]      length
///                 The size of the positions array.
///                 <!-- source arglength(positions_array) -->
///
HAPI_DECL HAPI_SetInputCurvePositions(
                             const HAPI_Session* session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             const float* positions_array,
                             int start,
                             int length);

/// @brief  Sets the positions for input curves, doing checks for
/// curve validity, and adjusting the curve settings accordingly.
/// Will also cook the node. Additionally, adds rotation and scale
/// attributes to the curve. 
///
/// @ingroup InputCurves
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      part_id
///                 Currently unused. Input asset geos are assumed
///                 to have only one part.
///
/// @param[in]      positions_array
///                 A float array representing the positions attribute.
///                 It will read the array assuming a tuple size of 3.
///                 Note that this function does not do any coordinate axes 
///                 conversion.
/// 
/// @param[in]      positions_array
///                 A float array representing the positions attribute.
///                 It will read the array assuming a tuple size of 3.
///                 Note that this function does not do any coordinate axes 
///                 conversion.
/// 
/// @param[in]      positions_start
///                 The index of the first position in positions_array.
///                 <!-- default 0 -->
/// 
/// @param[in]      positions_length
///                 The size of the positions array.
///                 <!-- source arglength(positions_array) -->
///
/// @param[in]      rotations_array
///                 A float array representing the rotation (rot) attribute.
///                 It will read the array assuming a tuple size of 4
///                 representing quaternion values
/// 
/// @param[in]      rotations_start
///                 The index of the first rotation in rotations_array.
///                 <!-- default 0 -->
/// 
/// @param[in]      rotations_length
///                 The size of the rotations array.
///                 <!-- source arglength(rotations_array) -->
/// 
/// @param[in]      scales_array
///                 A float array representing the scale attribute.
///                 It will read the array assuming a tuple size of 3
/// 
/// @param[in]      scales_start
///                 The index of the first scale in scales_array.
///                 <!-- default 0 -->
/// 
/// @param[in]      scales_length
///                 The size of the scales array.
///                 <!-- source arglength(scales_array) -->
///
HAPI_DECL HAPI_SetInputCurvePositionsRotationsScales(
                             const HAPI_Session* session,
                             HAPI_NodeId node_id,
                             HAPI_PartId part_id,
                             const float* positions_array,
                             int positions_start,
                             int positions_length,
                             const float* rotations_array,
                             int rotations_start,
                             int rotations_length,
                             const float * scales_array,
                             int scales_start,
                             int scales_length);

// BASIC PRIMITIVES ---------------------------------------------------------

/// @brief  Get the box info on a geo part (if the part is a box).
///
/// @ingroup Geometry
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      geo_node_id
///                 The geo node id.
///
/// @param[in]      part_id
///                 The part id of the
///
/// @param[out]     box_info
///                 The returned box info.
///
HAPI_DECL HAPI_GetBoxInfo( const HAPI_Session * session,
                           HAPI_NodeId geo_node_id,
                           HAPI_PartId part_id,
                           HAPI_BoxInfo * box_info );

/// @brief  Get the sphere info on a geo part (if the part is a sphere).
///
/// @ingroup Geometry
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      geo_node_id
///                 The geo node id.
///
/// @param[in]      part_id
///                 The part id of the
///
/// @param[out]     sphere_info
///                 The returned sphere info.
///
HAPI_DECL HAPI_GetSphereInfo( const HAPI_Session * session,
                              HAPI_NodeId geo_node_id,
                              HAPI_PartId part_id,
                              HAPI_SphereInfo * sphere_info );

/// @defgroup Caching
/// Functions for working with memory and file caches

/// @brief  Get the number of currently active caches.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     active_cache_count
///                 The number of currently active caches.
///
HAPI_DECL HAPI_GetActiveCacheCount( const HAPI_Session * session,
                                    int * active_cache_count );

/// @brief  Get the names of the currently active caches.
///
///         Requires a valid active cache count which you get from:
///         ::HAPI_GetActiveCacheCount().
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     cache_names_array
///                 String array with the returned cache names. Must be
///                 at least the size of @a active_cache_count.
///
/// @param[in]      active_cache_count
///                 The count returned by ::HAPI_GetActiveCacheCount().
///                 <!-- source ::HAPI_GetActiveCacheCount -->
///
HAPI_DECL HAPI_GetActiveCacheNames( const HAPI_Session * session,
                                    HAPI_StringHandle * cache_names_array,
                                    int active_cache_count );

/// @brief  Lets you inspect specific properties of the different memory
///         caches in the current Houdini context.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      cache_name
///                 Cache name from ::HAPI_GetActiveCacheNames().
///
/// @param[in]      cache_property
///                 The specific property of the cache to get the value for.
///
/// @param[out]     property_value
///                 Returned property value.
///
HAPI_DECL HAPI_GetCacheProperty( const HAPI_Session * session,
                                 const char * cache_name,
                                 HAPI_CacheProperty cache_property,
                                 int * property_value );

/// @brief  Lets you modify specific properties of the different memory
///         caches in the current Houdini context. This includes clearing
///         caches, reducing their memory use, or changing how memory limits
///         are respected by a cache.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      cache_name
///                 Cache name from ::HAPI_GetActiveCacheNames().
///
/// @param[in]      cache_property
///                 The specific property of the cache to modify.
///
/// @param[in]      property_value
///                 The new property value.
///
HAPI_DECL HAPI_SetCacheProperty( const HAPI_Session * session,
                                 const char * cache_name,
                                 HAPI_CacheProperty cache_property,
                                 int property_value );

/// @brief  Saves a geometry to file.  The type of file to save is
///         to be determined by the extension ie. .bgeo, .obj
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      file_name
///                 The name of the file to be saved.  The extension
///                 of the file determines its type.
///
HAPI_DECL HAPI_SaveGeoToFile( const HAPI_Session * session,
                              HAPI_NodeId node_id,
                              const char * file_name );

/// @brief  Loads a geometry file and put its contents onto a SOP
///         node.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      file_name
///                 The name of the file to be loaded
///
HAPI_DECL HAPI_LoadGeoFromFile( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                const char * file_name );

/// @brief  Saves the node and all its contents to file.
///         The saved file can be loaded by calling ::HAPI_LoadNodeFromFile.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      file_name
///                 The name of the file to be saved.  The extension
///                 of the file determines its type.
///
HAPI_DECL HAPI_SaveNodeToFile( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               const char * file_name );

/// @brief  Loads and creates a previously saved node and all 
///         its contents from given file.
///         The saved file must have been created by calling
///         ::HAPI_SaveNodeToFile.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      file_name
///                 The name of the file to be loaded
///
/// @param[in]      parent_node_id
///                 The parent node id of the Geometry object.
///
/// @param[in]      node_label
///                 The name of the new Geometry object.
///
/// @param[in]      cook_on_load
///                 Set to true if you wish the nodes to cook as soon
///                 as they are created. Otherwise, you will have to
///                 call ::HAPI_CookNode() explicitly for each after you
///                 call this function.
///
/// @param[out]     new_node_id
///                 The newly created node id.
///
HAPI_DECL HAPI_LoadNodeFromFile( const HAPI_Session * session,
                                 const char * file_name,
                                 HAPI_NodeId parent_node_id,
                                 const char * node_label,
                                 HAPI_Bool cook_on_load,
                                 HAPI_NodeId * new_node_id );

/// @brief  Cache the current state of the geo to memory, given the
///         format, and return the size. Use this size with your call
///         to ::HAPI_SaveGeoToMemory() to copy the cached geo to your
///         buffer. It is guaranteed that the size will not change between
///         your call to ::HAPI_GetGeoSize() and ::HAPI_SaveGeoToMemory().
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      format
///                 The file format, ie. ".obj", ".bgeo.sc" etc.
///
/// @param[out]     size
///                 The size of the buffer required to hold the output.
///
HAPI_DECL HAPI_GetGeoSize( const HAPI_Session * session,
                           HAPI_NodeId node_id,
                           const char * format,
                           int * size );

/// @brief  Saves the cached geometry to your buffer in memory,
///         whose format and required size is identified by the call to
///         ::HAPI_GetGeoSize(). The call to ::HAPI_GetGeoSize() is
///         required as ::HAPI_GetGeoSize() does the actual saving work.
///
///         Also note that this call to ::HAPI_SaveGeoToMemory will delete
///         the internal geo buffer that was cached in the previous call
///         to ::HAPI_GetGeoSize(). This means that you will need to call
///         ::HAPI_GetGeoSize() again before you can call this function.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     buffer
///                 The buffer we will write into.
///
/// @param[in]      length
///                 The size of the buffer passed in.
///                 <!-- source ::HAPI_GetGeoSize -->
///
HAPI_DECL HAPI_SaveGeoToMemory( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                char * buffer,
                                int length );

/// @brief  Loads a geometry from memory and put its
///         contents onto a SOP node.
///
/// @ingroup Caching
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      format
///                 The file format, ie. "obj", "bgeo" etc.
///
/// @param[in]      buffer
///                 The buffer we will read the geometry from.
///
/// @param[in]      length
///                 The size of the buffer passed in.
///                 <!-- source arglength(buffer) -->
///
HAPI_DECL HAPI_LoadGeoFromMemory( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  const char * format,
                                  const char * buffer,
                                  int length );

/// @brief  Set the specified node's display flag.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      onOff
///                 Display flag.
///
HAPI_DECL HAPI_SetNodeDisplay( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               int onOff );

/// @brief  Get the specified node's total cook count, including
///	    its children, if specified.
///
/// @ingroup Nodes
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      node_type_filter
///                 The node type by which to filter the children.
///
/// @param[in]      node_flags_filter
///                 The node flags by which to filter the children.
///
/// @param[in]      recursive
///                 Whether or not to include the specified node's
///                 children cook count in the tally.
///
/// @param[out]     count
///                 The number of cooks in total for this session.
///
HAPI_DECL HAPI_GetTotalCookCount( const HAPI_Session * session,
                                  HAPI_NodeId node_id,
                                  HAPI_NodeTypeBits node_type_filter,
                                  HAPI_NodeFlagsBits node_flags_filter,
                                  HAPI_Bool recursive,
                                  int * count );

/// @defgroup SessionSync
/// Functions for working with SessionSync

/// @brief  Enable or disable SessionSync mode.
///
/// @ingroup SessionSync
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      enable
///                 Enable or disable SessionSync mode.
///
HAPI_DECL HAPI_SetSessionSync( const HAPI_Session * session,
                               HAPI_Bool enable );

/// @brief  Get the ::HAPI_Viewport info for synchronizing viewport in
///	    SessionSync. When SessionSync is running this will
///	    return Houdini's current viewport information.
///
/// @ingroup SessionSync
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[out]     viewport
///                 The output ::HAPI_Viewport.
///
HAPI_DECL HAPI_GetViewport( const HAPI_Session * session,
                            HAPI_Viewport * viewport );

/// @brief  Set the ::HAPI_Viewport info for synchronizing viewport in
///	    SessionSync. When SessionSync is running, this can be
///	    used to set the viewport information which Houdini
///	    will then synchronizse with for its viewport.
///
/// @ingroup SessionSync
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      viewport
///                 A ::HAPI_Viewport that stores the viewport.
///
HAPI_DECL HAPI_SetViewport( const HAPI_Session * session,
                            const HAPI_Viewport * viewport );

/// @brief  Get the ::HAPI_SessionSyncInfo for synchronizing SessionSync
///	    state between Houdini and Houdini Engine integrations.
///
/// @ingroup SessionSync
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[out]     session_sync_info
///                 The output ::HAPI_SessionSyncInfo.
///
HAPI_DECL HAPI_GetSessionSyncInfo(
    const HAPI_Session * session,
    HAPI_SessionSyncInfo * session_sync_info );

/// @brief  Set the ::HAPI_SessionSyncInfo for synchronizing SessionSync
///	    state between Houdini and Houdini Engine integrations.
///
/// @ingroup SessionSync
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      session_sync_info
///                 A ::HAPI_SessionSyncInfo that stores the state.
///
HAPI_DECL HAPI_SetSessionSyncInfo(
    const HAPI_Session * session,
    const HAPI_SessionSyncInfo * session_sync_info );

/// @defgroup PDG PDG/TOPs
/// Functions for working with PDG/TOPs

/// @brief Return an array of PDG graph context names and ids, the first 
///        count names will be returned.  These ids can be used 
///        with ::HAPI_GetPDGEvents and ::HAPI_GetPDGState.  The values
///        of the names can be retrieved with ::HAPI_GetString.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[out]     num_contexts
///                 Total number of PDG graph contexts found.
///
/// @param[out]     context_names_array
///                 Array of int (string handles) to house the
///                 context names.  These handles are valid until the next
///                 call to this function.
///
/// @param[out]     context_id_array
///                 Array of graph context ids.
///
/// @param[in]      count
///                 Length of @p context_names_array and @p context_id_array
///
HAPI_DECL HAPI_GetPDGGraphContexts( const HAPI_Session * session,
                                    int * num_contexts,
                                    HAPI_StringHandle * context_names_array,
                                    HAPI_PDG_GraphContextId * context_id_array,
                                    int count );

/// @brief  Get the PDG graph context for the specified TOP node.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      top_node_id
///                 The id of the TOP node to query its graph context.
///
/// @param[out]     context_id
///                 The PDG graph context id.
///
HAPI_DECL HAPI_GetPDGGraphContextId( const HAPI_Session * session,
                                     HAPI_NodeId top_node_id,
                                     HAPI_PDG_GraphContextId * context_id );

/// @brief  Starts a PDG cooking operation.  This can be asynchronous.  
///        Progress can be checked with ::HAPI_GetPDGState() and
///        ::HAPI_GetPDGState(). Events generated during this cook can be 
///        collected with ::HAPI_GetPDGEvents(). Any uncollected events will be 
///         discarded at the start of the cook.
///
///        If there are any $HIPFILE file dependencies on nodes involved in the cook
///        a hip file will be automatically saved to $HOUDINI_TEMP_DIR directory so
///        that it can be copied to the working directory by the scheduler.  This means
///        $HIP will be equal to $HOUDINI_TEMP_DIR.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      cook_node_id
///                 The node id of a TOP node for the cook operation.
///
/// @param[in]      generate_only
///                 1 means only static graph generation will done.  0 means
///                 a full graph cook.  Generation is always blocking.
///
/// @param[in]      blocking
///                 0 means return immediately and cooking will be done 
///                 asynchronously.   1 means return when cooking completes.
///
HAPI_DECL HAPI_CookPDG( const HAPI_Session * session,
                        HAPI_NodeId cook_node_id,
                        int generate_only,
                        int blocking );

/// @brief  Starts a PDG cooking operation.  This can be asynchronous.
///        Progress can be checked with ::HAPI_GetPDGState() and
///        ::HAPI_GetPDGState(). Events generated during this cook can be
///        collected with ::HAPI_GetPDGEvents(). Any uncollected events will be
///         discarded at the start of the cook.
///
///        If there are any $HIPFILE file dependencies on nodes involved in the
///        cook a hip file will be automatically saved to $HOUDINI_TEMP_DIR
///        directory so that it can be copied to the working directory by the
///        scheduler.  This means $HIP will be equal to $HOUDINI_TEMP_DIR.
/// 
///        If cook_node_id is a network / subnet, then if it has output nodes
///        it cooks all of its output nodes and not just output 0. If it does
///        not have output nodes it cooks the node with the output flag.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///
/// @param[in]      cook_node_id
///                 The node id of a TOP node for the cook operation.
///
/// @param[in]      generate_only
///                 1 means only static graph generation will done.  0 means
///                 a full graph cook.  Generation is always blocking.
///
/// @param[in]      blocking
///                 0 means return immediately and cooking will be done
///                 asynchronously.   1 means return when cooking completes.
///
HAPI_DECL HAPI_CookPDGAllOutputs(
        const HAPI_Session* session,
        HAPI_NodeId cook_node_id,
        int generate_only,
        int blocking);

/// @brief  Returns PDG events that have been collected.  Calling this function
///         will remove those events from the queue.  Events collection is restarted
///         by calls to ::HAPI_CookPDG().
///
/// @ingroup PDG
///
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      graph_context_id
///                 The id of the graph context 
///
/// @param[out]     event_array
///                 buffer of ::HAPI_PDG_EventInfo of size at least length.
///
/// @param[in]      length
///                 The size of the buffer passed in.
///
/// @param[out]     event_count
///                 Number of events removed from queue and copied to buffer.
///
/// @param[out]     remaining_events
///                 Number of queued events remaining after this operation.
///
HAPI_DECL HAPI_GetPDGEvents( const HAPI_Session * session,
                             HAPI_PDG_GraphContextId graph_context_id,
                             HAPI_PDG_EventInfo * event_array,
                             int length,
                             int * event_count,
                             int * remaining_events );

/// @brief  Gets the state of a PDG graph
///
/// @ingroup PDG
///
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      graph_context_id
///                 The graph context id
///
/// @param[out]     pdg_state
///                 One of ::HAPI_PDG_State.
///
HAPI_DECL HAPI_GetPDGState( const HAPI_Session * session, 
                            HAPI_PDG_GraphContextId graph_context_id, 
                            int * pdg_state );


/// @brief  Creates a new pending workitem for the given node.  The workitem
///         will not be submitted to the graph until it is committed with 
///         ::HAPI_CommitWorkitems().  The node is expected to be a generator type.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     workitem_id
///                 The id of the pending workitem.
///
/// @param[in]      name
///                 The null-terminated name of the workitem.  The name will
///                 be automatically suffixed to make it unique.
///
/// @param[in]      index
///                 The index of the workitem.  The semantics of the index
///                 are user defined.
///
HAPI_DECL HAPI_CreateWorkitem( const HAPI_Session * session,
                               HAPI_NodeId node_id,
                               HAPI_PDG_WorkitemId * workitem_id,
                               const char * name,
                               int index );

/// @brief  Retrieves the info of a given workitem by id.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      graph_context_id
///                 The graph context that the workitem is in.
///
/// @param[in]      workitem_id
///                 The id of the workitem.
///
/// @param[out]     workitem_info
///                 The returned ::HAPI_PDG_WorkitemInfo for the workitem.  Note
///                 that the enclosed string handle is only valid until the next
///                 call to this function.
///
HAPI_DECL HAPI_GetWorkitemInfo( const HAPI_Session * session,
                                HAPI_PDG_GraphContextId graph_context_id,
                                HAPI_PDG_WorkitemId workitem_id,
                                HAPI_PDG_WorkitemInfo * workitem_info );

/// @brief  Adds integer data to a pending PDG workitem data member for the given node.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the pending workitem returned by ::HAPI_CreateWorkitem()
///
/// @param[in]      data_name
///                 null-terminated name of the data member
///
/// @param[in]      values_array
///                 array of integer values
///
/// @param[in]      length
///                 number of values to copy from values_array to the parameter
///
HAPI_DECL HAPI_SetWorkitemIntData( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PDG_WorkitemId workitem_id,
                                   const char * data_name,
                                   const int * values_array,
                                   int length );

/// @brief  Adds float data to a pending PDG workitem data member for the given node.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the pending workitem returned by ::HAPI_CreateWorkitem()
///
/// @param[in]      data_name
///                 null-terminated name of the workitem data member
///
/// @param[in]      values_array
///                 array of float values
///
/// @param[in]      length
///                 number of values to copy from values_array to the parameter
///
HAPI_DECL HAPI_SetWorkitemFloatData( const HAPI_Session * session,
                                     HAPI_NodeId node_id,
                                     HAPI_PDG_WorkitemId workitem_id,
                                     const char * data_name,
                                     const float * values_array,
                                     int length );

/// @brief  Adds integer data to a pending PDG workitem data member for the given node.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the created workitem returned by HAPI_CreateWorkitem()
///
/// @param[in]      data_name
///                 null-terminated name of the data member
///
/// @param[in]      data_index
///                 index of the string data member
///
/// @param[in]      value
///                 null-terminated string to copy to the workitem data member
///
HAPI_DECL HAPI_SetWorkitemStringData( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PDG_WorkitemId workitem_id,
                                      const char * data_name,
                                      int data_index,
                                      const char * value );

/// @brief  Commits any pending workitems.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id for which the pending workitems have been
///                 created but not yet injected.
///
HAPI_DECL HAPI_CommitWorkitems( const HAPI_Session * session,
                                HAPI_NodeId node_id );

/// @brief  Gets the number of workitems that are available on the given node.
///         Should be used with ::HAPI_GetWorkitems.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     num
///                 The number of workitems.
///
HAPI_DECL HAPI_GetNumWorkitems( const HAPI_Session * session,
                                HAPI_NodeId node_id,
                                int * num );

/// @brief  Gets the list of work item ids for the given node
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[out]     workitem_ids_array
///                 buffer for resulting array of ::HAPI_PDG_WorkitemId
///
/// @param[in]      length
///                 The length of the @p workitem_ids buffer
///
HAPI_DECL HAPI_GetWorkitems( const HAPI_Session * session,
                             HAPI_NodeId node_id,
                             int * workitem_ids_array, 
                             int length );

/// @brief  Gets the length of the workitem data member.
///         It is the length of the array of data.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the workitem
///
/// @param[in]      data_name
///                 null-terminated name of the data member
///
/// @param[out]     length
///                 The length of the data member array
///
HAPI_DECL HAPI_GetWorkitemDataLength( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PDG_WorkitemId workitem_id,
                                      const char * data_name,
                                      int * length );

/// @brief  Gets int data from a work item member.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the workitem
///
/// @param[in]      data_name
///                 null-terminated name of the data member
///
/// @param[out]     data_array
///                 buffer of at least size length to copy the data into.  The required
///                 length should be determined by ::HAPI_GetWorkitemDataLength().
///
/// @param[in]      length
///                 The length of @p data_array
///
HAPI_DECL HAPI_GetWorkitemIntData( const HAPI_Session * session,
                                   HAPI_NodeId node_id,
                                   HAPI_PDG_WorkitemId workitem_id,
                                   const char * data_name,
                                   int * data_array,
                                   int length );

/// @brief  Gets float data from a work item member.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the workitem
///
/// @param[in]      data_name
///                 null-terminated name of the data member
///
/// @param[out]     data_array
///                 buffer of at least size length to copy the data into.  The required
///                 length should be determined by ::HAPI_GetWorkitemDataLength().
///
/// @param[in]      length
///                 The length of the @p data_array
///
HAPI_DECL HAPI_GetWorkitemFloatData( const HAPI_Session * session,
                                     HAPI_NodeId node_id,
                                     HAPI_PDG_WorkitemId workitem_id,
                                     const char * data_name,
                                     float * data_array,
                                     int length );

/// @brief  Gets string ids from a work item member.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the workitem
///
/// @param[in]      data_name
///                 null-terminated name of the data member
///
/// @param[out]     data_array
///                 buffer of at least size length to copy the data into.  The required
///                 length should be determined by ::HAPI_GetWorkitemDataLength().
///                 The data is an array of ::HAPI_StringHandle which can be used with 
///                 ::HAPI_GetString().  The string handles are valid until the 
///                 next call to this function.
///
/// @param[in]      length
///                 The length of @p data_array
///
HAPI_DECL HAPI_GetWorkitemStringData( const HAPI_Session * session,
                                      HAPI_NodeId node_id,
                                      HAPI_PDG_WorkitemId workitem_id,
                                      const char * data_name,
                                      HAPI_StringHandle * data_array,
                                      int length );                               

/// @brief  Gets the info for workitem results.
///         The number of workitem results is found on the ::HAPI_PDG_WorkitemInfo
///         returned by ::HAPI_GetWorkitemInfo()
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      workitem_id
///                 The id of the workitem
///
/// @param[out]     resultinfo_array
///                 Buffer to fill with info structs.  String handles are valid
///                 until the next call of this function.
///
/// @param[in]      resultinfo_count
///                 The length of @p resultinfo_array
///
HAPI_DECL HAPI_GetWorkitemResultInfo( const HAPI_Session * session,
				      HAPI_NodeId node_id,
				      HAPI_PDG_WorkitemId workitem_id,
				      HAPI_PDG_WorkitemResultInfo * resultinfo_array,
				      int resultinfo_count );

/// @brief  Dirties the given node.  Cancels the cook if necessary and then
///         deletes all workitems on the node.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      node_id
///                 The node id.
///
/// @param[in]      clean_results
///                 Remove the results generated by the node.
///                 <!-- default 0 -->
///
HAPI_DECL HAPI_DirtyPDGNode( const HAPI_Session * session, 
			     HAPI_NodeId node_id,
			     HAPI_Bool clean_results );

/// @brief  Pause the PDG cooking operation.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      graph_context_id
///                 The id of the graph context
///
HAPI_DECL HAPI_PausePDGCook( const HAPI_Session * session,
                         HAPI_PDG_GraphContextId graph_context_id );

/// @brief  Cancel the PDG cooking operation.
///
/// @ingroup PDG
///
/// @param[in]      session
///                 The session of Houdini you are interacting with.
///                 See @ref HAPI_Sessions for more on sessions.
///                 Pass NULL to just use the default in-process session.
///                 <!-- default NULL -->
///
/// @param[in]      graph_context_id
///                 The id of the graph context
///
HAPI_DECL HAPI_CancelPDGCook( const HAPI_Session * session,
                            HAPI_PDG_GraphContextId graph_context_id );

#endif // __HAPI_h__
