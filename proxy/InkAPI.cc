/** @file

  Implements callin functions for TSAPI plugins.

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#ifndef TS_NO_API

// Avoid complaining about the deprecated APIs.
// #define TS_DEPRECATED

#include <stdio.h>

#include "libts.h"
#include "I_Layout.h"

#include "ts.h"
#include "InkAPIInternal.h"
#include "Log.h"
#include "URL.h"
#include "MIME.h"
#include "HTTP.h"
#include "HttpClientSession.h"
#include "HttpServerSession.h"
#include "HttpSM.h"
#include "HttpConfig.h"
#include "P_Net.h"
#include "P_UDPNet.h"
#include "P_HostDB.h"
#include "StatSystem.h"
#include "P_Cache.h"
#include "P_ClusterCache.h"
#include "I_RecCore.h"
#include "I_RecSignals.h"
#include "ProxyConfig.h"
#include "stats/CoupledStats.h"
#include "stats/Stats.h"
#include "Plugin.h"
#include "LogObject.h"
#include "LogConfig.h"
//#include "UserNameCache.h"
#include "PluginVC.h"
#include "api/ts/experimental.h"
#include "ICP.h"
#include "HttpAcceptCont.h"
#include "PluginVC.h"
#include "FetchSM.h"
#include "HttpDebugNames.h"
#include "I_AIO.h"
#include "I_Tasks.h"
#include "RemapProcessor.h"

#include "I_RecDefs.h"
#include "I_RecCore.h"


/****************************************************************
 *  IMPORTANT - READ ME
 * Any plugin using the IO Core must enter
 *   with a held mutex.  SDK 1.0, 1.1 & 2.0 did not
 *   have this restriction so we need to add a mutex
 *   to Plugin's Continuation if it trys to use the IOCore
 * Not only does the plugin have to have a mutex
 *   before entering the IO Core.  The mutex needs to be held.
 *   We now take out the mutex on each call to ensure it is
 *   held for the entire duration of the IOCore call
 ***************************************************************/

// helper macro for setting HTTPHdr data
#define SET_HTTP_HDR(_HDR, _BUF_PTR, _OBJ_PTR) \
    _HDR.m_heap = ((HdrHeapSDKHandle*) _BUF_PTR)->m_heap; \
    _HDR.m_http = (HTTPHdrImpl*) _OBJ_PTR; \
    _HDR.m_mime = _HDR.m_http->m_fields_impl;

// Globals for new librecords stats
volatile int top_stat = 0;
RecRawStatBlock *api_rsb;

// Globals for the Sessions/Transaction index registry
volatile int next_argv_index = 0;

struct _STATE_ARG_TABLE {
  char* name;
  size_t name_len;
  char* description;
} state_arg_table[HTTP_SSN_TXN_MAX_USER_ARG];


/* URL schemes */
tsapi const char *TS_URL_SCHEME_FILE;
tsapi const char *TS_URL_SCHEME_FTP;
tsapi const char *TS_URL_SCHEME_GOPHER;
tsapi const char *TS_URL_SCHEME_HTTP;
tsapi const char *TS_URL_SCHEME_HTTPS;
tsapi const char *TS_URL_SCHEME_MAILTO;
tsapi const char *TS_URL_SCHEME_NEWS;
tsapi const char *TS_URL_SCHEME_NNTP;
tsapi const char *TS_URL_SCHEME_PROSPERO;
tsapi const char *TS_URL_SCHEME_TELNET;
tsapi const char *TS_URL_SCHEME_TUNNEL;
tsapi const char *TS_URL_SCHEME_WAIS;
tsapi const char *TS_URL_SCHEME_PNM;
tsapi const char *TS_URL_SCHEME_RTSP;
tsapi const char *TS_URL_SCHEME_RTSPU;
tsapi const char *TS_URL_SCHEME_MMS;
tsapi const char *TS_URL_SCHEME_MMSU;
tsapi const char *TS_URL_SCHEME_MMST;

/* URL schemes string lengths */
tsapi int TS_URL_LEN_FILE;
tsapi int TS_URL_LEN_FTP;
tsapi int TS_URL_LEN_GOPHER;
tsapi int TS_URL_LEN_HTTP;
tsapi int TS_URL_LEN_HTTPS;
tsapi int TS_URL_LEN_MAILTO;
tsapi int TS_URL_LEN_NEWS;
tsapi int TS_URL_LEN_NNTP;
tsapi int TS_URL_LEN_PROSPERO;
tsapi int TS_URL_LEN_TELNET;
tsapi int TS_URL_LEN_TUNNEL;
tsapi int TS_URL_LEN_WAIS;
tsapi int TS_URL_LEN_PNM;
tsapi int TS_URL_LEN_RTSP;
tsapi int TS_URL_LEN_RTSPU;
tsapi int TS_URL_LEN_MMS;
tsapi int TS_URL_LEN_MMSU;
tsapi int TS_URL_LEN_MMST;

/* MIME fields */
tsapi const char *TS_MIME_FIELD_ACCEPT;
tsapi const char *TS_MIME_FIELD_ACCEPT_CHARSET;
tsapi const char *TS_MIME_FIELD_ACCEPT_ENCODING;
tsapi const char *TS_MIME_FIELD_ACCEPT_LANGUAGE;
tsapi const char *TS_MIME_FIELD_ACCEPT_RANGES;
tsapi const char *TS_MIME_FIELD_AGE;
tsapi const char *TS_MIME_FIELD_ALLOW;
tsapi const char *TS_MIME_FIELD_APPROVED;
tsapi const char *TS_MIME_FIELD_AUTHORIZATION;
tsapi const char *TS_MIME_FIELD_BYTES;
tsapi const char *TS_MIME_FIELD_CACHE_CONTROL;
tsapi const char *TS_MIME_FIELD_CLIENT_IP;
tsapi const char *TS_MIME_FIELD_CONNECTION;
tsapi const char *TS_MIME_FIELD_CONTENT_BASE;
tsapi const char *TS_MIME_FIELD_CONTENT_ENCODING;
tsapi const char *TS_MIME_FIELD_CONTENT_LANGUAGE;
tsapi const char *TS_MIME_FIELD_CONTENT_LENGTH;
tsapi const char *TS_MIME_FIELD_CONTENT_LOCATION;
tsapi const char *TS_MIME_FIELD_CONTENT_MD5;
tsapi const char *TS_MIME_FIELD_CONTENT_RANGE;
tsapi const char *TS_MIME_FIELD_CONTENT_TYPE;
tsapi const char *TS_MIME_FIELD_CONTROL;
tsapi const char *TS_MIME_FIELD_COOKIE;
tsapi const char *TS_MIME_FIELD_DATE;
tsapi const char *TS_MIME_FIELD_DISTRIBUTION;
tsapi const char *TS_MIME_FIELD_ETAG;
tsapi const char *TS_MIME_FIELD_EXPECT;
tsapi const char *TS_MIME_FIELD_EXPIRES;
tsapi const char *TS_MIME_FIELD_FOLLOWUP_TO;
tsapi const char *TS_MIME_FIELD_FROM;
tsapi const char *TS_MIME_FIELD_HOST;
tsapi const char *TS_MIME_FIELD_IF_MATCH;
tsapi const char *TS_MIME_FIELD_IF_MODIFIED_SINCE;
tsapi const char *TS_MIME_FIELD_IF_NONE_MATCH;
tsapi const char *TS_MIME_FIELD_IF_RANGE;
tsapi const char *TS_MIME_FIELD_IF_UNMODIFIED_SINCE;
tsapi const char *TS_MIME_FIELD_KEEP_ALIVE;
tsapi const char *TS_MIME_FIELD_KEYWORDS;
tsapi const char *TS_MIME_FIELD_LAST_MODIFIED;
tsapi const char *TS_MIME_FIELD_LINES;
tsapi const char *TS_MIME_FIELD_LOCATION;
tsapi const char *TS_MIME_FIELD_MAX_FORWARDS;
tsapi const char *TS_MIME_FIELD_MESSAGE_ID;
tsapi const char *TS_MIME_FIELD_NEWSGROUPS;
tsapi const char *TS_MIME_FIELD_ORGANIZATION;
tsapi const char *TS_MIME_FIELD_PATH;
tsapi const char *TS_MIME_FIELD_PRAGMA;
tsapi const char *TS_MIME_FIELD_PROXY_AUTHENTICATE;
tsapi const char *TS_MIME_FIELD_PROXY_AUTHORIZATION;
tsapi const char *TS_MIME_FIELD_PROXY_CONNECTION;
tsapi const char *TS_MIME_FIELD_PUBLIC;
tsapi const char *TS_MIME_FIELD_RANGE;
tsapi const char *TS_MIME_FIELD_REFERENCES;
tsapi const char *TS_MIME_FIELD_REFERER;
tsapi const char *TS_MIME_FIELD_REPLY_TO;
tsapi const char *TS_MIME_FIELD_RETRY_AFTER;
tsapi const char *TS_MIME_FIELD_SENDER;
tsapi const char *TS_MIME_FIELD_SERVER;
tsapi const char *TS_MIME_FIELD_SET_COOKIE;
tsapi const char *TS_MIME_FIELD_SUBJECT;
tsapi const char *TS_MIME_FIELD_SUMMARY;
tsapi const char *TS_MIME_FIELD_TE;
tsapi const char *TS_MIME_FIELD_TRANSFER_ENCODING;
tsapi const char *TS_MIME_FIELD_UPGRADE;
tsapi const char *TS_MIME_FIELD_USER_AGENT;
tsapi const char *TS_MIME_FIELD_VARY;
tsapi const char *TS_MIME_FIELD_VIA;
tsapi const char *TS_MIME_FIELD_WARNING;
tsapi const char *TS_MIME_FIELD_WWW_AUTHENTICATE;
tsapi const char *TS_MIME_FIELD_XREF;
tsapi const char *TS_MIME_FIELD_X_FORWARDED_FOR;

/* MIME fields string lengths */
tsapi int TS_MIME_LEN_ACCEPT;
tsapi int TS_MIME_LEN_ACCEPT_CHARSET;
tsapi int TS_MIME_LEN_ACCEPT_ENCODING;
tsapi int TS_MIME_LEN_ACCEPT_LANGUAGE;
tsapi int TS_MIME_LEN_ACCEPT_RANGES;
tsapi int TS_MIME_LEN_AGE;
tsapi int TS_MIME_LEN_ALLOW;
tsapi int TS_MIME_LEN_APPROVED;
tsapi int TS_MIME_LEN_AUTHORIZATION;
tsapi int TS_MIME_LEN_BYTES;
tsapi int TS_MIME_LEN_CACHE_CONTROL;
tsapi int TS_MIME_LEN_CLIENT_IP;
tsapi int TS_MIME_LEN_CONNECTION;
tsapi int TS_MIME_LEN_CONTENT_BASE;
tsapi int TS_MIME_LEN_CONTENT_ENCODING;
tsapi int TS_MIME_LEN_CONTENT_LANGUAGE;
tsapi int TS_MIME_LEN_CONTENT_LENGTH;
tsapi int TS_MIME_LEN_CONTENT_LOCATION;
tsapi int TS_MIME_LEN_CONTENT_MD5;
tsapi int TS_MIME_LEN_CONTENT_RANGE;
tsapi int TS_MIME_LEN_CONTENT_TYPE;
tsapi int TS_MIME_LEN_CONTROL;
tsapi int TS_MIME_LEN_COOKIE;
tsapi int TS_MIME_LEN_DATE;
tsapi int TS_MIME_LEN_DISTRIBUTION;
tsapi int TS_MIME_LEN_ETAG;
tsapi int TS_MIME_LEN_EXPECT;
tsapi int TS_MIME_LEN_EXPIRES;
tsapi int TS_MIME_LEN_FOLLOWUP_TO;
tsapi int TS_MIME_LEN_FROM;
tsapi int TS_MIME_LEN_HOST;
tsapi int TS_MIME_LEN_IF_MATCH;
tsapi int TS_MIME_LEN_IF_MODIFIED_SINCE;
tsapi int TS_MIME_LEN_IF_NONE_MATCH;
tsapi int TS_MIME_LEN_IF_RANGE;
tsapi int TS_MIME_LEN_IF_UNMODIFIED_SINCE;
tsapi int TS_MIME_LEN_KEEP_ALIVE;
tsapi int TS_MIME_LEN_KEYWORDS;
tsapi int TS_MIME_LEN_LAST_MODIFIED;
tsapi int TS_MIME_LEN_LINES;
tsapi int TS_MIME_LEN_LOCATION;
tsapi int TS_MIME_LEN_MAX_FORWARDS;
tsapi int TS_MIME_LEN_MESSAGE_ID;
tsapi int TS_MIME_LEN_NEWSGROUPS;
tsapi int TS_MIME_LEN_ORGANIZATION;
tsapi int TS_MIME_LEN_PATH;
tsapi int TS_MIME_LEN_PRAGMA;
tsapi int TS_MIME_LEN_PROXY_AUTHENTICATE;
tsapi int TS_MIME_LEN_PROXY_AUTHORIZATION;
tsapi int TS_MIME_LEN_PROXY_CONNECTION;
tsapi int TS_MIME_LEN_PUBLIC;
tsapi int TS_MIME_LEN_RANGE;
tsapi int TS_MIME_LEN_REFERENCES;
tsapi int TS_MIME_LEN_REFERER;
tsapi int TS_MIME_LEN_REPLY_TO;
tsapi int TS_MIME_LEN_RETRY_AFTER;
tsapi int TS_MIME_LEN_SENDER;
tsapi int TS_MIME_LEN_SERVER;
tsapi int TS_MIME_LEN_SET_COOKIE;
tsapi int TS_MIME_LEN_SUBJECT;
tsapi int TS_MIME_LEN_SUMMARY;
tsapi int TS_MIME_LEN_TE;
tsapi int TS_MIME_LEN_TRANSFER_ENCODING;
tsapi int TS_MIME_LEN_UPGRADE;
tsapi int TS_MIME_LEN_USER_AGENT;
tsapi int TS_MIME_LEN_VARY;
tsapi int TS_MIME_LEN_VIA;
tsapi int TS_MIME_LEN_WARNING;
tsapi int TS_MIME_LEN_WWW_AUTHENTICATE;
tsapi int TS_MIME_LEN_XREF;
tsapi int TS_MIME_LEN_OWNER_LEFT_TIME;
tsapi int TS_MIME_LEN_X_FORWARDED_FOR;


/* HTTP miscellaneous values */
tsapi const char *TS_HTTP_VALUE_BYTES;
tsapi const char *TS_HTTP_VALUE_CHUNKED;
tsapi const char *TS_HTTP_VALUE_CLOSE;
tsapi const char *TS_HTTP_VALUE_COMPRESS;
tsapi const char *TS_HTTP_VALUE_DEFLATE;
tsapi const char *TS_HTTP_VALUE_GZIP;
tsapi const char *TS_HTTP_VALUE_IDENTITY;
tsapi const char *TS_HTTP_VALUE_KEEP_ALIVE;
tsapi const char *TS_HTTP_VALUE_MAX_AGE;
tsapi const char *TS_HTTP_VALUE_MAX_STALE;
tsapi const char *TS_HTTP_VALUE_MIN_FRESH;
tsapi const char *TS_HTTP_VALUE_MUST_REVALIDATE;
tsapi const char *TS_HTTP_VALUE_NONE;
tsapi const char *TS_HTTP_VALUE_NO_CACHE;
tsapi const char *TS_HTTP_VALUE_NO_STORE;
tsapi const char *TS_HTTP_VALUE_NO_TRANSFORM;
tsapi const char *TS_HTTP_VALUE_ONLY_IF_CACHED;
tsapi const char *TS_HTTP_VALUE_PRIVATE;
tsapi const char *TS_HTTP_VALUE_PROXY_REVALIDATE;
tsapi const char *TS_HTTP_VALUE_PUBLIC;
tsapi const char *TS_HTTP_VALUE_S_MAXAGE;

/* HTTP miscellaneous values string lengths */
tsapi int TS_HTTP_LEN_BYTES;
tsapi int TS_HTTP_LEN_CHUNKED;
tsapi int TS_HTTP_LEN_CLOSE;
tsapi int TS_HTTP_LEN_COMPRESS;
tsapi int TS_HTTP_LEN_DEFLATE;
tsapi int TS_HTTP_LEN_GZIP;
tsapi int TS_HTTP_LEN_IDENTITY;
tsapi int TS_HTTP_LEN_KEEP_ALIVE;
tsapi int TS_HTTP_LEN_MAX_AGE;
tsapi int TS_HTTP_LEN_MAX_STALE;
tsapi int TS_HTTP_LEN_MIN_FRESH;
tsapi int TS_HTTP_LEN_MUST_REVALIDATE;
tsapi int TS_HTTP_LEN_NONE;
tsapi int TS_HTTP_LEN_NO_CACHE;
tsapi int TS_HTTP_LEN_NO_STORE;
tsapi int TS_HTTP_LEN_NO_TRANSFORM;
tsapi int TS_HTTP_LEN_ONLY_IF_CACHED;
tsapi int TS_HTTP_LEN_PRIVATE;
tsapi int TS_HTTP_LEN_PROXY_REVALIDATE;
tsapi int TS_HTTP_LEN_PUBLIC;
tsapi int TS_HTTP_LEN_S_MAXAGE;

/* HTTP methods */
tsapi const char *TS_HTTP_METHOD_CONNECT;
tsapi const char *TS_HTTP_METHOD_DELETE;
tsapi const char *TS_HTTP_METHOD_GET;
tsapi const char *TS_HTTP_METHOD_HEAD;
tsapi const char *TS_HTTP_METHOD_ICP_QUERY;
tsapi const char *TS_HTTP_METHOD_OPTIONS;
tsapi const char *TS_HTTP_METHOD_POST;
tsapi const char *TS_HTTP_METHOD_PURGE;
tsapi const char *TS_HTTP_METHOD_PUT;
tsapi const char *TS_HTTP_METHOD_TRACE;

/* HTTP methods string lengths */
tsapi int TS_HTTP_LEN_CONNECT;
tsapi int TS_HTTP_LEN_DELETE;
tsapi int TS_HTTP_LEN_GET;
tsapi int TS_HTTP_LEN_HEAD;
tsapi int TS_HTTP_LEN_ICP_QUERY;
tsapi int TS_HTTP_LEN_OPTIONS;
tsapi int TS_HTTP_LEN_POST;
tsapi int TS_HTTP_LEN_PURGE;
tsapi int TS_HTTP_LEN_PUT;
tsapi int TS_HTTP_LEN_TRACE;

/* TLS Next Protocol well-known protocol names. */

tsapi const char * TS_NPN_PROTOCOL_HTTP_1_0 = "http/1.0";
tsapi const char * TS_NPN_PROTOCOL_HTTP_1_1 = "http/1.1";
tsapi const char * TS_NPN_PROTOCOL_SPDY_1   = "spdy/1";   // obsolete
tsapi const char * TS_NPN_PROTOCOL_SPDY_2   = "spdy/2";   // shipping
tsapi const char * TS_NPN_PROTOCOL_SPDY_3   = "spdy/3";   // upcoming

/* MLoc Constants */
tsapi const TSMLoc TS_NULL_MLOC = (TSMLoc)NULL;

HttpAPIHooks *http_global_hooks = NULL;
ConfigUpdateCbTable *global_config_cbs = NULL;

static char traffic_server_version[128] = "";
static int ts_major_version = 0;
static int ts_minor_version = 0;
static int ts_patch_version = 0;

static ClassAllocator<APIHook> apiHookAllocator("apiHookAllocator");
static ClassAllocator<INKContInternal> INKContAllocator("INKContAllocator");
static ClassAllocator<INKVConnInternal> INKVConnAllocator("INKVConnAllocator");
static ClassAllocator<MIMEFieldSDKHandle> mHandleAllocator("MIMEFieldSDKHandle");


////////////////////////////////////////////////////////////////////
//
// API error logging
//
////////////////////////////////////////////////////////////////////
void
TSError(const char *fmt, ...)
{
  va_list args;

  if (is_action_tag_set("deft") || is_action_tag_set("sdk_vbos_errors")) {
    va_start(args, fmt);
    diags->print_va(NULL, DL_Error, NULL, fmt, args);
    va_end(args);
  }
  va_start(args, fmt);
  Log::va_error((char *) fmt, args);
  va_end(args);
}

// Assert in debug AND optim
int
_TSReleaseAssert(const char *text, const char *file, int line)
{
  _ink_assert(text, file, line);
  return 0;
}

// Assert only in debug
int
_TSAssert(const char *text, const char *file, int line)
{
#ifdef DEBUG
  _ink_assert(text, file, line);
#else
  NOWARN_UNUSED(text);
  NOWARN_UNUSED(file);
  NOWARN_UNUSED(line);
#endif

  return 0;
}

// This assert is for internal API use only.
#if TS_USE_FAST_SDK
#define sdk_assert(EX) (void)(EX)
#else
#define sdk_assert(EX)                                          \
  (void)((EX) || (_TSReleaseAssert(#EX, __FILE__, __LINE__)))
#endif


////////////////////////////////////////////////////////////////////
//
// SDK Interoperability Support
//
// ----------------------------------------------------------------
//
// Standalone Fields (SDK Version-Interoperability Hack)
//
//
// A "standalone" field is an ugly hack for portability with old
// versions of the SDK that mirrored the old header system.  In
// the old system, you could create arbitrary tiny little field
// objects, distinct from MIME header objects, and link them
// together.  In the new header system, all fields are internal
// constituents of the MIME header.  To preserve the semantics of
// the old SDK, we need to maintain the concept of fields that
// are created outside of a MIME header.  Whenever a field is
// "attached" to a MIME header, it is copied into the MIME header
// field's slot, and the handle to the field is updated to refer
// to the new field.
//
// Hopefully, we can eliminate this old compatibility interface and
// migrate users to the newer semantics quickly.
//
// ----------------------------------------------------------------
//
// MIMEField SDK Handles (SDK Version-Interoperability Hack)
//
// MIMEField "handles" are used by the SDK as an indirect reference
// to the MIMEField.  Because versions 1 & 2 of the SDK allowed
// standalone fields that existed without associated MIME headers,
// and because the version 3 SDK requires an associated MIME header
// for all field mutation operations (for presence bits, etc.) we
// need a data structure that:
//
//   * identifies standalone fields and stores field name/value
//     information for fields that are not yet in a header
//   * redirects the field to a real header field when the field
//     is inserted into a header
//   * maintains the associated MIMEHdrImpl when returning field
//     slots from lookup and create functions
//
// If the MIMEHdrImpl pointer is NULL, then the handle points
// to a standalone field, otherwise the handle points to a field
// within the MIME header.
//
////////////////////////////////////////////////////////////////////


/*****************************************************************/
/* Handles to headers are impls, but need to handle MIME or HTTP */
/*****************************************************************/

inline MIMEHdrImpl *
_hdr_obj_to_mime_hdr_impl(HdrHeapObjImpl * obj)
{
  MIMEHdrImpl *impl;
  if (obj->m_type == HDR_HEAP_OBJ_HTTP_HEADER)
    impl = ((HTTPHdrImpl *) obj)->m_fields_impl;
  else if (obj->m_type == HDR_HEAP_OBJ_MIME_HEADER)
    impl = (MIMEHdrImpl *) obj;
  else {
    ink_release_assert(!"mloc not a header type");
    impl = NULL;                /* gcc does not know about 'ink_release_assert' - make him happy */
  }
  return impl;
}

inline MIMEHdrImpl *
_hdr_mloc_to_mime_hdr_impl(TSMLoc mloc)
{
  return _hdr_obj_to_mime_hdr_impl((HdrHeapObjImpl *) mloc);
}

TSReturnCode
sdk_sanity_check_field_handle(TSMLoc field, TSMLoc parent_hdr = NULL)
{
  if (field == TS_NULL_MLOC)
    return TS_ERROR;

  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field;
  if (field_handle->m_type != HDR_HEAP_OBJ_FIELD_SDK_HANDLE)
    return TS_ERROR;

  if (parent_hdr != NULL) {
    MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(parent_hdr);
    if (field_handle->mh != mh)
      return TS_ERROR;
  }
  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_mbuffer(TSMBuffer bufp)
{
  HdrHeapSDKHandle *handle = (HdrHeapSDKHandle *) bufp;
  if ((handle == NULL) || (handle->m_heap == NULL) || (handle->m_heap->m_magic != HDR_BUF_MAGIC_ALIVE))
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_mime_hdr_handle(TSMLoc field)
{
  if (field == TS_NULL_MLOC)
    return TS_ERROR;

  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field;
  if (field_handle->m_type != HDR_HEAP_OBJ_MIME_HEADER)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_url_handle(TSMLoc field)
{
  if (field == TS_NULL_MLOC)
    return TS_ERROR;

  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field;
  if (field_handle->m_type != HDR_HEAP_OBJ_URL)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_http_hdr_handle(TSMLoc field)
{
  if (field == TS_NULL_MLOC)
    return TS_ERROR;

  HTTPHdrImpl *field_handle = (HTTPHdrImpl *) field;
  if (field_handle->m_type != HDR_HEAP_OBJ_HTTP_HEADER)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_continuation(TSCont cont)
{
  if ((cont == NULL) || (((INKContInternal *) cont)->m_free_magic == INKCONT_INTERN_MAGIC_DEAD))
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_fetch_sm(TSFetchSM fetch_sm)
{
  if (fetch_sm == NULL)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_http_ssn(TSHttpSsn ssnp)
{
  if (ssnp == NULL)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_txn(TSHttpTxn txnp)
{
  if ((txnp != NULL) && (((HttpSM *) txnp)->magic == HTTP_SM_MAGIC_ALIVE))
    return TS_SUCCESS;
  return TS_ERROR;
}

TSReturnCode
sdk_sanity_check_mime_parser(TSMimeParser parser)
{
  if (parser == NULL)
    return TS_ERROR;
  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_http_parser(TSHttpParser parser)
{
  if (parser == NULL)
    return TS_ERROR;
  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_alt_info(TSHttpAltInfo info)
{
  if (info == NULL)
    return TS_ERROR;
  return TS_SUCCESS;
}

TSReturnCode
sdk_sanity_check_hook_id(TSHttpHookID id)
{
  if (id<TS_HTTP_READ_REQUEST_HDR_HOOK || id> TS_HTTP_LAST_HOOK)
    return TS_ERROR;
  return TS_SUCCESS;
}


TSReturnCode
sdk_sanity_check_null_ptr(void *ptr)
{
  if (ptr == NULL)
    return TS_ERROR;
  return TS_SUCCESS;
}

/**
  The function checks if the buffer is Modifiable and returns true if
  it is modifiable, else returns false.

*/
bool
isWriteable(TSMBuffer bufp)
{
  if (bufp != NULL) {
    return ((HdrHeapSDKHandle *) bufp)->m_heap->m_writeable;
  }
  return false;
}


/******************************************************/
/* Allocators for field handles and standalone fields */
/******************************************************/
static MIMEFieldSDKHandle *
sdk_alloc_field_handle(TSMBuffer bufp, MIMEHdrImpl *mh)
{
  MIMEFieldSDKHandle *handle = mHandleAllocator.alloc();

  // TODO: Should remove this when memory allocation can't fail.
  sdk_assert(sdk_sanity_check_null_ptr((void*)handle) == TS_SUCCESS);

  obj_init_header(handle, HDR_HEAP_OBJ_FIELD_SDK_HANDLE, sizeof(MIMEFieldSDKHandle), 0);
  handle->mh = mh;

  return handle;
}

static void
sdk_free_field_handle(TSMBuffer bufp, MIMEFieldSDKHandle *field_handle)
{
  if (sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS) {
    mHandleAllocator.free(field_handle);
  }
}


////////////////////////////////////////////////////////////////////
//
// FileImpl
//
////////////////////////////////////////////////////////////////////
FileImpl::FileImpl()
  : m_fd(-1), m_mode(CLOSED), m_buf(NULL), m_bufsize(0), m_bufpos(0)
{ }

FileImpl::~FileImpl()
{
  fclose();
}

int
FileImpl::fopen(const char *filename, const char *mode)
{
  if (mode[0] == '\0') {
    return 0;
  } else if (mode[0] == 'r') {
    if (mode[1] != '\0') {
      return 0;
    }
    m_mode = READ;
    m_fd = open(filename, O_RDONLY | _O_ATTRIB_NORMAL);
  } else if (mode[0] == 'w') {
    if (mode[1] != '\0') {
      return 0;
    }
    m_mode = WRITE;
    m_fd = open(filename, O_WRONLY | O_CREAT | _O_ATTRIB_NORMAL, 0644);
  } else if (mode[0] == 'a') {
    if (mode[1] != '\0') {
      return 0;
    }
    m_mode = WRITE;
    m_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND | _O_ATTRIB_NORMAL, 0644);
  }

  if (m_fd < 0) {
    m_mode = CLOSED;
    return 0;
  } else {
    return 1;
  }
}

void
FileImpl::fclose()
{
  if (m_fd != -1) {
    fflush();

    close(m_fd);
    m_fd = -1;
    m_mode = CLOSED;
  }

  if (m_buf) {
    ats_free(m_buf);
    m_buf = NULL;
    m_bufsize = 0;
    m_bufpos = 0;
  }
}

int
FileImpl::fread(void *buf, int length)
{
  int64_t amount;
  int64_t err;

  if ((m_mode != READ) || (m_fd == -1)) {
    return -1;
  }

  if (!m_buf) {
    m_bufpos = 0;
    m_bufsize = 1024;
    m_buf = (char *)ats_malloc(m_bufsize);
  }

  if (m_bufpos < length) {
    amount = length;
    if (amount < 1024) {
      amount = 1024;
    }
    if (amount > (m_bufsize - m_bufpos)) {
      while (amount > (m_bufsize - m_bufpos)) {
        m_bufsize *= 2;
      }
      m_buf = (char *)ats_realloc(m_buf, m_bufsize);
    }

    do {
      err = read(m_fd, &m_buf[m_bufpos], amount);
    } while ((err < 0) && (errno == EINTR));

    if (err < 0) {
      return -1;
    }

    m_bufpos += err;
  }

  if (buf) {
    amount = length;
    if (amount > m_bufpos) {
      amount = m_bufpos;
    }
    memcpy(buf, m_buf, amount);
    memmove(m_buf, &m_buf[amount], m_bufpos - amount);
    m_bufpos -= amount;
    return amount;
  } else {
    return m_bufpos;
  }
}

int
FileImpl::fwrite(const void *buf, int length)
{
  const char *p, *e;
  int64_t avail;

  if ((m_mode != WRITE) || (m_fd == -1)) {
    return -1;
  }

  if (!m_buf) {
    m_bufpos = 0;
    m_bufsize = 1024;
    m_buf = (char *)ats_malloc(m_bufsize);
  }

  p = (const char *) buf;
  e = p + length;

  while (p != e) {
    avail = m_bufsize - m_bufpos;
    if (avail > length) {
      avail = length;
    }
    memcpy(&m_buf[m_bufpos], p, avail);

    m_bufpos += avail;
    p += avail;
    length -= avail;

    if ((length > 0) && (m_bufpos > 0)) {
      if (fflush() <= 0) {
        break;
      }
    }
  }

  return (p - (const char *) buf);
}

int
FileImpl::fflush()
{
  char *p, *e;
  int err = 0;

  if ((m_mode != WRITE) || (m_fd == -1)) {
    return -1;
  }

  if (m_buf) {
    p = m_buf;
    e = &m_buf[m_bufpos];

    while (p != e) {
      do {
        err = write(m_fd, p, e - p);
      } while ((err < 0) && (errno == EINTR));

      if (err < 0) {
        break;
      }

      p += err;
    }

    err = p - m_buf;
    memmove(m_buf, &m_buf[err], m_bufpos - err);
    m_bufpos -= err;
  }

  return err;
}

char *
FileImpl::fgets(char *buf, int length)
{
  char *e;
  int pos;

  if (length == 0) {
    return NULL;
  }

  if (!m_buf || (m_bufpos < (length - 1))) {
    pos = m_bufpos;

    fread(NULL, length - 1);

    if (!m_bufpos && (pos == m_bufpos)) {
      return NULL;
    }
  }

  e = (char *) memchr(m_buf, '\n', m_bufpos);
  if (e) {
    e += 1;
    if (length > (e - m_buf + 1)) {
      length = e - m_buf + 1;
    }
  }

  pos = fread(buf, length - 1);
  buf[pos] = '\0';

  return buf;
}

////////////////////////////////////////////////////////////////////
//
// INKContInternal
//
////////////////////////////////////////////////////////////////////

INKContInternal::INKContInternal()
  : DummyVConnection(NULL), mdata(NULL), m_event_func(NULL), m_event_count(0), m_closed(1), m_deletable(0),
    m_deleted(0), m_free_magic(INKCONT_INTERN_MAGIC_ALIVE)
{ }

INKContInternal::INKContInternal(TSEventFunc funcp, TSMutex mutexp)
  : DummyVConnection((ProxyMutex *) mutexp),
    mdata(NULL), m_event_func(funcp), m_event_count(0), m_closed(1), m_deletable(0), m_deleted(0),
    m_free_magic(INKCONT_INTERN_MAGIC_ALIVE)
{
  SET_HANDLER(&INKContInternal::handle_event);
}

void
INKContInternal::init(TSEventFunc funcp, TSMutex mutexp)
{
  SET_HANDLER(&INKContInternal::handle_event);

  mutex = (ProxyMutex *) mutexp;
  m_event_func = funcp;
}

void
INKContInternal::destroy()
{
  if (m_free_magic == INKCONT_INTERN_MAGIC_DEAD) {
    ink_release_assert(!"Plugin tries to use a continuation which is deleted");
  }
  m_deleted = 1;
  if (m_deletable) {
    this->mutex = NULL;
    m_free_magic = INKCONT_INTERN_MAGIC_DEAD;
    INKContAllocator.free(this);
  } else {
    // TODO: Should this schedule on some other "thread" ?
    // TODO: we don't care about the return action?
    TSContSchedule((TSCont)this, 0, TS_THREAD_POOL_DEFAULT);
  }
}

void
INKContInternal::handle_event_count(int event)
{
  if ((event == EVENT_IMMEDIATE) || (event == EVENT_INTERVAL)) {
    int val;

    m_deletable = (m_closed != 0);

    val = ink_atomic_increment((int *) &m_event_count, -1);
    if (val <= 0) {
      ink_assert(!"not reached");
    }

    m_deletable = m_deletable && (val == 1);
  }
}

int
INKContInternal::handle_event(int event, void *edata)
{
  if (m_free_magic == INKCONT_INTERN_MAGIC_DEAD) {
    ink_release_assert(!"Plugin tries to use a continuation which is deleted");
  }
  handle_event_count(event);
  if (m_deleted) {
    if (m_deletable) {
      this->mutex = NULL;
      m_free_magic = INKCONT_INTERN_MAGIC_DEAD;
      INKContAllocator.free(this);
    }
  } else {
    return m_event_func((TSCont) this, (TSEvent) event, edata);
  }
  return EVENT_DONE;
}


////////////////////////////////////////////////////////////////////
//
// INKVConnInternal
//
////////////////////////////////////////////////////////////////////

INKVConnInternal::INKVConnInternal()
:INKContInternal(), m_read_vio(), m_write_vio(), m_output_vc(NULL)
{
  m_closed = 0;
}

INKVConnInternal::INKVConnInternal(TSEventFunc funcp, TSMutex mutexp)
:INKContInternal(funcp, mutexp), m_read_vio(), m_write_vio(), m_output_vc(NULL)
{
  m_closed = 0;
  SET_HANDLER(&INKVConnInternal::handle_event);
}

void
INKVConnInternal::init(TSEventFunc funcp, TSMutex mutexp)
{
  INKContInternal::init(funcp, mutexp);
  SET_HANDLER(&INKVConnInternal::handle_event);
}

void
INKVConnInternal::destroy()
{
  m_deleted = 1;
  if (m_deletable) {
    this->mutex = NULL;
    m_read_vio.set_continuation(NULL);
    m_write_vio.set_continuation(NULL);
    INKVConnAllocator.free(this);
  }
}

int
INKVConnInternal::handle_event(int event, void *edata)
{
  handle_event_count(event);
  if (m_deleted) {
    if (m_deletable) {
      this->mutex = NULL;
      m_read_vio.set_continuation(NULL);
      m_write_vio.set_continuation(NULL);
      INKVConnAllocator.free(this);
    }
  } else {
    return m_event_func((TSCont) this, (TSEvent) event, edata);
  }
  return EVENT_DONE;
}

VIO *
INKVConnInternal::do_io_read(Continuation *c, int64_t nbytes, MIOBuffer *buf)
{
  m_read_vio.buffer.writer_for(buf);
  m_read_vio.op = VIO::READ;
  m_read_vio.set_continuation(c);
  m_read_vio.nbytes = nbytes;
  m_read_vio.ndone = 0;
  m_read_vio.vc_server = this;

  if (ink_atomic_increment((int *) &m_event_count, 1) < 0) {
    ink_assert(!"not reached");
  }
  eventProcessor.schedule_imm(this, ET_NET);

  return &m_read_vio;
}

VIO *
INKVConnInternal::do_io_write(Continuation *c, int64_t nbytes, IOBufferReader *buf, bool owner)
{
  ink_assert(!owner);
  m_write_vio.buffer.reader_for(buf);
  m_write_vio.op = VIO::WRITE;
  m_write_vio.set_continuation(c);
  m_write_vio.nbytes = nbytes;
  m_write_vio.ndone = 0;
  m_write_vio.vc_server = this;

  if (m_write_vio.buffer.reader()->read_avail() > 0) {
    if (ink_atomic_increment((int *) &m_event_count, 1) < 0) {
      ink_assert(!"not reached");
    }
    eventProcessor.schedule_imm(this, ET_NET);
  }

  return &m_write_vio;
}

void
INKVConnInternal::do_io_transform(VConnection *vc)
{
  m_output_vc = vc;
}

void
INKVConnInternal::do_io_close(int error)
{
  if (ink_atomic_increment((int *) &m_event_count, 1) < 0) {
    ink_assert(!"not reached");
  }

  INK_WRITE_MEMORY_BARRIER;

  if (error != -1) {
    lerrno = error;
    m_closed = TS_VC_CLOSE_ABORT;
  } else {
    m_closed = TS_VC_CLOSE_NORMAL;
  }

  m_read_vio.op = VIO::NONE;
  m_read_vio.buffer.clear();

  m_write_vio.op = VIO::NONE;
  m_write_vio.buffer.clear();

  if (m_output_vc) {
    m_output_vc->do_io_close(error);
  }

  eventProcessor.schedule_imm(this, ET_NET);
}

void
INKVConnInternal::do_io_shutdown(ShutdownHowTo_t howto)
{
  if ((howto == IO_SHUTDOWN_READ) || (howto == IO_SHUTDOWN_READWRITE)) {
    m_read_vio.op = VIO::NONE;
    m_read_vio.buffer.clear();
  }

  if ((howto == IO_SHUTDOWN_WRITE) || (howto == IO_SHUTDOWN_READWRITE)) {
    m_write_vio.op = VIO::NONE;
    m_write_vio.buffer.clear();
  }

  if (ink_atomic_increment((int *) &m_event_count, 1) < 0) {
    ink_assert(!"not reached");
  }
  eventProcessor.schedule_imm(this, ET_NET);
}

void
INKVConnInternal::reenable(VIO *vio)
{
  NOWARN_UNUSED(vio);
  if (ink_atomic_increment((int *) &m_event_count, 1) < 0) {
    ink_assert(!"not reached");
  }
  eventProcessor.schedule_imm(this, ET_NET);
}

void
INKVConnInternal::retry(unsigned int delay)
{
  if (ink_atomic_increment((int *) &m_event_count, 1) < 0) {
    ink_assert(!"not reached");
  }
  mutex->thread_holding->schedule_in(this, HRTIME_MSECONDS(delay));
}

bool
INKVConnInternal::get_data(int id, void *data)
{
  switch (id) {
  case TS_API_DATA_READ_VIO:
    *((TSVIO *) data) = reinterpret_cast<TSVIO>(&m_read_vio);
    return true;
  case TS_API_DATA_WRITE_VIO:
    *((TSVIO *) data) = reinterpret_cast<TSVIO>(&m_write_vio);
    return true;
  case TS_API_DATA_OUTPUT_VC:
    *((TSVConn *) data) = reinterpret_cast<TSVConn>(m_output_vc);
    return true;
  case TS_API_DATA_CLOSED:
    *((int *) data) = m_closed;
    return true;
  default:
    return INKContInternal::get_data(id, data);
  }
}

bool INKVConnInternal::set_data(int id, void *data)
{
  switch (id) {
  case TS_API_DATA_OUTPUT_VC:
    m_output_vc = (VConnection *) data;
    return true;
  default:
    return INKContInternal::set_data(id, data);
  }
}

////////////////////////////////////////////////////////////////////
//
// APIHook, APIHooks, HttpAPIHooks
//
////////////////////////////////////////////////////////////////////

int
APIHook::invoke(int event, void *edata)
{
  if ((event == EVENT_IMMEDIATE) || (event == EVENT_INTERVAL)) {
    if (ink_atomic_increment((int *) &m_cont->m_event_count, 1) < 0) {
      ink_assert(!"not reached");
    }
  }
  return m_cont->handleEvent(event, edata);
}

APIHook *
APIHook::next() const
{
  return m_link.next;
}


void
APIHooks::prepend(INKContInternal *cont)
{
  APIHook *api_hook;

  api_hook = apiHookAllocator.alloc();
  api_hook->m_cont = cont;

  m_hooks.push(api_hook);
}

void
APIHooks::append(INKContInternal *cont)
{
  APIHook *api_hook;

  api_hook = apiHookAllocator.alloc();
  api_hook->m_cont = cont;

  m_hooks.enqueue(api_hook);
}

APIHook *
APIHooks::get()
{
  return m_hooks.head;
}


HttpAPIHooks::HttpAPIHooks():
hooks_set(0)
{
}

HttpAPIHooks::~HttpAPIHooks()
{
  clear();
}



void
HttpAPIHooks::clear()
{
  APIHook *api_hook;
  APIHook *next_hook;
  int i;

  for (i = 0; i < TS_HTTP_LAST_HOOK; i++) {
    api_hook = m_hooks[i].get();
    while (api_hook) {
      next_hook = api_hook->m_link.next;
      apiHookAllocator.free(api_hook);
      api_hook = next_hook;
    }
  }
  hooks_set = 0;
}

void
HttpAPIHooks::prepend(TSHttpHookID id, INKContInternal *cont)
{
  hooks_set = 1;
  m_hooks[id].prepend(cont);
}

void
HttpAPIHooks::append(TSHttpHookID id, INKContInternal *cont)
{
  hooks_set = 1;
  m_hooks[id].append(cont);
}

APIHook *
HttpAPIHooks::get(TSHttpHookID id)
{
  return m_hooks[id].get();
}


////////////////////////////////////////////////////////////////////
//
// ConfigUpdateCbTable
//
////////////////////////////////////////////////////////////////////

ConfigUpdateCbTable::ConfigUpdateCbTable()
{
  cb_table = ink_hash_table_create(InkHashTableKeyType_String);
}

ConfigUpdateCbTable::~ConfigUpdateCbTable()
{
  ink_assert(cb_table != NULL);

  ink_hash_table_destroy(cb_table);
}

void
ConfigUpdateCbTable::insert(INKContInternal *contp, const char *name)
{
  ink_assert(cb_table != NULL);

  if (contp && name)
    ink_hash_table_insert(cb_table, (InkHashTableKey) name, (InkHashTableValue) contp);
}

void
ConfigUpdateCbTable::invoke(const char *name)
{
  ink_assert(cb_table != NULL);

  InkHashTableIteratorState ht_iter;
  InkHashTableEntry *ht_entry;
  INKContInternal *contp;

  if (name != NULL) {
    if (strcmp(name, "*") == 0) {
      ht_entry = ink_hash_table_iterator_first(cb_table, &ht_iter);
      while (ht_entry != NULL) {
        contp = (INKContInternal *)ink_hash_table_entry_value(cb_table, ht_entry);
        ink_assert(contp != NULL);
        invoke(contp);
        ht_entry = ink_hash_table_iterator_next(cb_table, &ht_iter);
      }
    } else {
      ht_entry = ink_hash_table_lookup_entry(cb_table, (InkHashTableKey) name);
      if (ht_entry != NULL) {
        contp = (INKContInternal *) ink_hash_table_entry_value(cb_table, ht_entry);
        ink_assert(contp != NULL);
        invoke(contp);
      }
    }
  }
}

void
ConfigUpdateCbTable::invoke(INKContInternal *contp)
{
  eventProcessor.schedule_imm(NEW(new ConfigUpdateCallback(contp)), ET_TASK);
}

////////////////////////////////////////////////////////////////////
//
// api_init
//
////////////////////////////////////////////////////////////////////

void
api_init()
{
  // HDR FIX ME

  static int init = 1;

  if (init) {
    init = 0;

#ifndef UNSAFE_FORCE_MUTEX
    ink_mutex_init(&big_mux, "APIMongoMutex");
#endif

    /* URL schemes */
    TS_URL_SCHEME_FILE = URL_SCHEME_FILE;
    TS_URL_SCHEME_FTP = URL_SCHEME_FTP;
    TS_URL_SCHEME_GOPHER = URL_SCHEME_GOPHER;
    TS_URL_SCHEME_HTTP = URL_SCHEME_HTTP;
    TS_URL_SCHEME_HTTPS = URL_SCHEME_HTTPS;
    TS_URL_SCHEME_MAILTO = URL_SCHEME_MAILTO;
    TS_URL_SCHEME_NEWS = URL_SCHEME_NEWS;
    TS_URL_SCHEME_NNTP = URL_SCHEME_NNTP;
    TS_URL_SCHEME_PROSPERO = URL_SCHEME_PROSPERO;
    TS_URL_SCHEME_TELNET = URL_SCHEME_TELNET;
    TS_URL_SCHEME_WAIS = URL_SCHEME_WAIS;

    TS_URL_LEN_FILE = URL_LEN_FILE;
    TS_URL_LEN_FTP = URL_LEN_FTP;
    TS_URL_LEN_GOPHER = URL_LEN_GOPHER;
    TS_URL_LEN_HTTP = URL_LEN_HTTP;
    TS_URL_LEN_HTTPS = URL_LEN_HTTPS;
    TS_URL_LEN_MAILTO = URL_LEN_MAILTO;
    TS_URL_LEN_NEWS = URL_LEN_NEWS;
    TS_URL_LEN_NNTP = URL_LEN_NNTP;
    TS_URL_LEN_PROSPERO = URL_LEN_PROSPERO;
    TS_URL_LEN_TELNET = URL_LEN_TELNET;
    TS_URL_LEN_WAIS = URL_LEN_WAIS;

    /* MIME fields */
    TS_MIME_FIELD_ACCEPT = MIME_FIELD_ACCEPT;
    TS_MIME_FIELD_ACCEPT_CHARSET = MIME_FIELD_ACCEPT_CHARSET;
    TS_MIME_FIELD_ACCEPT_ENCODING = MIME_FIELD_ACCEPT_ENCODING;
    TS_MIME_FIELD_ACCEPT_LANGUAGE = MIME_FIELD_ACCEPT_LANGUAGE;
    TS_MIME_FIELD_ACCEPT_RANGES = MIME_FIELD_ACCEPT_RANGES;
    TS_MIME_FIELD_AGE = MIME_FIELD_AGE;
    TS_MIME_FIELD_ALLOW = MIME_FIELD_ALLOW;
    TS_MIME_FIELD_APPROVED = MIME_FIELD_APPROVED;
    TS_MIME_FIELD_AUTHORIZATION = MIME_FIELD_AUTHORIZATION;
    TS_MIME_FIELD_BYTES = MIME_FIELD_BYTES;
    TS_MIME_FIELD_CACHE_CONTROL = MIME_FIELD_CACHE_CONTROL;
    TS_MIME_FIELD_CLIENT_IP = MIME_FIELD_CLIENT_IP;
    TS_MIME_FIELD_CONNECTION = MIME_FIELD_CONNECTION;
    TS_MIME_FIELD_CONTENT_BASE = MIME_FIELD_CONTENT_BASE;
    TS_MIME_FIELD_CONTENT_ENCODING = MIME_FIELD_CONTENT_ENCODING;
    TS_MIME_FIELD_CONTENT_LANGUAGE = MIME_FIELD_CONTENT_LANGUAGE;
    TS_MIME_FIELD_CONTENT_LENGTH = MIME_FIELD_CONTENT_LENGTH;
    TS_MIME_FIELD_CONTENT_LOCATION = MIME_FIELD_CONTENT_LOCATION;
    TS_MIME_FIELD_CONTENT_MD5 = MIME_FIELD_CONTENT_MD5;
    TS_MIME_FIELD_CONTENT_RANGE = MIME_FIELD_CONTENT_RANGE;
    TS_MIME_FIELD_CONTENT_TYPE = MIME_FIELD_CONTENT_TYPE;
    TS_MIME_FIELD_CONTROL = MIME_FIELD_CONTROL;
    TS_MIME_FIELD_COOKIE = MIME_FIELD_COOKIE;
    TS_MIME_FIELD_DATE = MIME_FIELD_DATE;
    TS_MIME_FIELD_DISTRIBUTION = MIME_FIELD_DISTRIBUTION;
    TS_MIME_FIELD_ETAG = MIME_FIELD_ETAG;
    TS_MIME_FIELD_EXPECT = MIME_FIELD_EXPECT;
    TS_MIME_FIELD_EXPIRES = MIME_FIELD_EXPIRES;
    TS_MIME_FIELD_FOLLOWUP_TO = MIME_FIELD_FOLLOWUP_TO;
    TS_MIME_FIELD_FROM = MIME_FIELD_FROM;
    TS_MIME_FIELD_HOST = MIME_FIELD_HOST;
    TS_MIME_FIELD_IF_MATCH = MIME_FIELD_IF_MATCH;
    TS_MIME_FIELD_IF_MODIFIED_SINCE = MIME_FIELD_IF_MODIFIED_SINCE;
    TS_MIME_FIELD_IF_NONE_MATCH = MIME_FIELD_IF_NONE_MATCH;
    TS_MIME_FIELD_IF_RANGE = MIME_FIELD_IF_RANGE;
    TS_MIME_FIELD_IF_UNMODIFIED_SINCE = MIME_FIELD_IF_UNMODIFIED_SINCE;
    TS_MIME_FIELD_KEEP_ALIVE = MIME_FIELD_KEEP_ALIVE;
    TS_MIME_FIELD_KEYWORDS = MIME_FIELD_KEYWORDS;
    TS_MIME_FIELD_LAST_MODIFIED = MIME_FIELD_LAST_MODIFIED;
    TS_MIME_FIELD_LINES = MIME_FIELD_LINES;
    TS_MIME_FIELD_LOCATION = MIME_FIELD_LOCATION;
    TS_MIME_FIELD_MAX_FORWARDS = MIME_FIELD_MAX_FORWARDS;
    TS_MIME_FIELD_MESSAGE_ID = MIME_FIELD_MESSAGE_ID;
    TS_MIME_FIELD_NEWSGROUPS = MIME_FIELD_NEWSGROUPS;
    TS_MIME_FIELD_ORGANIZATION = MIME_FIELD_ORGANIZATION;
    TS_MIME_FIELD_PATH = MIME_FIELD_PATH;
    TS_MIME_FIELD_PRAGMA = MIME_FIELD_PRAGMA;
    TS_MIME_FIELD_PROXY_AUTHENTICATE = MIME_FIELD_PROXY_AUTHENTICATE;
    TS_MIME_FIELD_PROXY_AUTHORIZATION = MIME_FIELD_PROXY_AUTHORIZATION;
    TS_MIME_FIELD_PROXY_CONNECTION = MIME_FIELD_PROXY_CONNECTION;
    TS_MIME_FIELD_PUBLIC = MIME_FIELD_PUBLIC;
    TS_MIME_FIELD_RANGE = MIME_FIELD_RANGE;
    TS_MIME_FIELD_REFERENCES = MIME_FIELD_REFERENCES;
    TS_MIME_FIELD_REFERER = MIME_FIELD_REFERER;
    TS_MIME_FIELD_REPLY_TO = MIME_FIELD_REPLY_TO;
    TS_MIME_FIELD_RETRY_AFTER = MIME_FIELD_RETRY_AFTER;
    TS_MIME_FIELD_SENDER = MIME_FIELD_SENDER;
    TS_MIME_FIELD_SERVER = MIME_FIELD_SERVER;
    TS_MIME_FIELD_SET_COOKIE = MIME_FIELD_SET_COOKIE;
    TS_MIME_FIELD_SUBJECT = MIME_FIELD_SUBJECT;
    TS_MIME_FIELD_SUMMARY = MIME_FIELD_SUMMARY;
    TS_MIME_FIELD_TE = MIME_FIELD_TE;
    TS_MIME_FIELD_TRANSFER_ENCODING = MIME_FIELD_TRANSFER_ENCODING;
    TS_MIME_FIELD_UPGRADE = MIME_FIELD_UPGRADE;
    TS_MIME_FIELD_USER_AGENT = MIME_FIELD_USER_AGENT;
    TS_MIME_FIELD_VARY = MIME_FIELD_VARY;
    TS_MIME_FIELD_VIA = MIME_FIELD_VIA;
    TS_MIME_FIELD_WARNING = MIME_FIELD_WARNING;
    TS_MIME_FIELD_WWW_AUTHENTICATE = MIME_FIELD_WWW_AUTHENTICATE;
    TS_MIME_FIELD_XREF = MIME_FIELD_XREF;
    TS_MIME_FIELD_X_FORWARDED_FOR = MIME_FIELD_X_FORWARDED_FOR;


    TS_MIME_LEN_ACCEPT = MIME_LEN_ACCEPT;
    TS_MIME_LEN_ACCEPT_CHARSET = MIME_LEN_ACCEPT_CHARSET;
    TS_MIME_LEN_ACCEPT_ENCODING = MIME_LEN_ACCEPT_ENCODING;
    TS_MIME_LEN_ACCEPT_LANGUAGE = MIME_LEN_ACCEPT_LANGUAGE;
    TS_MIME_LEN_ACCEPT_RANGES = MIME_LEN_ACCEPT_RANGES;
    TS_MIME_LEN_AGE = MIME_LEN_AGE;
    TS_MIME_LEN_ALLOW = MIME_LEN_ALLOW;
    TS_MIME_LEN_APPROVED = MIME_LEN_APPROVED;
    TS_MIME_LEN_AUTHORIZATION = MIME_LEN_AUTHORIZATION;
    TS_MIME_LEN_BYTES = MIME_LEN_BYTES;
    TS_MIME_LEN_CACHE_CONTROL = MIME_LEN_CACHE_CONTROL;
    TS_MIME_LEN_CLIENT_IP = MIME_LEN_CLIENT_IP;
    TS_MIME_LEN_CONNECTION = MIME_LEN_CONNECTION;
    TS_MIME_LEN_CONTENT_BASE = MIME_LEN_CONTENT_BASE;
    TS_MIME_LEN_CONTENT_ENCODING = MIME_LEN_CONTENT_ENCODING;
    TS_MIME_LEN_CONTENT_LANGUAGE = MIME_LEN_CONTENT_LANGUAGE;
    TS_MIME_LEN_CONTENT_LENGTH = MIME_LEN_CONTENT_LENGTH;
    TS_MIME_LEN_CONTENT_LOCATION = MIME_LEN_CONTENT_LOCATION;
    TS_MIME_LEN_CONTENT_MD5 = MIME_LEN_CONTENT_MD5;
    TS_MIME_LEN_CONTENT_RANGE = MIME_LEN_CONTENT_RANGE;
    TS_MIME_LEN_CONTENT_TYPE = MIME_LEN_CONTENT_TYPE;
    TS_MIME_LEN_CONTROL = MIME_LEN_CONTROL;
    TS_MIME_LEN_COOKIE = MIME_LEN_COOKIE;
    TS_MIME_LEN_DATE = MIME_LEN_DATE;
    TS_MIME_LEN_DISTRIBUTION = MIME_LEN_DISTRIBUTION;
    TS_MIME_LEN_ETAG = MIME_LEN_ETAG;
    TS_MIME_LEN_EXPECT = MIME_LEN_EXPECT;
    TS_MIME_LEN_EXPIRES = MIME_LEN_EXPIRES;
    TS_MIME_LEN_FOLLOWUP_TO = MIME_LEN_FOLLOWUP_TO;
    TS_MIME_LEN_FROM = MIME_LEN_FROM;
    TS_MIME_LEN_HOST = MIME_LEN_HOST;
    TS_MIME_LEN_IF_MATCH = MIME_LEN_IF_MATCH;
    TS_MIME_LEN_IF_MODIFIED_SINCE = MIME_LEN_IF_MODIFIED_SINCE;
    TS_MIME_LEN_IF_NONE_MATCH = MIME_LEN_IF_NONE_MATCH;
    TS_MIME_LEN_IF_RANGE = MIME_LEN_IF_RANGE;
    TS_MIME_LEN_IF_UNMODIFIED_SINCE = MIME_LEN_IF_UNMODIFIED_SINCE;
    TS_MIME_LEN_KEEP_ALIVE = MIME_LEN_KEEP_ALIVE;
    TS_MIME_LEN_KEYWORDS = MIME_LEN_KEYWORDS;
    TS_MIME_LEN_LAST_MODIFIED = MIME_LEN_LAST_MODIFIED;
    TS_MIME_LEN_LINES = MIME_LEN_LINES;
    TS_MIME_LEN_LOCATION = MIME_LEN_LOCATION;
    TS_MIME_LEN_MAX_FORWARDS = MIME_LEN_MAX_FORWARDS;
    TS_MIME_LEN_MESSAGE_ID = MIME_LEN_MESSAGE_ID;
    TS_MIME_LEN_NEWSGROUPS = MIME_LEN_NEWSGROUPS;
    TS_MIME_LEN_ORGANIZATION = MIME_LEN_ORGANIZATION;
    TS_MIME_LEN_PATH = MIME_LEN_PATH;
    TS_MIME_LEN_PRAGMA = MIME_LEN_PRAGMA;
    TS_MIME_LEN_PROXY_AUTHENTICATE = MIME_LEN_PROXY_AUTHENTICATE;
    TS_MIME_LEN_PROXY_AUTHORIZATION = MIME_LEN_PROXY_AUTHORIZATION;
    TS_MIME_LEN_PROXY_CONNECTION = MIME_LEN_PROXY_CONNECTION;
    TS_MIME_LEN_PUBLIC = MIME_LEN_PUBLIC;
    TS_MIME_LEN_RANGE = MIME_LEN_RANGE;
    TS_MIME_LEN_REFERENCES = MIME_LEN_REFERENCES;
    TS_MIME_LEN_REFERER = MIME_LEN_REFERER;
    TS_MIME_LEN_REPLY_TO = MIME_LEN_REPLY_TO;
    TS_MIME_LEN_RETRY_AFTER = MIME_LEN_RETRY_AFTER;
    TS_MIME_LEN_SENDER = MIME_LEN_SENDER;
    TS_MIME_LEN_SERVER = MIME_LEN_SERVER;
    TS_MIME_LEN_SET_COOKIE = MIME_LEN_SET_COOKIE;
    TS_MIME_LEN_SUBJECT = MIME_LEN_SUBJECT;
    TS_MIME_LEN_SUMMARY = MIME_LEN_SUMMARY;
    TS_MIME_LEN_TE = MIME_LEN_TE;
    TS_MIME_LEN_TRANSFER_ENCODING = MIME_LEN_TRANSFER_ENCODING;
    TS_MIME_LEN_UPGRADE = MIME_LEN_UPGRADE;
    TS_MIME_LEN_USER_AGENT = MIME_LEN_USER_AGENT;
    TS_MIME_LEN_VARY = MIME_LEN_VARY;
    TS_MIME_LEN_VIA = MIME_LEN_VIA;
    TS_MIME_LEN_WARNING = MIME_LEN_WARNING;
    TS_MIME_LEN_WWW_AUTHENTICATE = MIME_LEN_WWW_AUTHENTICATE;
    TS_MIME_LEN_XREF = MIME_LEN_XREF;
    TS_MIME_LEN_X_FORWARDED_FOR = MIME_LEN_X_FORWARDED_FOR;


    /* HTTP methods */
    TS_HTTP_METHOD_CONNECT = HTTP_METHOD_CONNECT;
    TS_HTTP_METHOD_DELETE = HTTP_METHOD_DELETE;
    TS_HTTP_METHOD_GET = HTTP_METHOD_GET;
    TS_HTTP_METHOD_HEAD = HTTP_METHOD_HEAD;
    TS_HTTP_METHOD_ICP_QUERY = HTTP_METHOD_ICP_QUERY;
    TS_HTTP_METHOD_OPTIONS = HTTP_METHOD_OPTIONS;
    TS_HTTP_METHOD_POST = HTTP_METHOD_POST;
    TS_HTTP_METHOD_PURGE = HTTP_METHOD_PURGE;
    TS_HTTP_METHOD_PUT = HTTP_METHOD_PUT;
    TS_HTTP_METHOD_TRACE = HTTP_METHOD_TRACE;

    TS_HTTP_LEN_CONNECT = HTTP_LEN_CONNECT;
    TS_HTTP_LEN_DELETE = HTTP_LEN_DELETE;
    TS_HTTP_LEN_GET = HTTP_LEN_GET;
    TS_HTTP_LEN_HEAD = HTTP_LEN_HEAD;
    TS_HTTP_LEN_ICP_QUERY = HTTP_LEN_ICP_QUERY;
    TS_HTTP_LEN_OPTIONS = HTTP_LEN_OPTIONS;
    TS_HTTP_LEN_POST = HTTP_LEN_POST;
    TS_HTTP_LEN_PURGE = HTTP_LEN_PURGE;
    TS_HTTP_LEN_PUT = HTTP_LEN_PUT;
    TS_HTTP_LEN_TRACE = HTTP_LEN_TRACE;

    /* HTTP miscellaneous values */
    TS_HTTP_VALUE_BYTES = HTTP_VALUE_BYTES;
    TS_HTTP_VALUE_CHUNKED = HTTP_VALUE_CHUNKED;
    TS_HTTP_VALUE_CLOSE = HTTP_VALUE_CLOSE;
    TS_HTTP_VALUE_COMPRESS = HTTP_VALUE_COMPRESS;
    TS_HTTP_VALUE_DEFLATE = HTTP_VALUE_DEFLATE;
    TS_HTTP_VALUE_GZIP = HTTP_VALUE_GZIP;
    TS_HTTP_VALUE_IDENTITY = HTTP_VALUE_IDENTITY;
    TS_HTTP_VALUE_KEEP_ALIVE = HTTP_VALUE_KEEP_ALIVE;
    TS_HTTP_VALUE_MAX_AGE = HTTP_VALUE_MAX_AGE;
    TS_HTTP_VALUE_MAX_STALE = HTTP_VALUE_MAX_STALE;
    TS_HTTP_VALUE_MIN_FRESH = HTTP_VALUE_MIN_FRESH;
    TS_HTTP_VALUE_MUST_REVALIDATE = HTTP_VALUE_MUST_REVALIDATE;
    TS_HTTP_VALUE_NONE = HTTP_VALUE_NONE;
    TS_HTTP_VALUE_NO_CACHE = HTTP_VALUE_NO_CACHE;
    TS_HTTP_VALUE_NO_STORE = HTTP_VALUE_NO_STORE;
    TS_HTTP_VALUE_NO_TRANSFORM = HTTP_VALUE_NO_TRANSFORM;
    TS_HTTP_VALUE_ONLY_IF_CACHED = HTTP_VALUE_ONLY_IF_CACHED;
    TS_HTTP_VALUE_PRIVATE = HTTP_VALUE_PRIVATE;
    TS_HTTP_VALUE_PROXY_REVALIDATE = HTTP_VALUE_PROXY_REVALIDATE;
    TS_HTTP_VALUE_PUBLIC = HTTP_VALUE_PUBLIC;
    TS_HTTP_VALUE_S_MAXAGE = HTTP_VALUE_S_MAXAGE;

    TS_HTTP_LEN_BYTES = HTTP_LEN_BYTES;
    TS_HTTP_LEN_CHUNKED = HTTP_LEN_CHUNKED;
    TS_HTTP_LEN_CLOSE = HTTP_LEN_CLOSE;
    TS_HTTP_LEN_COMPRESS = HTTP_LEN_COMPRESS;
    TS_HTTP_LEN_DEFLATE = HTTP_LEN_DEFLATE;
    TS_HTTP_LEN_GZIP = HTTP_LEN_GZIP;
    TS_HTTP_LEN_IDENTITY = HTTP_LEN_IDENTITY;
    TS_HTTP_LEN_KEEP_ALIVE = HTTP_LEN_KEEP_ALIVE;
    TS_HTTP_LEN_MAX_AGE = HTTP_LEN_MAX_AGE;
    TS_HTTP_LEN_MAX_STALE = HTTP_LEN_MAX_STALE;
    TS_HTTP_LEN_MIN_FRESH = HTTP_LEN_MIN_FRESH;
    TS_HTTP_LEN_MUST_REVALIDATE = HTTP_LEN_MUST_REVALIDATE;
    TS_HTTP_LEN_NONE = HTTP_LEN_NONE;
    TS_HTTP_LEN_NO_CACHE = HTTP_LEN_NO_CACHE;
    TS_HTTP_LEN_NO_STORE = HTTP_LEN_NO_STORE;
    TS_HTTP_LEN_NO_TRANSFORM = HTTP_LEN_NO_TRANSFORM;
    TS_HTTP_LEN_ONLY_IF_CACHED = HTTP_LEN_ONLY_IF_CACHED;
    TS_HTTP_LEN_PRIVATE = HTTP_LEN_PRIVATE;
    TS_HTTP_LEN_PROXY_REVALIDATE = HTTP_LEN_PROXY_REVALIDATE;
    TS_HTTP_LEN_PUBLIC = HTTP_LEN_PUBLIC;
    TS_HTTP_LEN_S_MAXAGE = HTTP_LEN_S_MAXAGE;

    http_global_hooks = NEW(new HttpAPIHooks);
    global_config_cbs = NEW(new ConfigUpdateCbTable);

    if (TS_MAX_API_STATS > 0) {
      api_rsb = RecAllocateRawStatBlock(TS_MAX_API_STATS);
      if (NULL == api_rsb) {
        Warning("Can't allocate API stats block");
      } else {
        Debug("sdk", "initialized SDK stats APIs with %d slots", TS_MAX_API_STATS);
      }
    } else {
      api_rsb = NULL;
    }

    memset(state_arg_table, 0, sizeof(state_arg_table));

    // Setup the version string for returning to plugins
    ink_strlcpy(traffic_server_version, appVersionInfo.VersionStr, sizeof(traffic_server_version));
    // Extract the elements.
    // coverity[secure_coding]
    if (sscanf(traffic_server_version, "%d.%d.%d", &ts_major_version, &ts_minor_version, &ts_patch_version) != 3) {
      Warning("Unable to parse traffic server version string '%s'\n", traffic_server_version);
    }

  }
}

////////////////////////////////////////////////////////////////////
//
// API memory management
//
////////////////////////////////////////////////////////////////////

void *
_TSmalloc(size_t size, const char *path)
{
  NOWARN_UNUSED(path);
  return ats_malloc(size);
}

void *
_TSrealloc(void *ptr, size_t size, const char *path)
{
  NOWARN_UNUSED(path);
  return ats_realloc(ptr, size);
}

// length has to be int64_t and not size_t, since -1 means to call strlen() to get length
char *
_TSstrdup(const char *str, int64_t length, const char *path)
{
  return _xstrdup(str, length, path);
}

size_t
_TSstrlcpy(char *dst, const char *str, size_t siz)
{
  return ink_strlcpy(dst, str, siz);
}

size_t
_TSstrlcat(char *dst, const char *str, size_t siz)
{
  return ink_strlcat(dst, str, siz);
}

void
_TSfree(void *ptr)
{
  ats_free(ptr);
}

////////////////////////////////////////////////////////////////////
//
// Encoding utility
//
////////////////////////////////////////////////////////////////////
TSReturnCode
TSBase64Decode(const char *str, size_t str_len, unsigned char *dst, size_t dst_size, size_t *length)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)str) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)dst) == TS_SUCCESS);

  return ats_base64_decode(str, str_len, dst, dst_size, length) ? TS_SUCCESS : TS_ERROR;
}

TSReturnCode
TSBase64Encode(const char *str, size_t str_len, char *dst, size_t dst_size, size_t *length)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)str) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)dst) == TS_SUCCESS);

  return ats_base64_encode(str, str_len, dst, dst_size, length) ? TS_SUCCESS : TS_ERROR;
}

////////////////////////////////////////////////////////////////////
//
// API utility routines
//
////////////////////////////////////////////////////////////////////

unsigned int
TSrandom()
{
  return this_ethread()->generator.random();
}

double
TSdrandom()
{
  return this_ethread()->generator.drandom();
}

ink_hrtime
TShrtime()
{
  return ink_get_based_hrtime();
}

////////////////////////////////////////////////////////////////////
//
// API install and plugin locations
//
////////////////////////////////////////////////////////////////////

const char *
TSInstallDirGet(void)
{
  return system_root_dir;
}

const char *
TSConfigDirGet(void)
{
  return system_config_directory;
}

const char *
TSTrafficServerVersionGet(void)
{
  return traffic_server_version;
}
int TSTrafficServerVersionGetMajor() { return ts_major_version; }
int TSTrafficServerVersionGetMinor() { return ts_minor_version; }
int TSTrafficServerVersionGetPatch() { return ts_patch_version; }

const char *
TSPluginDirGet(void)
{
  static char path[PATH_NAME_MAX + 1] = "";

  if (*path == '\0') {
    char *plugin_dir = NULL;
    RecGetRecordString_Xmalloc("proxy.config.plugin.plugin_dir", &plugin_dir);
    if (!plugin_dir) {
      Error("Unable to read proxy.config.plugin.plugin_dir");
      return NULL;
    }
    Layout::relative_to(path, sizeof(path),
                        Layout::get()->prefix, plugin_dir);
    ats_free(plugin_dir);
  }

  return path;
}

////////////////////////////////////////////////////////////////////
//
// Plugin registration
//
////////////////////////////////////////////////////////////////////

TSReturnCode
TSPluginRegister(TSSDKVersion sdk_version, TSPluginRegistrationInfo *plugin_info)
{
  PluginSDKVersion version = (PluginSDKVersion)sdk_version;

  if (!plugin_reg_current)
    return TS_ERROR;

  sdk_assert(sdk_sanity_check_null_ptr((void*) plugin_info) == TS_SUCCESS);

  plugin_reg_current->plugin_registered = true;

  // We're compatible only within the 3.x release
  if (version >= PLUGIN_SDK_VERSION_3_0 && version < PLUGIN_SDK_VERSION_4_0) {
    plugin_reg_current->sdk_version = version;
  } else {
    plugin_reg_current->sdk_version = PLUGIN_SDK_VERSION_UNKNOWN;
  }

  if (plugin_info->plugin_name) {
    plugin_reg_current->plugin_name = ats_strdup(plugin_info->plugin_name);
  }

  if (plugin_info->vendor_name) {
    plugin_reg_current->vendor_name = ats_strdup(plugin_info->vendor_name);
  }

  if (plugin_info->support_email) {
    plugin_reg_current->support_email = ats_strdup(plugin_info->support_email);
  }

  return TS_SUCCESS;
}

////////////////////////////////////////////////////////////////////
//
// API file management
//
////////////////////////////////////////////////////////////////////

TSFile
TSfopen(const char *filename, const char *mode)
{
  FileImpl *file;

  file = NEW(new FileImpl);
  if (!file->fopen(filename, mode)) {
    delete file;
    return NULL;
  }

  return (TSFile)file;
}

void
TSfclose(TSFile filep)
{
  FileImpl *file = (FileImpl *) filep;
  file->fclose();
  delete file;
}

size_t
TSfread(TSFile filep, void *buf, size_t length)
{
  FileImpl *file = (FileImpl *) filep;
  return file->fread(buf, length);
}

size_t
TSfwrite(TSFile filep, const void *buf, size_t length)
{
  FileImpl *file = (FileImpl *) filep;
  return file->fwrite(buf, length);
}

void
TSfflush(TSFile filep)
{
  FileImpl *file = (FileImpl *) filep;
  file->fflush();
}

char *
TSfgets(TSFile filep, char *buf, size_t length)
{
  FileImpl *file = (FileImpl *) filep;
  return file->fgets(buf, length);
}

////////////////////////////////////////////////////////////////////
//
// Header component object handles
//
////////////////////////////////////////////////////////////////////

TSReturnCode
TSHandleMLocRelease(TSMBuffer bufp, TSMLoc parent, TSMLoc mloc)
{
  MIMEFieldSDKHandle *field_handle;
  HdrHeapObjImpl *obj = (HdrHeapObjImpl *) mloc;

  if (mloc == TS_NULL_MLOC)
    return TS_SUCCESS;

  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);

  switch (obj->m_type) {
  case HDR_HEAP_OBJ_URL:
  case HDR_HEAP_OBJ_HTTP_HEADER:
  case HDR_HEAP_OBJ_MIME_HEADER:
    return TS_SUCCESS;

  case HDR_HEAP_OBJ_FIELD_SDK_HANDLE:
    field_handle = (MIMEFieldSDKHandle *) obj;
    if (sdk_sanity_check_field_handle(mloc, parent) != TS_SUCCESS)
      return TS_ERROR;

    sdk_free_field_handle(bufp, field_handle);
    return TS_SUCCESS;

  default:
    ink_release_assert(!"invalid mloc");
    return TS_ERROR;
  }
}


////////////////////////////////////////////////////////////////////
//
// HdrHeaps (previously known as "Marshal Buffers")
//
////////////////////////////////////////////////////////////////////

// TSMBuffer: pointers to HdrHeapSDKHandle objects

TSMBuffer
TSMBufferCreate(void)
{
  TSMBuffer bufp;
  HdrHeapSDKHandle *new_heap = NEW(new HdrHeapSDKHandle);

  new_heap->m_heap = new_HdrHeap();
  bufp = (TSMBuffer)new_heap;
  // TODO: Should remove this when memory allocation is guaranteed to fail.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  return bufp;
}

TSReturnCode
TSMBufferDestroy(TSMBuffer bufp)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  if (!isWriteable(bufp))
    return TS_ERROR;

  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  HdrHeapSDKHandle *sdk_heap = (HdrHeapSDKHandle *)bufp;
  sdk_heap->m_heap->destroy();
  delete sdk_heap;
  return TS_SUCCESS;
}

////////////////////////////////////////////////////////////////////
//
// URLs
//
////////////////////////////////////////////////////////////////////

// TSMBuffer: pointers to HdrHeapSDKHandle objects
// TSMLoc:    pointers to URLImpl objects
TSReturnCode
TSUrlCreate(TSMBuffer bufp, TSMLoc *locp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr(locp) == TS_SUCCESS);

  if (isWriteable(bufp)) {
    HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
    *locp = (TSMLoc)url_create(heap);
    return TS_SUCCESS;
  }
  return TS_ERROR;
}

TSReturnCode
TSUrlDestroy(TSMBuffer bufp, TSMLoc url_loc)
{
  return TS_SUCCESS;
}

TSReturnCode
TSUrlClone(TSMBuffer dest_bufp, TSMBuffer src_bufp, TSMLoc src_url, TSMLoc *locp)
{
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(src_url) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr(locp) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  HdrHeap *s_heap, *d_heap;
  URLImpl *s_url, *d_url;

  s_heap = ((HdrHeapSDKHandle *) src_bufp)->m_heap;
  d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  s_url = (URLImpl *) src_url;

  d_url = url_copy(s_url, s_heap, d_heap, (s_heap != d_heap));
  *locp = (TSMLoc)d_url;
  return TS_SUCCESS;
}

TSReturnCode
TSUrlCopy(TSMBuffer dest_bufp, TSMLoc dest_obj, TSMBuffer src_bufp, TSMLoc src_obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(src_obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(dest_obj) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  HdrHeap *s_heap, *d_heap;
  URLImpl *s_url, *d_url;

  s_heap = ((HdrHeapSDKHandle *) src_bufp)->m_heap;
  d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  s_url = (URLImpl *) src_obj;
  d_url = (URLImpl *) dest_obj;

  url_copy_onto(s_url, s_heap, d_url, d_heap, (s_heap != d_heap));
  return TS_SUCCESS;
}

void
TSUrlPrint(TSMBuffer bufp, TSMLoc obj, TSIOBuffer iobufp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(iobufp) == TS_SUCCESS);

  MIOBuffer *b = (MIOBuffer *) iobufp;
  IOBufferBlock *blk;
  int bufindex;
  int tmp, dumpoffset;
  int done;
  URL u;

  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;
  dumpoffset = 0;

  do {
    blk = b->get_current_block();
    if (!blk || blk->write_avail() == 0) {
      b->add_block();
      blk = b->get_current_block();
    }

    bufindex = 0;
    tmp = dumpoffset;

    done = u.print(blk->end(), blk->write_avail(), &bufindex, &tmp);

    dumpoffset += bufindex;
    b->fill(bufindex);
  } while (!done);
}

TSParseResult
TSUrlParse(TSMBuffer bufp, TSMLoc obj, const char **start, const char *end)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)*start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)end) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_PARSE_ERROR;

  URL u;
  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;
  url_clear(u.m_url_impl);
  return (TSParseResult)u.parse(start, end);
}

int
TSUrlLengthGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  URLImpl *url_impl = (URLImpl *) obj;
  return url_length_get(url_impl);
}

char *
TSUrlStringGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  URLImpl *url_impl = (URLImpl *) obj;
  return url_string_get(url_impl, NULL, length, NULL);
}

char *
TSUrlStringGetBuf(TSMBuffer bufp, TSMLoc obj, char *buff, int buf_size, int* length)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)buff) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  URLImpl *url_impl = (URLImpl *) obj;
  return url_string_get_buf(url_impl, buff, buf_size, length);
}

typedef const char *(URL::*URLPartGetF) (int *length);
typedef void (URL::*URLPartSetF) (const char *value, int length);

static const char *
URLPartGet(TSMBuffer bufp, TSMLoc obj, int *length, URLPartGetF url_f)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  URL u;

  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;

  return (u.*url_f)(length);
}

static TSReturnCode
URLPartSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length, URLPartSetF url_f)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  URL u;
  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;

  if (!value)
    length = 0;
  else if (length < 0)
    length = strlen(value);
  (u.*url_f)(value, length);

  return TS_SUCCESS;
}

const char *
TSUrlSchemeGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::scheme_get);
}

TSReturnCode
TSUrlSchemeSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::scheme_set);
}

/* Internet specific URLs */

const char *
TSUrlUserGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::user_get);
}

TSReturnCode
TSUrlUserSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::user_set);
}

const char *
TSUrlPasswordGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::password_get);
}

TSReturnCode
TSUrlPasswordSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::password_set);
}

const char *
TSUrlHostGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::host_get);
}

TSReturnCode
TSUrlHostSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::host_set);
}

int
TSUrlPortGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  URL u;
  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;

  return u.port_get();
}

TSReturnCode
TSUrlPortSet(TSMBuffer bufp, TSMLoc obj, int port)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  if (!isWriteable(bufp) || (port < 0))
    return TS_ERROR;

  URL u;

  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;
  u.port_set(port);
  return TS_SUCCESS;
}

/* FTP and HTTP specific URLs  */

const char *
TSUrlPathGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::path_get);
}

TSReturnCode
TSUrlPathSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::path_set);
}

/* FTP specific URLs */

int
TSUrlFtpTypeGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  URL u;
  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;
  return u.type_get();
}

TSReturnCode
TSUrlFtpTypeSet(TSMBuffer bufp, TSMLoc obj, int type)
{
  //The valid values are : 0, 65('A'), 97('a'),
  //69('E'), 101('e'), 73 ('I') and 105('i').
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);


  if ((type == 0 || type=='A' || type=='E' || type=='I' || type=='a' || type=='i' || type=='e') && isWriteable(bufp)) {
    URL u;

    u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
    u.m_url_impl = (URLImpl *) obj;
    u.type_set(type);
    return TS_SUCCESS;
  }

  return TS_ERROR;
}

/* HTTP specific URLs */

const char *
TSUrlHttpParamsGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::params_get);
}

TSReturnCode
TSUrlHttpParamsSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::params_set);
}

const char *
TSUrlHttpQueryGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::query_get);
}

TSReturnCode
TSUrlHttpQuerySet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::query_set);
}

const char *
TSUrlHttpFragmentGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  return URLPartGet(bufp, obj, length, &URL::fragment_get);
}

TSReturnCode
TSUrlHttpFragmentSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  return URLPartSet(bufp, obj, value, length, &URL::fragment_set);
}

// URL percent encoding
TSReturnCode
TSStringPercentEncode(const char* str, int str_len, char *dst, size_t dst_size, size_t *length, const unsigned char *map)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)str) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)dst) == TS_SUCCESS);

  int new_len; // Unfortunately, a lot of the core uses "int" for length's internally...

  if (str_len < 0)
    str_len = strlen(str);

  sdk_assert(str_len < static_cast<int>(dst_size));

  // TODO: Perhaps we should make escapify_url() deal with const properly...
  if (NULL == LogUtils::escapify_url(NULL, const_cast<char*>(str), str_len, &new_len, dst, dst_size, map)) {
    if (length)
      *length = 0;
    return TS_ERROR;
  }

  if (length)
    *length = new_len;

  return TS_SUCCESS;
}

TSReturnCode
TSStringPercentDecode(const char *str, size_t str_len, char* dst, size_t dst_size, size_t *length)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)str) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)dst) == TS_SUCCESS);

  if (0 == str_len)
    str_len = strlen(str);

  // return unescapifyStr(str);
  char *buffer = dst;
  const char *src =  str;
  int s = 0; // State, which we don't really use

  // TODO: We should check for "failures" here?
  unescape_str(buffer, buffer+dst_size, src, src+str_len, s);
  *buffer = '\0';
  if (length)
    *length = (buffer - dst);

  return TS_SUCCESS;
}

TSReturnCode
TSUrlPercentEncode(TSMBuffer bufp, TSMLoc obj, char *dst, size_t dst_size, size_t *length, const unsigned char *map)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  char *url;
  int url_len;
  TSReturnCode ret;
  URLImpl *url_impl = (URLImpl *)obj;

  // TODO: at some point, it might be nice to allow this to write to a pre-allocated buffer
  url = url_string_get(url_impl, NULL, &url_len, NULL);
  ret = TSStringPercentEncode(url, url_len, dst, dst_size, length, map);
  ats_free(url);

  return ret;
}


////////////////////////////////////////////////////////////////////
//
// MIME Headers
//
////////////////////////////////////////////////////////////////////

/**************/
/* MimeParser */
/**************/

TSMimeParser
TSMimeParserCreate(void)
{
  TSMimeParser parser = reinterpret_cast<TSMimeParser>(ats_malloc(sizeof(MIMEParser)));

  mime_parser_init((MIMEParser *) parser);
  return parser;
}

void
TSMimeParserClear(TSMimeParser parser)
{
  sdk_assert(sdk_sanity_check_mime_parser(parser) == TS_SUCCESS);

  mime_parser_clear((MIMEParser *) parser);
}

void
TSMimeParserDestroy(TSMimeParser parser)
{
  sdk_assert(sdk_sanity_check_mime_parser(parser) == TS_SUCCESS);

  mime_parser_clear((MIMEParser *) parser);
  ats_free(parser);
}

/***********/
/* MimeHdr */
/***********/

// TSMBuffer: pointers to HdrHeapSDKHandle objects
// TSMLoc:    pointers to MIMEFieldSDKHandle objects

TSReturnCode
TSMimeHdrCreate(TSMBuffer bufp, TSMLoc *locp)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If not allowed, return NULL.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)locp) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  *locp = reinterpret_cast<TSMLoc>(mime_hdr_create(((HdrHeapSDKHandle *) bufp)->m_heap));
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrDestroy(TSMBuffer bufp, TSMLoc obj)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS));

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(obj);

  mime_hdr_destroy(((HdrHeapSDKHandle *) bufp)->m_heap, mh);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrClone(TSMBuffer dest_bufp, TSMBuffer src_bufp, TSMLoc src_hdr, TSMLoc *locp)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If not allowed, return NULL.
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mime_hdr_handle(src_hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(src_hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)locp) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  HdrHeap *s_heap, *d_heap;
  MIMEHdrImpl *s_mh, *d_mh;

  s_heap = ((HdrHeapSDKHandle *) src_bufp)->m_heap;
  d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  s_mh = _hdr_mloc_to_mime_hdr_impl(src_hdr);

  d_mh = mime_hdr_clone(s_mh, s_heap, d_heap, (s_heap != d_heap));
  *locp = (TSMLoc)d_mh;

  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrCopy(TSMBuffer dest_bufp, TSMLoc dest_obj, TSMBuffer src_bufp, TSMLoc src_obj)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(src_obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(src_obj) == TS_SUCCESS));
  sdk_assert((sdk_sanity_check_mime_hdr_handle(dest_obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(dest_obj) == TS_SUCCESS));

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  HdrHeap *s_heap, *d_heap;
  MIMEHdrImpl *s_mh, *d_mh;

  s_heap = ((HdrHeapSDKHandle *) src_bufp)->m_heap;
  d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  s_mh = _hdr_mloc_to_mime_hdr_impl(src_obj);
  d_mh = _hdr_mloc_to_mime_hdr_impl(dest_obj);

  mime_hdr_fields_clear(d_heap, d_mh);
  mime_hdr_copy_onto(s_mh, s_heap, d_mh, d_heap, (s_heap != d_heap));
  return TS_SUCCESS;
}

void
TSMimeHdrPrint(TSMBuffer bufp, TSMLoc obj, TSIOBuffer iobufp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_iocore_structure(iobufp) == TS_SUCCESS);

  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(obj);
  MIOBuffer *b = (MIOBuffer *) iobufp;
  IOBufferBlock *blk;
  int bufindex;
  int tmp, dumpoffset = 0;
  int done;

  do {
    blk = b->get_current_block();
    if (!blk || blk->write_avail() == 0) {
      b->add_block();
      blk = b->get_current_block();
    }

    bufindex = 0;
    tmp = dumpoffset;
    done = mime_hdr_print(heap, mh, blk->end(), blk->write_avail(), &bufindex, &tmp);

    dumpoffset += bufindex;
    b->fill(bufindex);
  } while (!done);
}

TSParseResult
TSMimeHdrParse(TSMimeParser parser, TSMBuffer bufp, TSMLoc obj, const char **start, const char *end)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_null_ptr((void*)start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)*start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)end) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_PARSE_ERROR;

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(obj);

  return (TSParseResult)mime_parser_parse((MIMEParser *) parser, ((HdrHeapSDKHandle *) bufp)->m_heap, mh,
                                          start, end, false, false);
}

int
TSMimeHdrLengthGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS));

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(obj);
  return mime_hdr_length_get(mh);
}

TSReturnCode
TSMimeHdrFieldsClear(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS));

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(obj);

  mime_hdr_fields_clear(((HdrHeapSDKHandle *) bufp)->m_heap, mh);
  return TS_SUCCESS;
}

int
TSMimeHdrFieldsCount(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS));

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(obj);
  return mime_hdr_fields_count(mh);
}

// The following three helper functions should not be used in plugins! Since they are not used
// by plugins, there's no need to validate the input.
const char *
TSMimeFieldValueGet(TSMBuffer bufp, TSMLoc field_obj, int idx, int *value_len_ptr)
{
  NOWARN_UNUSED(bufp);
  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field_obj;

  if (idx >= 0) {
    return mime_field_value_get_comma_val(handle->field_ptr, value_len_ptr, idx);
  } else {
    return mime_field_value_get(handle->field_ptr, value_len_ptr);
  }

  *value_len_ptr = 0;
  return NULL;
}

void
TSMimeFieldValueSet(TSMBuffer bufp, TSMLoc field_obj, int idx, const char *value, int length)
{
  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field_obj;
  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;

  if (length == -1)
    length = strlen(value);

  if (idx >= 0)
    mime_field_value_set_comma_val(heap, handle->mh, handle->field_ptr, idx, value, length);
  else
    mime_field_value_set(heap, handle->mh, handle->field_ptr, value, length, true);
}

void
TSMimeFieldValueInsert(TSMBuffer bufp, TSMLoc field_obj, const char *value, int length, int idx)
{
  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field_obj;
  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;

  if (length == -1)
    length = strlen(value);

  mime_field_value_insert_comma_val(heap, handle->mh, handle->field_ptr, idx, value, length);
}


/****************/
/* MimeHdrField */
/****************/

// TSMBuffer: pointers to HdrHeapSDKHandle objects
// TSMLoc:    pointers to MIMEFieldSDKHandle objects

int
TSMimeHdrFieldEqual(TSMBuffer bufp, TSMLoc hdr_obj, TSMLoc field1_obj, TSMLoc field2_obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_field_handle(field1_obj, hdr_obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_field_handle(field2_obj, hdr_obj) == TS_SUCCESS);

  MIMEFieldSDKHandle *field1_handle = (MIMEFieldSDKHandle *) field1_obj;
  MIMEFieldSDKHandle *field2_handle = (MIMEFieldSDKHandle *) field2_obj;

  if ((field1_handle == NULL) || (field2_handle == NULL))
    return (field1_handle == field2_handle);
  return (field1_handle->field_ptr == field2_handle->field_ptr);
}

TSMLoc
TSMimeHdrFieldGet(TSMBuffer bufp, TSMLoc hdr_obj, int idx)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr_obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr_obj) == TS_SUCCESS));
  sdk_assert(idx >= 0);

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(hdr_obj);
  MIMEField *f = mime_hdr_field_get(mh, idx);

  if (f == NULL)
    return TS_NULL_MLOC;

  MIMEFieldSDKHandle *h = sdk_alloc_field_handle(bufp, mh);

  h->field_ptr = f;
  return reinterpret_cast<TSMLoc>(h);
}

TSMLoc
TSMimeHdrFieldFind(TSMBuffer bufp, TSMLoc hdr_obj, const char *name, int length)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr_obj) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr_obj) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_null_ptr((void*)name) == TS_SUCCESS);

  if (length == -1)
    length = strlen(name);

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(hdr_obj);
  MIMEField *f = mime_hdr_field_find(mh, name, length);

  if (f == NULL)
    return TS_NULL_MLOC;

  MIMEFieldSDKHandle *h = sdk_alloc_field_handle(bufp, mh);

  h->field_ptr = f;
  return reinterpret_cast<TSMLoc>(h);
}

TSReturnCode
TSMimeHdrFieldAppend(TSMBuffer bufp, TSMLoc mh_mloc, TSMLoc field_mloc)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(mh_mloc) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(mh_mloc) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field_mloc) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEField *mh_field;
  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(mh_mloc);
  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field_mloc;

  //////////////////////////////////////////////////////////////////////
  // The field passed in field_mloc might have been allocated from    //
  // inside a MIME header (the correct way), or it might have been    //
  // created in isolation as a "standalone field" (the old way).      //
  //                                                                  //
  // If it's a standalone field (the associated mime header is NULL), //
  // then we need to now allocate a real field inside the header,     //
  // copy over the data, and convert the standalone field into a      //
  // forwarding pointer to the real field, in case it's used again    //
  //////////////////////////////////////////////////////////////////////
  if (field_handle->mh == NULL) {
    HdrHeap *heap = (HdrHeap *) (((HdrHeapSDKHandle *) bufp)->m_heap);

    // allocate a new hdr field and copy any pre-set info
    mh_field = mime_field_create(heap, mh);

    // FIX: is it safe to copy everything over?
    memcpy(mh_field, field_handle->field_ptr, sizeof(MIMEField));

    // now set up the forwarding ptr from standalone field to hdr field
    field_handle->mh = mh;
    field_handle->field_ptr = mh_field;
  }

  ink_assert(field_handle->mh == mh);
  ink_assert(field_handle->field_ptr->m_ptr_name);

  mime_hdr_field_attach(mh, field_handle->field_ptr, 1, NULL);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldRemove(TSMBuffer bufp, TSMLoc mh_mloc, TSMLoc field_mloc)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(mh_mloc) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(mh_mloc) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field_mloc, mh_mloc) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field_mloc;

  if (field_handle->mh != NULL) {
    MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(mh_mloc);
    ink_assert(mh == field_handle->mh);
    sdk_sanity_check_field_handle(field_mloc, mh_mloc);
    mime_hdr_field_detach(mh, field_handle->field_ptr, false);        // only detach this dup
  }
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldDestroy(TSMBuffer bufp, TSMLoc mh_mloc, TSMLoc field_mloc)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(mh_mloc) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(mh_mloc) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field_mloc, mh_mloc) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field_mloc;

  if (field_handle->mh == NULL) { // NOT SUPPORTED!!
    ink_release_assert(!"Failed MH");
  } else {
    MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(mh_mloc);
    HdrHeap *heap = (HdrHeap *) (((HdrHeapSDKHandle *) bufp)->m_heap);

    ink_assert(mh == field_handle->mh);
    if (sdk_sanity_check_field_handle(field_mloc, mh_mloc) != TS_SUCCESS)
      return TS_ERROR;

    // detach and delete this field, but not all dups
    mime_hdr_field_delete(heap, mh, field_handle->field_ptr, false);
  }
  // for consistence, the handle will not be released here.
  // users will be required to do it.
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldCreate(TSMBuffer bufp, TSMLoc mh_mloc, TSMLoc *locp)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If not allowed, return NULL.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(mh_mloc) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(mh_mloc) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_null_ptr((void*)locp) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(mh_mloc);
  HdrHeap *heap = (HdrHeap *) (((HdrHeapSDKHandle *) bufp)->m_heap);
  MIMEFieldSDKHandle *h = sdk_alloc_field_handle(bufp, mh);

  h->field_ptr = mime_field_create(heap, mh);
  *locp = reinterpret_cast<TSMLoc>(h);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldCreateNamed(TSMBuffer bufp, TSMLoc mh_mloc, const char *name, int name_len, TSMLoc *locp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(mh_mloc) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(mh_mloc) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_null_ptr((void*)name) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)locp) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  if (name_len == -1)
    name_len = strlen(name);

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(mh_mloc);
  HdrHeap *heap = (HdrHeap *) (((HdrHeapSDKHandle *) bufp)->m_heap);
  MIMEFieldSDKHandle *h = sdk_alloc_field_handle(bufp, mh);
  h->field_ptr = mime_field_create_named(heap, mh, name, name_len);
  *locp = reinterpret_cast<TSMLoc>(h);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldCopy(TSMBuffer dest_bufp, TSMLoc dest_hdr, TSMLoc dest_field, TSMBuffer src_bufp, TSMLoc src_hdr, TSMLoc src_field)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(src_hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(src_hdr) == TS_SUCCESS));
  sdk_assert((sdk_sanity_check_mime_hdr_handle(dest_hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(dest_hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(src_field, src_hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_field_handle(dest_field, dest_hdr) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  bool dest_attached;
  MIMEFieldSDKHandle *s_handle = (MIMEFieldSDKHandle *) src_field;
  MIMEFieldSDKHandle *d_handle = (MIMEFieldSDKHandle *) dest_field;
  HdrHeap *d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;

  // FIX: This tortuous detach/change/attach algorithm is due to the
  //      fact that we can't change the name of an attached header (assertion)

  // TODO: This is never used ... is_live() has no side effects, so this should be ok
  // to not call, so commented out
  // src_attached = (s_handle->mh && s_handle->field_ptr->is_live());
  dest_attached = (d_handle->mh && d_handle->field_ptr->is_live());

  if (dest_attached)
    mime_hdr_field_detach(d_handle->mh, d_handle->field_ptr, false);

  mime_field_name_value_set(d_heap, d_handle->mh, d_handle->field_ptr,
                            s_handle->field_ptr->m_wks_idx,
                            s_handle->field_ptr->m_ptr_name,
                            s_handle->field_ptr->m_len_name,
                            s_handle->field_ptr->m_ptr_value, s_handle->field_ptr->m_len_value, 0, 0, true);

  if (dest_attached)
    mime_hdr_field_attach(d_handle->mh, d_handle->field_ptr, 1, NULL);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldClone(TSMBuffer dest_bufp, TSMLoc dest_hdr, TSMBuffer src_bufp, TSMLoc src_hdr, TSMLoc src_field, TSMLoc *locp)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If not allowed, return NULL.
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(dest_hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(dest_hdr) == TS_SUCCESS));
  sdk_assert((sdk_sanity_check_mime_hdr_handle(src_hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(src_hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(src_field, src_hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)locp) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  // This is sort of sub-optimal, since we'll check the args again. TODO.
  if (TSMimeHdrFieldCreate(dest_bufp, dest_hdr, locp) == TS_SUCCESS) {
    TSMimeHdrFieldCopy(dest_bufp, dest_hdr, *locp, src_bufp, src_hdr, src_field);
    return TS_SUCCESS;
  }
  // TSMimeHdrFieldCreate() failed for some reason.
  return TS_ERROR;
}

TSReturnCode
TSMimeHdrFieldCopyValues(TSMBuffer dest_bufp, TSMLoc dest_hdr, TSMLoc dest_field, TSMBuffer src_bufp, TSMLoc src_hdr,
                         TSMLoc src_field)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(src_hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(src_hdr) == TS_SUCCESS));
  sdk_assert((sdk_sanity_check_mime_hdr_handle(dest_hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(dest_hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(src_field, src_hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_field_handle(dest_field, dest_hdr) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  MIMEFieldSDKHandle *s_handle = (MIMEFieldSDKHandle *) src_field;
  MIMEFieldSDKHandle *d_handle = (MIMEFieldSDKHandle *) dest_field;
  HdrHeap *d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  MIMEField *s_field, *d_field;

  s_field = s_handle->field_ptr;
  d_field = d_handle->field_ptr;
  mime_field_value_set(d_heap, d_handle->mh, d_field, s_field->m_ptr_value, s_field->m_len_value, true);
  return TS_SUCCESS;
}

// TODO: This is implemented horribly slowly, but who's using it anyway?
//       If we threaded all the MIMEFields, this function could be easier,
//       but we'd have to print dups in order and we'd need a flag saying
//       end of dup list or dup follows.
TSMLoc
TSMimeHdrFieldNext(TSMBuffer bufp, TSMLoc hdr, TSMLoc field)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;

  if (handle->mh == NULL)
    return TS_NULL_MLOC;

  int slotnum = mime_hdr_field_slotnum(handle->mh, handle->field_ptr);
  if (slotnum == -1)
    return TS_NULL_MLOC;

  while (1) {
    ++slotnum;
    MIMEField *f = mime_hdr_field_get_slotnum(handle->mh, slotnum);

    if (f == NULL)
      return TS_NULL_MLOC;
    if (f->is_live()) {
      MIMEFieldSDKHandle *h = sdk_alloc_field_handle(bufp, handle->mh);
      
      h->field_ptr = f;
      return reinterpret_cast<TSMLoc>(h);
    }
  }
  return TS_NULL_MLOC; // Shouldn't happen.
}

TSMLoc
TSMimeHdrFieldNextDup(TSMBuffer bufp, TSMLoc hdr, TSMLoc field)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  MIMEHdrImpl *mh = _hdr_mloc_to_mime_hdr_impl(hdr);
  MIMEFieldSDKHandle *field_handle = (MIMEFieldSDKHandle *) field;
  MIMEField *next = field_handle->field_ptr->m_next_dup;
  if (next == NULL)
    return TS_NULL_MLOC;

  MIMEFieldSDKHandle *next_handle = sdk_alloc_field_handle(bufp, mh);
  next_handle->field_ptr = next;
  return (TSMLoc)next_handle;
}

int
TSMimeHdrFieldLengthGet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  return mime_field_length_get(handle->field_ptr);
}

const char *
TSMimeHdrFieldNameGet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int *length)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  return mime_field_name_get(handle->field_ptr, length);
}

TSReturnCode
TSMimeHdrFieldNameSet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, const char *name, int length)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) name) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  if (length == -1)
    length = strlen(name);

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;

  int attached = (handle->mh && handle->field_ptr->is_live());

  if (attached)
    mime_hdr_field_detach(handle->mh, handle->field_ptr, false);

  handle->field_ptr->name_set(heap, handle->mh, name, length);

  if (attached)
    mime_hdr_field_attach(handle->mh, handle->field_ptr, 1, NULL);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValuesClear(TSMBuffer bufp, TSMLoc hdr, TSMLoc field)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;

  /**
   * Modified the string value passed from an empty string ("") to NULL.
   * An empty string is also considered to be a token. The correct value of
   * the field after this function should be NULL.
   */
  mime_field_value_set(heap, handle->mh, handle->field_ptr, NULL, 0, 1);
  return TS_SUCCESS;
}

int
TSMimeHdrFieldValuesCount(TSMBuffer bufp, TSMLoc hdr, TSMLoc field)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  return mime_field_value_get_comma_val_count(handle->field_ptr);
}

const char*
TSMimeHdrFieldValueStringGet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, int *value_len_ptr) {
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) value_len_ptr) == TS_SUCCESS);

  return TSMimeFieldValueGet(bufp, field, idx, value_len_ptr);
}

time_t
TSMimeHdrFieldValueDateGet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  int value_len;
  const char *value_str = TSMimeFieldValueGet(bufp, field, -1, &value_len);

  if (value_str == NULL)
    return (time_t) 0;

  return mime_parse_date(value_str, value_str + value_len);
}

int
TSMimeHdrFieldValueIntGet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  int value_len;
  const char *value_str = TSMimeFieldValueGet(bufp, field, idx, &value_len);

  if (value_str == NULL)
    return 0;

  return mime_parse_int(value_str, value_str + value_len);
}

int64_t
TSMimeHdrFieldValueInt64Get(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  int value_len;
  const char *value_str = TSMimeFieldValueGet(bufp, field, idx, &value_len);

  if (value_str == NULL)
    return 0;

  return mime_parse_int64(value_str, value_str + value_len);
}

unsigned int
TSMimeHdrFieldValueUintGet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  int value_len;
  const char *value_str = TSMimeFieldValueGet(bufp, field, idx, &value_len);

  if (value_str == NULL)
    return 0;

  return mime_parse_uint(value_str, value_str + value_len);
}

TSReturnCode
TSMimeHdrFieldValueStringSet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, const char *value, int length)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) value) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  if (length == -1)
    length = strlen(value);

  TSMimeFieldValueSet(bufp, field, idx, value, length);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueDateSet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, time_t value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  char tmp[33];
  int len = mime_format_date(tmp, value);

    // idx is ignored and we overwrite all existing values
    // TSMimeFieldValueSet(bufp, field_obj, idx, tmp, len);
  TSMimeFieldValueSet(bufp, field, -1, tmp, len);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueIntSet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, int value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  char tmp[16];
  int len = mime_format_int(tmp, value, sizeof(tmp));

  TSMimeFieldValueSet(bufp, field, idx, tmp, len);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueInt64Set(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, int64_t value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  char tmp[20];
  int len = mime_format_int64(tmp, value, sizeof(tmp));

  TSMimeFieldValueSet(bufp, field, idx, tmp, len);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueUintSet(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, unsigned int value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  char tmp[16];
  int len = mime_format_uint(tmp, value, sizeof(tmp));

  TSMimeFieldValueSet(bufp, field, idx, tmp, len);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueAppend(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, const char *value, int length)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)value) == TS_SUCCESS);
  sdk_assert(idx >= 0);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;

  if (length == -1)
    length = strlen(value);
  mime_field_value_extend_comma_val(heap, handle->mh, handle->field_ptr, idx, value, length);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueStringInsert(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, const char *value, int length)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR, else return TS_SUCCESS.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) value) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  if (length == -1)
    length = strlen(value);
  TSMimeFieldValueInsert(bufp, field, value, length, idx);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueIntInsert(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, int value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR, else return TS_SUCCESS.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  char tmp[16];
  int len = mime_format_int(tmp, value, sizeof(tmp));

  TSMimeFieldValueInsert(bufp, field, tmp, len, idx);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueUintInsert(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx, unsigned int value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR, else return TS_SUCCESS.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  char tmp[16];
  int len = mime_format_uint(tmp, value, sizeof(tmp));

  TSMimeFieldValueInsert(bufp, field, tmp, len, idx);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueDateInsert(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, time_t value)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR, else return TS_SUCCESS
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  if (TSMimeHdrFieldValuesClear(bufp, hdr, field) == TS_ERROR)
    return TS_ERROR;

  char tmp[33];
  int len = mime_format_date(tmp, value);
    // idx ignored, overwrite all exisiting values
    // (void)TSMimeFieldValueInsert(bufp, field_obj, tmp, len, idx);
  (void) TSMimeFieldValueSet(bufp, field, -1, tmp, len);
  return TS_SUCCESS;
}

TSReturnCode
TSMimeHdrFieldValueDelete(TSMBuffer bufp, TSMLoc hdr, TSMLoc field, int idx)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert((sdk_sanity_check_mime_hdr_handle(hdr) == TS_SUCCESS) ||
             (sdk_sanity_check_http_hdr_handle(hdr) == TS_SUCCESS));
  sdk_assert(sdk_sanity_check_field_handle(field, hdr) == TS_SUCCESS);
  sdk_assert(idx >= 0);

  if (!isWriteable(bufp))
    return TS_ERROR;

  MIMEFieldSDKHandle *handle = (MIMEFieldSDKHandle *) field;
  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;

  mime_field_value_delete_comma_val(heap, handle->mh, handle->field_ptr, idx);
  return TS_SUCCESS;
}

/**************/
/* HttpParser */
/**************/
TSHttpParser
TSHttpParserCreate(void)
{
  TSHttpParser parser = reinterpret_cast<TSHttpParser>(ats_malloc(sizeof(HTTPParser)));
  http_parser_init((HTTPParser *) parser);

  return parser;
}

void
TSHttpParserClear(TSHttpParser parser)
{
  sdk_assert(sdk_sanity_check_http_parser(parser) == TS_SUCCESS);
  http_parser_clear((HTTPParser *) parser);
}

void
TSHttpParserDestroy(TSHttpParser parser)
{
  sdk_assert(sdk_sanity_check_http_parser(parser) == TS_SUCCESS);
  http_parser_clear((HTTPParser *) parser);
  ats_free(parser);
}

/***********/
/* HttpHdr */
/***********/


TSMLoc
TSHttpHdrCreate(TSMBuffer bufp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);

  HTTPHdr h;
  h.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  h.create(HTTP_TYPE_UNKNOWN);
  return (TSMLoc)(h.m_http);
}

void
TSHttpHdrDestroy(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  // No more objects counts in heap or deallocation
  //   so do nothing!

  // HDR FIX ME - Did this free the MBuffer in Pete's old system
}

TSReturnCode
TSHttpHdrClone(TSMBuffer dest_bufp, TSMBuffer src_bufp, TSMLoc src_hdr, TSMLoc *locp)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If not allowed, return NULL.
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(src_hdr) == TS_SUCCESS);

  if (!isWriteable(dest_bufp))
    return TS_ERROR;

  HdrHeap *s_heap, *d_heap;
  HTTPHdrImpl *s_hh, *d_hh;

  s_heap = ((HdrHeapSDKHandle *) src_bufp)->m_heap;
  d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  s_hh = (HTTPHdrImpl *) src_hdr;

  if (s_hh->m_type != HDR_HEAP_OBJ_HTTP_HEADER)
    return TS_ERROR;

  // TODO: This is never used
  // inherit_strs = (s_heap != d_heap ? true : false);
  d_hh = http_hdr_clone(s_hh, s_heap, d_heap);
  *locp = (TSMLoc)d_hh;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpHdrCopy(TSMBuffer dest_bufp, TSMLoc dest_obj, TSMBuffer src_bufp, TSMLoc src_obj)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(src_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(dest_bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(dest_obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(src_obj) == TS_SUCCESS);

  if (! isWriteable(dest_bufp))
    return TS_ERROR;

  bool inherit_strs;
  HdrHeap *s_heap, *d_heap;
  HTTPHdrImpl *s_hh, *d_hh;

  s_heap = ((HdrHeapSDKHandle *) src_bufp)->m_heap;
  d_heap = ((HdrHeapSDKHandle *) dest_bufp)->m_heap;
  s_hh = (HTTPHdrImpl *) src_obj;
  d_hh = (HTTPHdrImpl *) dest_obj;

  if ((s_hh->m_type != HDR_HEAP_OBJ_HTTP_HEADER) || (d_hh->m_type != HDR_HEAP_OBJ_HTTP_HEADER))
    return TS_ERROR;

  inherit_strs = (s_heap != d_heap ? true : false);
  TSHttpHdrTypeSet(dest_bufp, dest_obj, (TSHttpType) (s_hh->m_polarity));
  http_hdr_copy_onto(s_hh, s_heap, d_hh, d_heap, inherit_strs);
  return TS_SUCCESS;
}

void
TSHttpHdrPrint(TSMBuffer bufp, TSMLoc obj, TSIOBuffer iobufp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(iobufp) == TS_SUCCESS);

  MIOBuffer *b = (MIOBuffer *) iobufp;
  IOBufferBlock *blk;
  HTTPHdr h;
  int bufindex;
  int tmp, dumpoffset;
  int done;

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);

  dumpoffset = 0;
  do {
    blk = b->get_current_block();
    if (!blk || blk->write_avail() == 0) {
      b->add_block();
      blk = b->get_current_block();
    }

    bufindex = 0;
    tmp = dumpoffset;

    done = h.print(blk->end(), blk->write_avail(), &bufindex, &tmp);

    dumpoffset += bufindex;
    b->fill(bufindex);
  } while (!done);
}

TSParseResult
TSHttpHdrParseReq(TSHttpParser parser, TSMBuffer bufp, TSMLoc obj, const char **start, const char *end)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)*start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)end) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_PARSE_ERROR;

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);
  TSHttpHdrTypeSet(bufp, obj, TS_HTTP_TYPE_REQUEST);
  return (TSParseResult)h.parse_req((HTTPParser *) parser, start, end, false);
}

TSParseResult
TSHttpHdrParseResp(TSHttpParser parser, TSMBuffer bufp, TSMLoc obj, const char **start, const char *end)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)*start) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)end) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_PARSE_ERROR;

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);
  TSHttpHdrTypeSet(bufp, obj, TS_HTTP_TYPE_RESPONSE);
  return (TSParseResult)h.parse_resp((HTTPParser *) parser, start, end, false);
}

int
TSHttpHdrLengthGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);
  return h.length_get();
}

TSHttpType
TSHttpHdrTypeGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  HTTPHdr h;
  SET_HTTP_HDR(h, bufp, obj);
  /* Don't need the assert as the check is done in sdk_sanity_check_http_hdr_handle
     ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);
   */
  return (TSHttpType)h.type_get();
}

TSReturnCode
TSHttpHdrTypeSet(TSMBuffer bufp, TSMLoc obj, TSHttpType type)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert((type >= TS_HTTP_TYPE_UNKNOWN) && (type <= TS_HTTP_TYPE_RESPONSE));

  if (!isWriteable(bufp))
    return TS_ERROR;

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);

  // FIX: why are we using an HTTPHdr here?  why can't we
  //      just manipulate the impls directly?

  // In Pete's MBuffer system you can change the type
  //   at will.  Not so anymore.  We need to try to
  //   fake the difference.  We not going to let
  //   people change the types of a header.  If they
  //   try, too bad.
  if (h.m_http->m_polarity == HTTP_TYPE_UNKNOWN) {
    if (type == (TSHttpType) HTTP_TYPE_REQUEST) {
      h.m_http->u.req.m_url_impl = url_create(h.m_heap);
      h.m_http->m_polarity = (HTTPType) type;
    } else if (type == (TSHttpType) HTTP_TYPE_RESPONSE) {
      h.m_http->m_polarity = (HTTPType) type;
    }
  }
  return TS_SUCCESS;
}

int
TSHttpHdrVersionGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  HTTPVersion ver = h.version_get();
  return ver.m_version;
}

TSReturnCode
TSHttpHdrVersionSet(TSMBuffer bufp, TSMLoc obj, int ver)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  HTTPHdr h;
  HTTPVersion version(ver);

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);

  h.version_set(version);
  return TS_SUCCESS;
}

const char *
TSHttpHdrMethodGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  return h.method_get(length);
}

TSReturnCode
TSHttpHdrMethodSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)value) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  if (length < 0)
    length = strlen(value);

  h.method_set(value, length);
  return TS_SUCCESS;
}

TSReturnCode
TSHttpHdrUrlGet(TSMBuffer bufp, TSMLoc obj, TSMLoc *locp)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  HTTPHdrImpl *hh = (HTTPHdrImpl *) obj;

  if (hh->m_polarity != HTTP_TYPE_REQUEST)
    return TS_ERROR;

  *locp = ((TSMLoc)hh->u.req.m_url_impl);
  return TS_SUCCESS;
}

TSReturnCode
TSHttpHdrUrlSet(TSMBuffer bufp, TSMLoc obj, TSMLoc url)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(url) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  HdrHeap *heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  HTTPHdrImpl *hh = (HTTPHdrImpl *) obj;

  if (hh->m_type != HDR_HEAP_OBJ_HTTP_HEADER)
    return TS_ERROR;

  URLImpl *url_impl = (URLImpl *) url;
  http_hdr_url_set(heap, hh, url_impl);
  return TS_SUCCESS;
}

TSHttpStatus
TSHttpHdrStatusGet(TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  return (TSHttpStatus)h.status_get();
}

TSReturnCode
TSHttpHdrStatusSet(TSMBuffer bufp, TSMLoc obj, TSHttpStatus status)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);
  h.status_set((HTTPStatus) status);
  return TS_SUCCESS;
}

const char *
TSHttpHdrReasonGet(TSMBuffer bufp, TSMLoc obj, int *length)
{
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  return h.reason_get(length);
}

TSReturnCode
TSHttpHdrReasonSet(TSMBuffer bufp, TSMLoc obj, const char *value, int length)
{
  // Allow to modify the buffer only
  // if bufp is modifiable. If bufp is not modifiable return
  // TS_ERROR. If allowed, return TS_SUCCESS. Changed the
  // return value of function from void to TSReturnCode.
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_http_hdr_handle(obj) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) value) == TS_SUCCESS);

  if (!isWriteable(bufp))
    return TS_ERROR;

  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);
  /* Don't need the assert as the check is done in sdk_sanity_check_http_hdr_handle
     ink_assert(h.m_http->m_type == HDR_HEAP_OBJ_HTTP_HEADER);
  */

  if (length < 0)
    length = strlen(value);
  h.reason_set(value, length);
  return TS_SUCCESS;
}

const char *
TSHttpHdrReasonLookup(TSHttpStatus status)
{
  return http_hdr_reason_lookup((HTTPStatus) status);
}


////////////////////////////////////////////////////////////////////
//
// Cache
//
////////////////////////////////////////////////////////////////////

inline TSReturnCode
sdk_sanity_check_cachekey(TSCacheKey key)
{
  if (NULL == key)
    return TS_ERROR;

  return TS_SUCCESS;
}

TSCacheKey
TSCacheKeyCreate(void)
{
  TSCacheKey key = (TSCacheKey)NEW(new CacheInfo());

  // TODO: Probably remove this when we can be use "NEW" can't fail.
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);
  return key;
}

TSReturnCode
TSCacheKeyDigestSet(TSCacheKey key, const char *input, int length)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure((void*) input) == TS_SUCCESS);
  sdk_assert(length > 0);
  
  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  ((CacheInfo *) key)->cache_key.encodeBuffer((char *) input, length);
  return TS_SUCCESS;
}

TSReturnCode
TSCacheKeyDigestFromUrlSet(TSCacheKey key, TSMLoc url)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  url_MD5_get((URLImpl *) url, &((CacheInfo *) key)->cache_key);
  return TS_SUCCESS;
}

TSReturnCode
TSCacheKeyDataTypeSet(TSCacheKey key, TSCacheDataType type)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  switch (type) {
  case TS_CACHE_DATA_TYPE_NONE:
    ((CacheInfo *) key)->frag_type = CACHE_FRAG_TYPE_NONE;
    break;
  case TS_CACHE_DATA_TYPE_OTHER:      /* other maps to http */
  case TS_CACHE_DATA_TYPE_HTTP:
    ((CacheInfo *) key)->frag_type = CACHE_FRAG_TYPE_HTTP;
    break;
  default:
    return TS_ERROR;
  }

  return TS_SUCCESS;
}

TSReturnCode
TSCacheKeyHostNameSet(TSCacheKey key, const char *hostname, int host_len)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)hostname) == TS_SUCCESS);
  sdk_assert(host_len > 0);

  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  CacheInfo *i = (CacheInfo *) key;
  /* need to make a copy of the hostname. The caller
     might deallocate it anytime in the future */
  i->hostname = (char *)ats_malloc(host_len);
  memcpy(i->hostname, hostname, host_len);
  i->len = host_len;
  return TS_SUCCESS;
}

TSReturnCode
TSCacheKeyPinnedSet(TSCacheKey key, time_t pin_in_cache)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  CacheInfo *i = (CacheInfo *) key;
  i->pin_in_cache = pin_in_cache;
  return TS_SUCCESS;
}

TSReturnCode
TSCacheKeyDestroy(TSCacheKey key)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  CacheInfo *i = (CacheInfo *) key;

  ats_free(i->hostname);
  i->magic = CACHE_INFO_MAGIC_DEAD;
  delete i;
  return TS_SUCCESS;
}

TSReturnCode
TSCacheKeyEnableCluster(TSCacheKey key)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);
  if (((CacheInfo *) key)->magic != CACHE_INFO_MAGIC_ALIVE)
    return TS_ERROR;

  CacheInfo *i = (CacheInfo *) key;
  i->cluster_cache_local = false;
  return TS_SUCCESS;
}

TSCacheHttpInfo
TSCacheHttpInfoCopy(TSCacheHttpInfo infop)
{
  CacheHTTPInfo *new_info = NEW(new CacheHTTPInfo);

  new_info->copy((CacheHTTPInfo *) infop);
  return reinterpret_cast<TSCacheHttpInfo>(new_info);
}

void
TSCacheHttpInfoReqGet(TSCacheHttpInfo infop, TSMBuffer *bufp, TSMLoc *obj)
{
  CacheHTTPInfo *info = (CacheHTTPInfo *)infop;

  *(reinterpret_cast<HTTPHdr**>(bufp)) = info->request_get();
  *obj = reinterpret_cast<TSMLoc>(info->request_get()->m_http);
  sdk_sanity_check_mbuffer(*bufp);
}


void
TSCacheHttpInfoRespGet(TSCacheHttpInfo infop, TSMBuffer *bufp, TSMLoc *obj)
{
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;

  *(reinterpret_cast<HTTPHdr**>(bufp)) = info->response_get();
  *obj = reinterpret_cast<TSMLoc>(info->response_get()->m_http);
  sdk_sanity_check_mbuffer(*bufp);
}

time_t
TSCacheHttpInfoReqSentTimeGet(TSCacheHttpInfo infop)
{
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  return info->request_sent_time_get();
}

time_t
TSCacheHttpInfoRespReceivedTimeGet(TSCacheHttpInfo infop)
{
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  return info->response_received_time_get();
}

int64_t
TSCacheHttpInfoSizeGet(TSCacheHttpInfo infop)
{
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  return info->object_size_get();
}

void
TSCacheHttpInfoReqSet(TSCacheHttpInfo infop, TSMBuffer bufp, TSMLoc obj)
{
  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);

  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  info->request_set(&h);
}


void
TSCacheHttpInfoRespSet(TSCacheHttpInfo infop, TSMBuffer bufp, TSMLoc obj)
{
  HTTPHdr h;

  SET_HTTP_HDR(h, bufp, obj);

  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  info->response_set(&h);
}


int
TSCacheHttpInfoVector(TSCacheHttpInfo infop, void *data, int length)
{
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  CacheHTTPInfoVector vector;

  vector.insert(info);

  int size = vector.marshal_length();

  if (size > length)
    // error
    return 0;

  return vector.marshal((char *) data, length);
}


void
TSCacheHttpInfoDestroy(TSCacheHttpInfo infop)
{
  ((CacheHTTPInfo *) infop)->destroy();
}

TSCacheHttpInfo
TSCacheHttpInfoCreate(void)
{
  CacheHTTPInfo *info = new CacheHTTPInfo;
  info->create();

  return reinterpret_cast<TSCacheHttpInfo>(info);
}


////////////////////////////////////////////////////////////////////
//
// Configuration
//
////////////////////////////////////////////////////////////////////

unsigned int
TSConfigSet(unsigned int id, void *data, TSConfigDestroyFunc funcp)
{
  INKConfigImpl *config = NEW(new INKConfigImpl);
  config->mdata = data;
  config->m_destroy_func = funcp;
  return configProcessor.set(id, config);
}

TSConfig
TSConfigGet(unsigned int id)
{
  return reinterpret_cast<TSConfig>(configProcessor.get(id));
}

void
TSConfigRelease(unsigned int id, TSConfig configp)
{
  configProcessor.release(id, (ConfigInfo *) configp);
}

void *
TSConfigDataGet(TSConfig configp)
{
  INKConfigImpl *config = (INKConfigImpl *) configp;
  return config->mdata;
}

////////////////////////////////////////////////////////////////////
//
// Management
//
////////////////////////////////////////////////////////////////////

void
TSMgmtUpdateRegister(TSCont contp, const char *plugin_name)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)plugin_name) == TS_SUCCESS);

  global_config_cbs->insert((INKContInternal *)contp, plugin_name);
}

TSReturnCode
TSMgmtIntGet(const char *var_name, TSMgmtInt *result)
{
  return RecGetRecordInt((char *) var_name, (RecInt *) result) == REC_ERR_OKAY ? TS_SUCCESS : TS_ERROR;
}

TSReturnCode
TSMgmtCounterGet(const char *var_name, TSMgmtCounter *result)
{
  return RecGetRecordCounter((char *) var_name, (RecCounter *) result) == REC_ERR_OKAY ? TS_SUCCESS : TS_ERROR;
}

TSReturnCode
TSMgmtFloatGet(const char *var_name, TSMgmtFloat *result)
{
  return RecGetRecordFloat((char *) var_name, (RecFloat *) result) == REC_ERR_OKAY ? TS_SUCCESS : TS_ERROR;
}

TSReturnCode
TSMgmtStringGet(const char *var_name, TSMgmtString *result)
{
  RecString tmp = 0;
  (void) RecGetRecordString_Xmalloc((char *) var_name, &tmp);

  if (tmp) {
    *result = tmp;
    return TS_SUCCESS;
  }

  return TS_ERROR;
}

////////////////////////////////////////////////////////////////////
//
// Continuations
//
////////////////////////////////////////////////////////////////////

TSCont
TSContCreate(TSEventFunc funcp, TSMutex mutexp)
{
  // mutexp can be NULL
  if (mutexp != NULL)
    sdk_assert(sdk_sanity_check_mutex(mutexp) == TS_SUCCESS);

  INKContInternal *i = INKContAllocator.alloc();

  i->init(funcp, mutexp);
  return (TSCont)i;
}

void
TSContDestroy(TSCont contp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  INKContInternal *i = (INKContInternal *) contp;

  i->destroy();
}

void
TSContDataSet(TSCont contp, void *data)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  INKContInternal *i = (INKContInternal *) contp;

  i->mdata = data;
}

void *
TSContDataGet(TSCont contp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  INKContInternal *i = (INKContInternal *) contp;

  return i->mdata;
}

TSAction
TSContSchedule(TSCont contp, ink_hrtime timeout, TSThreadPool tp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);

  INKContInternal *i = (INKContInternal *) contp;
  TSAction action;

  if (ink_atomic_increment((int *) &i->m_event_count, 1) < 0)
    ink_assert (!"not reached");

  EventType etype;

  switch (tp) {
  case TS_THREAD_POOL_NET:
  case TS_THREAD_POOL_DEFAULT:
    etype = ET_NET;
    break;
  case TS_THREAD_POOL_TASK:
      etype = ET_TASK;
      break;
  case TS_THREAD_POOL_SSL:
    etype = ET_TASK; // Should be ET_SSL
    break;
  case TS_THREAD_POOL_DNS:
    etype = ET_DNS;
    break;
  case TS_THREAD_POOL_REMAP:
    etype = ET_TASK; // Should be ET_REMAP
    break;
  case TS_THREAD_POOL_CLUSTER:
    etype = ET_CLUSTER;
    break;
  case TS_THREAD_POOL_UDP:
    etype = ET_UDP;
    break;
  default:
    etype = ET_TASK;
    break;
  }

  if (timeout == 0) {
    action = reinterpret_cast<TSAction>(eventProcessor.schedule_imm(i, etype));
  } else {
    action = reinterpret_cast<TSAction>(eventProcessor.schedule_in(i, HRTIME_MSECONDS(timeout), etype));
  }

/* This is a hack. SHould be handled in ink_types */
  action = (TSAction) ((uintptr_t) action | 0x1);
  return action;
}

TSAction
TSContScheduleEvery(TSCont contp, ink_hrtime every, TSThreadPool tp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);

  INKContInternal *i = (INKContInternal *) contp;
  TSAction action;

  if (ink_atomic_increment((int *) &i->m_event_count, 1) < 0)
    ink_assert (!"not reached");

  EventType etype;

  switch (tp) {
  case TS_THREAD_POOL_NET:
  case TS_THREAD_POOL_DEFAULT:
    etype = ET_NET;
    break;
  case TS_THREAD_POOL_TASK:
    etype = ET_TASK;
    break;
  default:
    etype = ET_TASK;
    break;
  }

  action = reinterpret_cast<TSAction>(eventProcessor.schedule_every(i, HRTIME_MSECONDS(every), etype));

  /* This is a hack. SHould be handled in ink_types */
  action = (TSAction) ((uintptr_t) action | 0x1);
  return action;
}

TSAction
TSHttpSchedule(TSCont contp, TSHttpTxn txnp, ink_hrtime timeout)
{
  sdk_assert(sdk_sanity_check_iocore_structure (contp) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);

  INKContInternal *i = (INKContInternal *) contp;

  if (ink_atomic_increment((int *) &i->m_event_count, 1) < 0)
    ink_assert (!"not reached");

  TSAction action;
  Continuation *cont  = (Continuation*)contp;
  HttpSM *sm = (HttpSM*)txnp;

  sm->set_http_schedule(cont);

  if (timeout == 0) {
    action = reinterpret_cast<TSAction>(eventProcessor.schedule_imm(sm, ET_NET));
  } else {
    action = reinterpret_cast<TSAction>(eventProcessor.schedule_in(sm, HRTIME_MSECONDS (timeout), ET_NET));
  }

  action = (TSAction) ((uintptr_t) action | 0x1);
  return action;
}

int
TSContCall(TSCont contp, TSEvent event, void *edata)
{
  Continuation *c = (Continuation *) contp;
  return c->handleEvent((int) event, edata);
}

TSMutex
TSContMutexGet(TSCont contp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  Continuation *c = (Continuation *)contp;
  return (TSMutex) ((ProxyMutex *)c->mutex);
}


/* HTTP hooks */

void
TSHttpHookAdd(TSHttpHookID id, TSCont contp)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_hook_id(id) == TS_SUCCESS);

  http_global_hooks->append(id, (INKContInternal *)contp);
}

void
TSHttpIcpDynamicSet(int value)
{
  int32_t old_value, new_value;

  new_value = (value == 0) ? 0 : 1;
  old_value = icp_dynamic_enabled;
  while (old_value != new_value) {
    if (ink_atomic_cas(&icp_dynamic_enabled, old_value, new_value))
      break;
    old_value = icp_dynamic_enabled;
  }
}

/* HTTP sessions */
void
TSHttpSsnHookAdd(TSHttpSsn ssnp, TSHttpHookID id, TSCont contp)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_hook_id(id) == TS_SUCCESS);

  HttpClientSession *cs = (HttpClientSession *) ssnp;
  cs->ssn_hook_append(id, (INKContInternal *) contp);
}

int
TSHttpSsnTransactionCount(TSHttpSsn ssnp)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);

  HttpClientSession* cs = (HttpClientSession*)ssnp;
  return cs->get_transact_count();
}

class TSHttpSsnCallback : public Continuation
{
public:
  TSHttpSsnCallback(HttpClientSession *cs, TSEvent event)
    : Continuation(cs->mutex), m_cs(cs), m_event(event)
  {
    SET_HANDLER(&TSHttpSsnCallback::event_handler);
  }

  int event_handler(int, void*)
  {
    m_cs->handleEvent((int) m_event, 0);
    delete this;
    return 0;
  }

private:
  HttpClientSession *m_cs;
  TSEvent m_event;
};


void
TSHttpSsnReenable(TSHttpSsn ssnp, TSEvent event)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);

  HttpClientSession *cs = (HttpClientSession *) ssnp;
  EThread *eth = this_ethread();

  // If this function is being executed on a thread created by the API
  // which is DEDICATED, the continuation needs to be called back on a
  // REGULAR thread.
  if (eth->tt != REGULAR) {
    eventProcessor.schedule_imm(NEW(new TSHttpSsnCallback(cs, event)), ET_NET);
  } else {
    MUTEX_TRY_LOCK(trylock, cs->mutex, eth);
    if (!trylock) {
      eventProcessor.schedule_imm(NEW(new TSHttpSsnCallback(cs, event)), ET_NET);
    } else {
      cs->handleEvent((int) event, 0);
    }
  }
}


/* HTTP transactions */
void
TSHttpTxnHookAdd(TSHttpTxn txnp, TSHttpHookID id, TSCont contp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_hook_id(id) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  sm->txn_hook_append(id, (INKContInternal *) contp);
}


// Private api function for gzip plugin.
//  This function should only appear in TsapiPrivate.h
TSReturnCode
TSHttpTxnHookRegisteredFor(TSHttpTxn txnp, TSHttpHookID id, TSEventFunc funcp)
{
  HttpSM *sm = (HttpSM *) txnp;
  APIHook *hook = sm->txn_hook_get(id);

  while (hook != NULL) {
    if (hook->m_cont && hook->m_cont->m_event_func == funcp) {
      return TS_SUCCESS;
    }
    hook = hook->m_link.next;
  }

  return TS_ERROR;
}

TSHttpSsn
TSHttpTxnSsnGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return (TSHttpSsn)sm->ua_session;
}

// TODO: Is this still necessary ??
void
TSHttpTxnClientKeepaliveSet(TSHttpTxn txnp, int set)
{
  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  s->hdr_info.trust_response_cl = (set != 0) ? true : false;
}
TSReturnCode
TSHttpTxnClientDataGet(TSHttpTxn txnp, const char **bufp, int *len)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  
  HttpSM *sm = (HttpSM *) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.client_request);
  if (hptr->valid()) {
    *bufp = hptr->m_url_cached.m_url_impl->the_request;
    *len = hptr->m_url_cached.m_url_impl->the_request_len;
    return TS_SUCCESS;
  }
  return TS_ERROR;
}

TSReturnCode
TSHttpTxnClientReqGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.client_request);

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *obj = reinterpret_cast<TSMLoc>(hptr->m_http);
    if (sdk_sanity_check_mbuffer(*bufp) == TS_SUCCESS) {
      hptr->mark_target_dirty();
      return TS_SUCCESS;;
    }
  }
  return TS_ERROR;
}

// pristine url is the url before remap
TSReturnCode
TSHttpTxnPristineUrlGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *url_loc)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)url_loc) == TS_SUCCESS);

  HttpSM *sm = (HttpSM*) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.client_request);

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *url_loc = (TSMLoc)sm->t_state.pristine_url.m_url_impl;

    if ((sdk_sanity_check_mbuffer(*bufp) == TS_SUCCESS) && (*url_loc))
      return TS_SUCCESS;
  }
  return TS_ERROR;
}

// Shortcut to just get the URL.
char*
TSHttpTxnEffectiveUrlStringGet(TSHttpTxn txnp, int *length)
{
  sdk_assert(TS_SUCCESS == sdk_sanity_check_txn(txnp));
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  HttpSM *sm = reinterpret_cast<HttpSM*>(txnp);
  return sm->t_state.hdr_info.client_request.url_string_get(0, length);
}

TSReturnCode
TSHttpTxnClientRespGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.client_response);

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *obj = reinterpret_cast<TSMLoc>(hptr->m_http);
    sdk_sanity_check_mbuffer(*bufp);
    return TS_SUCCESS;
  }

  return TS_ERROR;
}


TSReturnCode
TSHttpTxnServerReqGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.server_request);

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *obj = reinterpret_cast<TSMLoc>(hptr->m_http);
    sdk_sanity_check_mbuffer(*bufp);
    return TS_SUCCESS;
  } 

  return TS_ERROR;
}

TSReturnCode
TSHttpTxnServerRespGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.server_response);

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *obj = reinterpret_cast<TSMLoc>(hptr->m_http);
    sdk_sanity_check_mbuffer(*bufp);
    return TS_SUCCESS;
  }

  return TS_ERROR;
}

TSReturnCode
TSHttpTxnCachedReqGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPInfo *cached_obj = sm->t_state.cache_info.object_read;

  // The following check is need to prevent the HttpSM handle copy from going bad
  // Since the cache manages the header buffer, sm->t_state.cache_info.object_read
  // is the only way to tell if handle has gone bad.
  if ((!cached_obj) || (!cached_obj->valid())) {
    return TS_ERROR;
  }

  HTTPHdr *cached_hdr = sm->t_state.cache_info.object_read->request_get();

  if (!cached_hdr->valid()) {
    return TS_ERROR;
  }
  // We can't use the HdrHeapSDKHandle structure in the RamCache since multiple
  // threads can access. We need to create our own for the transaction and return that.
  HdrHeapSDKHandle **handle = &(sm->t_state.cache_req_hdr_heap_handle);

  if (*handle == NULL) {
    *handle = (HdrHeapSDKHandle *) sm->t_state.arena.alloc(sizeof(HdrHeapSDKHandle));
    (*handle)->m_heap = cached_hdr->m_heap;
  }

  *(reinterpret_cast<HdrHeapSDKHandle**>(bufp)) = *handle;
  *obj = reinterpret_cast<TSMLoc>(cached_hdr->m_http);
  sdk_sanity_check_mbuffer(*bufp);

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnCachedRespGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPInfo *cached_obj = sm->t_state.cache_info.object_read;

  // The following check is need to prevent the HttpSM handle copy from going bad
  // Since the cache manages the header buffer, sm->t_state.cache_info.object_read
  // is the only way to tell if handle has gone bad.
  if ((!cached_obj) || (!cached_obj->valid())) {
    return TS_ERROR;
  }

  HTTPHdr *cached_hdr = sm->t_state.cache_info.object_read->response_get();

  if (!cached_hdr->valid()) {
    return TS_ERROR;
  }
  // We can't use the HdrHeapSDKHandle structure in the RamCache since multiple
  //  threads can access.  We need to create our own for the transaction and return that.
  HdrHeapSDKHandle **handle = &(sm->t_state.cache_resp_hdr_heap_handle);

  if (*handle == NULL) {
    *handle = (HdrHeapSDKHandle *) sm->t_state.arena.alloc(sizeof(HdrHeapSDKHandle));
    (*handle)->m_heap = cached_hdr->m_heap;
  }

  *(reinterpret_cast<HdrHeapSDKHandle**>(bufp)) = *handle;
  *obj = reinterpret_cast<TSMLoc>(cached_hdr->m_http);
  sdk_sanity_check_mbuffer(*bufp);

  return TS_SUCCESS;
}


TSReturnCode
TSHttpTxnCachedRespModifiableGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);
  HTTPHdr *c_resp = NULL;
  HTTPInfo *cached_obj = sm->t_state.cache_info.object_read;
  HTTPInfo *cached_obj_store = &(sm->t_state.cache_info.object_store);

  if ((!cached_obj) || (!cached_obj->valid()))
    return TS_ERROR;

  if (!cached_obj_store->valid())
    cached_obj_store->create();

  c_resp = cached_obj_store->response_get();
  if (c_resp == NULL || !c_resp->valid())
    cached_obj_store->response_set(cached_obj->response_get());
  c_resp = cached_obj_store->response_get();
  s->api_modifiable_cached_resp = true;

  ink_assert(c_resp != NULL && c_resp->valid());
  *(reinterpret_cast<HTTPHdr**>(bufp)) = c_resp;
  *obj = reinterpret_cast<TSMLoc>(c_resp->m_http);
  sdk_sanity_check_mbuffer(*bufp);

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnCacheLookupStatusGet(TSHttpTxn txnp, int *lookup_status)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) lookup_status) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;

  switch (sm->t_state.cache_lookup_result) {
  case HttpTransact::CACHE_LOOKUP_MISS:
  case HttpTransact::CACHE_LOOKUP_DOC_BUSY:
    *lookup_status = TS_CACHE_LOOKUP_MISS;
    break;
  case HttpTransact::CACHE_LOOKUP_HIT_STALE:
    *lookup_status = TS_CACHE_LOOKUP_HIT_STALE;
    break;
  case HttpTransact::CACHE_LOOKUP_HIT_WARNING:
  case HttpTransact::CACHE_LOOKUP_HIT_FRESH:
    *lookup_status = TS_CACHE_LOOKUP_HIT_FRESH;
    break;
  case HttpTransact::CACHE_LOOKUP_SKIPPED:
    *lookup_status = TS_CACHE_LOOKUP_SKIPPED;
    break;
  case HttpTransact::CACHE_LOOKUP_NONE:
  default:
    return TS_ERROR;
  };
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnCacheLookupCountGet(TSHttpTxn txnp, int *lookup_count)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*) lookup_count) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *)txnp;
  *lookup_count = sm->t_state.cache_info.lookup_count;
  return TS_SUCCESS;
}


/* two hooks this function may gets called:
   TS_HTTP_READ_CACHE_HDR_HOOK   &
   TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK
 */
TSReturnCode
TSHttpTxnCacheLookupStatusSet(TSHttpTxn txnp, int cachelookup)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::CacheLookupResult_t *sm_status = &(sm->t_state.cache_lookup_result);

  // converting from a miss to a hit is not allowed
  if (*sm_status == HttpTransact::CACHE_LOOKUP_MISS && cachelookup != TS_CACHE_LOOKUP_MISS)
    return TS_ERROR;

  // here is to handle converting a hit to a miss
  if (cachelookup == TS_CACHE_LOOKUP_MISS && *sm_status != HttpTransact::CACHE_LOOKUP_MISS) {
    sm->t_state.api_cleanup_cache_read = true;
    ink_assert(sm->t_state.transact_return_point != NULL);
    sm->t_state.transact_return_point = HttpTransact::HandleCacheOpenRead;
  }

  switch (cachelookup) {
  case TS_CACHE_LOOKUP_MISS:
    *sm_status = HttpTransact::CACHE_LOOKUP_MISS;
    break;
  case TS_CACHE_LOOKUP_HIT_STALE:
    *sm_status = HttpTransact::CACHE_LOOKUP_HIT_STALE;
    break;
  case TS_CACHE_LOOKUP_HIT_FRESH:
    *sm_status = HttpTransact::CACHE_LOOKUP_HIT_FRESH;
    break;
  default:
    return TS_ERROR;
  }

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnCacheLookupUrlGet(TSHttpTxn txnp, TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  URL u, *l_url;

  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;
  if (!u.valid())
    return TS_ERROR;

  l_url = sm->t_state.cache_info.lookup_url;
  if (l_url && l_url->valid()) {
    u.copy(l_url);
    return TS_SUCCESS;
  }

  return TS_ERROR;
}

TSReturnCode
TSHttpTxnCachedUrlSet(TSHttpTxn txnp, TSMBuffer bufp, TSMLoc obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(obj) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  URL u, *s_url;

  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) obj;
  if (!u.valid())
    return TS_ERROR;

  s_url = &(sm->t_state.cache_info.store_url);
  if (!s_url->valid())
    s_url->create(NULL);
  s_url->copy(&u);
  if (sm->decide_cached_url(&u))
    return TS_SUCCESS;

  return TS_ERROR;
}

TSReturnCode
TSHttpTxnNewCacheLookupDo(TSHttpTxn txnp, TSMBuffer bufp, TSMLoc url_loc)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(url_loc) == TS_SUCCESS);

  URL new_url, *client_url, *l_url, *o_url;
  INK_MD5 md51, md52;

  new_url.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  new_url.m_url_impl = (URLImpl *) url_loc;
  if (!new_url.valid())
    return TS_ERROR;

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  client_url = s->hdr_info.client_request.url_get();
  if (!(client_url->valid()))
    return TS_ERROR;

  // if l_url is not valid, then no cache lookup has been done yet
  // so we shouldn't be calling TSHttpTxnNewCacheLookupDo right now
  l_url = s->cache_info.lookup_url;
  if (!l_url || !l_url->valid()) {
    s->cache_info.lookup_url_storage.create(NULL);
    s->cache_info.lookup_url = &(s->cache_info.lookup_url_storage);
    l_url = s->cache_info.lookup_url;
  } else {
    l_url->MD5_get(&md51);
    new_url.MD5_get(&md52);
    if (md51 == md52)
      return TS_ERROR;
    o_url = &(s->cache_info.original_url);
    if (!o_url->valid()) {
      o_url->create(NULL);
      o_url->copy(l_url);
    }
  }

  // copy the new_url to lookup_url
  l_url->copy(&new_url);

  // bypass HttpTransact::HandleFiltering
  s->transact_return_point = HttpTransact::DecideCacheLookup;
  s->cache_info.action = HttpTransact::CACHE_DO_LOOKUP;
  sm->add_cache_sm();
  s->api_cleanup_cache_read = true;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnSecondUrlTryLock(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);
  // TSHttpTxnNewCacheLookupDo didn't continue
  if (!s->cache_info.original_url.valid())
    return TS_ERROR;
  sm->add_cache_sm();
  s->api_lock_url = HttpTransact::LOCK_URL_SECOND;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnFollowRedirect(TSHttpTxn txnp, int on)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;

  sm->api_enable_redirection = (on ? true : false);
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnRedirectRequest(TSHttpTxn txnp, TSMBuffer bufp, TSMLoc url_loc)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_mbuffer(bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_url_handle(url_loc) == TS_SUCCESS);

  URL u, *o_url, *r_url, *client_url;
  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  u.m_heap = ((HdrHeapSDKHandle *) bufp)->m_heap;
  u.m_url_impl = (URLImpl *) url_loc;
  if (!u.valid())
    return TS_ERROR;

  client_url = s->hdr_info.client_request.url_get();
  if (!(client_url->valid()))
    return TS_ERROR;

  s->redirect_info.redirect_in_process = true;
  o_url = &(s->redirect_info.original_url);
  if (!o_url->valid()) {
    o_url->create(NULL);
    o_url->copy(client_url);
  }
  client_url->copy(&u);

  r_url = &(s->redirect_info.redirect_url);
  if (!r_url->valid())
    r_url->create(NULL);
  r_url->copy(&u);

  s->hdr_info.server_request.destroy();
  // we want to close the server session
  s->api_release_server_session = true;

  s->request_sent_time = 0;
  s->response_received_time = 0;
  s->cache_info.write_lock_state = HttpTransact::CACHE_WL_INIT;
  s->next_action = HttpTransact::REDIRECT_READ;

  return TS_SUCCESS;
}

/**
 * timeout is in msec
 * overrides as proxy.config.http.transaction_active_timeout_out
**/
void
TSHttpTxnActiveTimeoutSet(TSHttpTxn txnp, int timeout)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->api_txn_active_timeout_value = timeout;
}

/**
 * timeout is in msec
 * overrides as proxy.config.http.connect_attempts_timeout
**/
void
TSHttpTxnConnectTimeoutSet(TSHttpTxn txnp, int timeout)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->api_txn_connect_timeout_value = timeout;
}

/**
 * timeout is in msec
 * overrides as proxy.config.dns.lookup_timeout
**/
void
TSHttpTxnDNSTimeoutSet(TSHttpTxn txnp, int timeout)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);

  s->api_txn_dns_timeout_value = timeout;
}


/**
 * timeout is in msec
 * overrides as proxy.config.http.transaction_no_activity_timeout_out
**/
void
TSHttpTxnNoActivityTimeoutSet(TSHttpTxn txnp, int timeout)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->api_txn_no_activity_timeout_value = timeout;
}


TSReturnCode
TSHttpTxnCacheLookupSkip(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->api_skip_cache_lookup = true;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnServerRespNoStore(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->api_server_response_no_store = true;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnServerRespIgnore(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  HTTPInfo *cached_obj = s->cache_info.object_read;
  HTTPHdr *cached_resp;

  if (cached_obj == NULL || !cached_obj->valid())
    return TS_ERROR;

  cached_resp = cached_obj->response_get();
  if (cached_resp == NULL || !cached_resp->valid())
    return TS_ERROR;

  s->api_server_response_ignore = true;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnShutDown(TSHttpTxn txnp, TSEvent event)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  if (event == TS_EVENT_HTTP_TXN_CLOSE)
    return TS_ERROR;

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->api_http_sm_shutdown = true;

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnAborted(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  switch (sm->t_state.squid_codes.log_code) {
  case SQUID_LOG_ERR_CLIENT_ABORT:
  case SQUID_LOG_ERR_CLIENT_ABORT_HIT:
  case SQUID_LOG_ERR_CLIENT_ABORT_MISS:
  case SQUID_LOG_TCP_SWAPFAIL:
    // check for client abort and cache read error
    return TS_SUCCESS;
  default:
    break;
  }

  if (sm->t_state.current.server && sm->t_state.current.server->abort == HttpTransact::ABORTED) {
    // check for the server abort
    return TS_SUCCESS;
  }
  // there can be the case of transformation error.
  return TS_ERROR;
}

void
TSHttpTxnReqCacheableSet(TSHttpTxn txnp, int flag)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM* sm = (HttpSM*)txnp;
  sm->t_state.api_req_cacheable = (flag != 0);
}

void
TSHttpTxnRespCacheableSet(TSHttpTxn txnp, int flag)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM* sm = (HttpSM*)txnp;
  sm->t_state.api_resp_cacheable = (flag != 0);
}

int
TSHttpTxnClientReqIsServerStyle(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return (sm->t_state.hdr_info.client_req_is_server_style ? 1 : 0);
}

void
TSHttpTxnOverwriteExpireTime(TSHttpTxn txnp, time_t expire_time)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  s->plugin_set_expire_time = expire_time;
}

TSReturnCode
TSHttpTxnUpdateCachedObject(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);
  HTTPInfo *cached_obj_store = &(sm->t_state.cache_info.object_store);
  HTTPHdr *client_request = &(sm->t_state.hdr_info.client_request);

  if (!cached_obj_store->valid() || !cached_obj_store->response_get())
    return TS_ERROR;

  if (!cached_obj_store->request_get() && !client_request->valid())
    return TS_ERROR;

  if (s->cache_info.write_lock_state == HttpTransact::CACHE_WL_READ_RETRY)
    return TS_ERROR;

  s->api_update_cached_object = HttpTransact::UPDATE_CACHED_OBJECT_PREPARE;
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnTransformRespGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPHdr *hptr = &(sm->t_state.hdr_info.transform_response);

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *obj = reinterpret_cast<TSMLoc>(hptr->m_http);
    return sdk_sanity_check_mbuffer(*bufp);
  } 

  return TS_ERROR;
}

sockaddr const*
TSHttpSsnClientAddrGet(TSHttpSsn ssnp)
{
  HttpClientSession *cs = reinterpret_cast<HttpClientSession *>(ssnp);

  if (cs == NULL) return 0;

  NetVConnection *vc = cs->get_netvc();
  if (vc == NULL) return 0;

  return vc->get_remote_addr();
}
sockaddr const*
TSHttpTxnClientAddrGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
 
  TSHttpSsn ssnp = TSHttpTxnSsnGet(txnp);
  return TSHttpSsnClientAddrGet(ssnp);
}

sockaddr const*
TSHttpSsnIncomingAddrGet(TSHttpSsn ssnp) {
  HttpClientSession *cs = reinterpret_cast<HttpClientSession *>(ssnp);

  if (cs == NULL) return 0;

  NetVConnection *vc = cs->get_netvc();
  if (vc == NULL) return 0;

  return vc->get_local_addr();
}
sockaddr const*
TSHttpTxnIncomingAddrGet(TSHttpTxn txnp) {
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
 
  TSHttpSsn ssnp = TSHttpTxnSsnGet(txnp);
  return TSHttpSsnIncomingAddrGet(ssnp);
}

sockaddr const*
TSHttpTxnServerAddrGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = reinterpret_cast<HttpSM *>(txnp);
  return &sm->t_state.server_info.addr.sa;
}

TSReturnCode
TSHttpTxnServerAddrSet(TSHttpTxn txnp, struct sockaddr const* addr)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = reinterpret_cast<HttpSM *>(txnp);
  if (ats_ip_copy(&sm->t_state.server_info.addr.sa, addr)) {
    sm->t_state.api_server_addr_set = true;
    return TS_SUCCESS;
  } else {
    return TS_ERROR;
  }
}


// [amc] This might use the port. The code path should do that but it
// hasn't been tested.
TSReturnCode
TSHttpTxnOutgoingAddrSet(TSHttpTxn txnp, const struct sockaddr *addr, socklen_t addrlen)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  HttpSM *sm = (HttpSM *) txnp;

  sm->ua_session->outbound_port = ats_ip_port_host_order(addr);

  if (ats_is_ip4(addr)) {
    sm->ua_session->outbound_ip4.assign(addr);
  } else if (ats_is_ip6(addr)) {
    sm->ua_session->outbound_ip6.assign(addr);
  } else {
    sm->ua_session->outbound_ip4.invalidate();
    sm->ua_session->outbound_ip6.invalidate();
  }
  return TS_ERROR;
}

sockaddr const*
TSHttpTxnNextHopAddrGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = reinterpret_cast<HttpSM *>(txnp);

    /**
     * Return zero if the server structure is not yet constructed.
     */
  if (sm->t_state.current.server == NULL)
    return NULL;

  return &sm->t_state.current.server->addr.sa;
}

TSReturnCode
TSHttpTxnOutgoingTransparencySet(TSHttpTxn txnp, int flag)
{
  if (TS_SUCCESS != sdk_sanity_check_txn(txnp)) return TS_ERROR;

# if ! TS_USE_TPROXY
  if (flag) return TS_ERROR; // can't enable if it's not configured.
# endif

  HttpSM *sm = reinterpret_cast<HttpSM*>(txnp);
  sm->ua_session->f_outbound_transparent = flag;
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnClientPacketMarkSet(TSHttpTxn txnp, int mark)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  HttpSM *sm = (HttpSM *) txnp;
  if (NULL == sm->ua_session) {
    return TS_ERROR;
  }

  NetVConnection *vc = sm->ua_session->get_netvc();
  if (NULL == vc) {
    return TS_ERROR;
  }

  vc->options.packet_mark = (uint32_t)mark;
  vc->apply_options();
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnServerPacketMarkSet(TSHttpTxn txnp, int mark)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  HttpSM *sm = (HttpSM *) txnp;

  // change the mark on an active server session
  if (NULL != sm->ua_session) {
    HttpServerSession *ssn = sm->ua_session->get_server_session();
    if (NULL != ssn) {
      NetVConnection *vc = ssn->get_netvc();
      if (vc != NULL) {
        vc->options.packet_mark = (uint32_t)mark;
        vc->apply_options();
      }
    }
  }

  // update the transactions mark config for future connections
  TSHttpTxnConfigIntSet(txnp, TS_CONFIG_NET_SOCK_PACKET_MARK_OUT, mark);
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnClientPacketTosSet(TSHttpTxn txnp, int tos)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  HttpSM *sm = (HttpSM *) txnp;
  if (NULL == sm->ua_session) {
    return TS_ERROR;
  }

  NetVConnection *vc = sm->ua_session->get_netvc();
  if (NULL == vc) {
    return TS_ERROR;
  }

  vc->options.packet_tos = (uint32_t)tos;
  vc->apply_options();
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnServerPacketTosSet(TSHttpTxn txnp, int tos)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  HttpSM *sm = (HttpSM *) txnp;

  // change the tos on an active server session
  if (NULL != sm->ua_session) {
    HttpServerSession *ssn = sm->ua_session->get_server_session();
    if (NULL != ssn) {
      NetVConnection *vc = ssn->get_netvc();
      if (vc != NULL) {
        vc->options.packet_tos = (uint32_t)tos;
        vc->apply_options();
      }
    }
  }

  // update the transactions mark config for future connections
  TSHttpTxnConfigIntSet(txnp, TS_CONFIG_NET_SOCK_PACKET_TOS_OUT, tos);
  return TS_SUCCESS;
}

void
TSHttpTxnErrorBodySet(TSHttpTxn txnp, char *buf, int buflength, char *mimetype)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)buf) == TS_SUCCESS);
  sdk_assert(buflength > 0);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  if (s->internal_msg_buffer)
    HttpTransact::free_internal_msg_buffer(s->internal_msg_buffer, s->internal_msg_buffer_fast_allocator_size);

  ats_free_null(s->internal_msg_buffer_type);

  s->internal_msg_buffer = buf;
  s->internal_msg_buffer_type = mimetype;
  s->internal_msg_buffer_size = buflength;
  s->internal_msg_buffer_fast_allocator_size = -1;
}

void
TSHttpTxnServerRequestBodySet(TSHttpTxn txnp, char *buf, int64_t buflength)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)buf) == TS_SUCCESS);
  sdk_assert(buflength > 0);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  if (s->method != HTTP_WKSIDX_GET)
    return;

  if (s->internal_msg_buffer)
    HttpTransact::free_internal_msg_buffer(s->internal_msg_buffer, s->internal_msg_buffer_fast_allocator_size);

  s->api_server_request_body_set = true;
  s->internal_msg_buffer = buf;
  s->internal_msg_buffer_size = buflength;
  s->internal_msg_buffer_fast_allocator_size = -1;
}

TSReturnCode
TSHttpTxnParentProxyGet(TSHttpTxn txnp, char **hostname, int *port)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;

  *hostname = sm->t_state.api_info.parent_proxy_name;
  *port = sm->t_state.api_info.parent_proxy_port;

  return TS_SUCCESS;
}

void
TSHttpTxnParentProxySet(TSHttpTxn txnp, char *hostname, int port)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)hostname) == TS_SUCCESS);
  sdk_assert(port > 0);

  HttpSM *sm = (HttpSM *) txnp;

  sm->t_state.api_info.parent_proxy_name = sm->t_state.arena.str_store(hostname, strlen(hostname));
  sm->t_state.api_info.parent_proxy_port = port;
}

void
TSHttpTxnUntransformedRespCache(TSHttpTxn txnp, int on)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  sm->t_state.api_info.cache_untransformed = (on ? true : false);
}

void
TSHttpTxnTransformedRespCache(TSHttpTxn txnp, int on)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  sm->t_state.api_info.cache_transformed = (on ? true : false);
}


class TSHttpSMCallback:public Continuation
{
public:
  TSHttpSMCallback(HttpSM *sm, TSEvent event)
    : Continuation(sm->mutex), m_sm(sm), m_event(event)
  {
    SET_HANDLER(&TSHttpSMCallback::event_handler);
  }

  int event_handler(int, void*)
  {
    m_sm->state_api_callback((int) m_event, 0);
    delete this;
    return 0;
  }

private:
  HttpSM *m_sm;
  TSEvent m_event;
};


//----------------------------------------------------------------------------
void
TSHttpTxnReenable(TSHttpTxn txnp, TSEvent event)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  EThread *eth = this_ethread();

  // If this function is being executed on a thread created by the API
  // which is DEDICATED, the continuation needs to be called back on a
  // REGULAR thread.
  if (eth->tt != REGULAR) {
    eventProcessor.schedule_imm(NEW(new TSHttpSMCallback(sm, event)), ET_NET);
  } else {
    MUTEX_TRY_LOCK(trylock, sm->mutex, eth);
    if (!trylock) {
      eventProcessor.schedule_imm(NEW(new TSHttpSMCallback(sm, event)), ET_NET);
    } else {
      sm->state_api_callback((int) event, 0);
    }
  }
}

TSReturnCode
TSHttpArgIndexReserve(const char* name, const char* description, int *arg_idx)
{
  sdk_assert(sdk_sanity_check_null_ptr(arg_idx) == TS_SUCCESS);

  int volatile ix = ink_atomic_increment(&next_argv_index, 1);

  if (ix < HTTP_SSN_TXN_MAX_USER_ARG) {
    state_arg_table[ix].name = ats_strdup(name);
    state_arg_table[ix].name_len = strlen(state_arg_table[ix].name);
    if (description)
      state_arg_table[ix].description = ats_strdup(description);
    *arg_idx = ix;

    return TS_SUCCESS;
  }
  return TS_ERROR;
}

TSReturnCode
TSHttpArgIndexLookup(int arg_idx, const char** name, const char** description)
{
  if (sdk_sanity_check_null_ptr(name) == TS_SUCCESS) {
    if (state_arg_table[arg_idx].name) {
      *name = state_arg_table[arg_idx].name;
      if (description)
        *description = state_arg_table[arg_idx].description;
      return TS_SUCCESS;
    }
  }
  return TS_ERROR;
}

// Not particularly efficient, but good enough for now.
TSReturnCode
TSHttpArgIndexNameLookup(const char* name, int *arg_idx, const char **description)
{
  sdk_assert(sdk_sanity_check_null_ptr(arg_idx) == TS_SUCCESS);

  size_t len = strlen(name);

  for (int ix = 0; ix <  next_argv_index; ++ix) {
    if ((len == state_arg_table[ix].name_len) && (0 == strcmp(name, state_arg_table[ix].name))) {
      if (description)
        *description = state_arg_table[ix].description;
      *arg_idx = ix;
      return TS_SUCCESS;
    }
  }
  return TS_ERROR;
}

void
TSHttpTxnArgSet(TSHttpTxn txnp, int arg_idx, void *arg)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(arg_idx >= 0 && arg_idx < HTTP_SSN_TXN_MAX_USER_ARG);

  HttpSM *sm = (HttpSM *) txnp;
  sm->t_state.user_args[arg_idx] = arg;
}

void *
TSHttpTxnArgGet(TSHttpTxn txnp, int arg_idx)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(arg_idx >= 0 && arg_idx < HTTP_SSN_TXN_MAX_USER_ARG);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->t_state.user_args[arg_idx];
}

void
TSHttpSsnArgSet(TSHttpSsn ssnp, int arg_idx, void *arg)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);
  sdk_assert(arg_idx >= 0 && arg_idx < HTTP_SSN_TXN_MAX_USER_ARG);

  HttpClientSession *cs = (HttpClientSession *)ssnp;

  cs->set_user_arg(arg_idx, arg);
}

void *
TSHttpSsnArgGet(TSHttpSsn ssnp, int arg_idx)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);
  sdk_assert(arg_idx >= 0 && arg_idx < HTTP_SSN_TXN_MAX_USER_ARG);

  HttpClientSession *cs = (HttpClientSession *)ssnp;
  return cs->get_user_arg(arg_idx);
}

void
TSHttpTxnSetHttpRetStatus(TSHttpTxn txnp, TSHttpStatus http_retstatus)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  sm->t_state.http_return_code = (HTTPStatus) http_retstatus;
}

int
TSHttpTxnGetMaxHttpRetBodySize(void)
{
  return HTTP_TRANSACT_STATE_MAX_XBUF_SIZE;
}

void
TSHttpTxnSetHttpRetBody(TSHttpTxn txnp, const char *body_msg, int plain_msg_flag)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  s->return_xbuf_size = 0;
  s->return_xbuf[0] = 0;
  s->return_xbuf_plain = false;
  if (body_msg) {
    ink_strlcpy(s->return_xbuf, body_msg, HTTP_TRANSACT_STATE_MAX_XBUF_SIZE);
    s->return_xbuf_size = strlen(s->return_xbuf);
    s->return_xbuf_plain = plain_msg_flag;
  }
}

/* control channel for HTTP */
TSReturnCode
TSHttpTxnCntl(TSHttpTxn txnp, TSHttpCntlType cntl, void *data)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;

  switch (cntl) {
  case TS_HTTP_CNTL_GET_LOGGING_MODE:
    {
      if (data == NULL)
        return TS_ERROR;

      intptr_t *rptr = (intptr_t *) data;

      if (sm->t_state.api_info.logging_enabled) {
        *rptr = (intptr_t) TS_HTTP_CNTL_ON;
      } else {
        *rptr = (intptr_t) TS_HTTP_CNTL_OFF;
      }

      return TS_SUCCESS;
    }

  case TS_HTTP_CNTL_SET_LOGGING_MODE:
    if (data != TS_HTTP_CNTL_ON && data != TS_HTTP_CNTL_OFF) {
      return TS_ERROR;
    } else {
      sm->t_state.api_info.logging_enabled = (bool) data;
      return TS_SUCCESS;
    }
    break;

  case TS_HTTP_CNTL_GET_INTERCEPT_RETRY_MODE:
    {
      if (data == NULL)
        return TS_ERROR;

      intptr_t *rptr = (intptr_t *) data;

      if (sm->t_state.api_info.retry_intercept_failures) {
        *rptr = (intptr_t) TS_HTTP_CNTL_ON;
      } else {
        *rptr = (intptr_t) TS_HTTP_CNTL_OFF;
      }

      return TS_SUCCESS;
    }

  case TS_HTTP_CNTL_SET_INTERCEPT_RETRY_MODE:
    if (data != TS_HTTP_CNTL_ON && data != TS_HTTP_CNTL_OFF) {
      return TS_ERROR;
    } else {
      sm->t_state.api_info.retry_intercept_failures = (bool) data;
      return TS_SUCCESS;
    }
  default:
    return TS_ERROR;
  }

  return TS_ERROR;
}

/* This is kinda horky, we have to use TSServerState instead of
   HttpTransact::ServerState_t, otherwise we have a prototype
   mismatch in the public ts/ts.h interfaces. */
TSServerState
TSHttpTxnServerStateGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpTransact::State *s = &(((HttpSM *) txnp)->t_state);
  return (TSServerState)s->current.state;
}

void
TSHttpTxnDebugSet(TSHttpTxn txnp, int on)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  ((HttpSM *)txnp)->debug_on = on;
}

int
TSHttpTxnDebugGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  return ((HttpSM *)txnp)->debug_on;
}
void
TSHttpSsnDebugSet(TSHttpSsn ssnp, int on)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);
  ((HttpClientSession *)ssnp)->debug_on = on;
}
int
TSHttpSsnDebugGet(TSHttpSsn ssnp)
{
  sdk_assert(sdk_sanity_check_http_ssn(ssnp) == TS_SUCCESS);
  return ((HttpClientSession *)ssnp)->debug_on;
}

int
TSHttpTxnClientReqHdrBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->client_request_hdr_bytes;
}

int64_t
TSHttpTxnClientReqBodyBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->client_request_body_bytes;
}

int
TSHttpTxnServerReqHdrBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->server_request_hdr_bytes;
}

int64_t
TSHttpTxnServerReqBodyBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->server_request_body_bytes;
}

int
TSHttpTxnServerRespHdrBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->server_response_hdr_bytes;
}

int64_t
TSHttpTxnServerRespBodyBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->server_response_body_bytes;
}

int
TSHttpTxnClientRespHdrBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->client_response_hdr_bytes;
}

int64_t
TSHttpTxnClientRespBodyBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->client_response_body_bytes;
}

int
TSHttpTxnPushedRespHdrBytesGet(TSHttpTxn txnp, int *bytes)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->pushed_response_hdr_bytes;
}

int64_t
TSHttpTxnPushedRespBodyBytesGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  return sm->pushed_response_body_bytes;
}

TSReturnCode
TSHttpTxnStartTimeGet(TSHttpTxn txnp, ink_hrtime *start_time)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;

  if (sm->milestones.ua_begin == 0)
    return TS_ERROR;

  *start_time = sm->milestones.ua_begin;
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnEndTimeGet(TSHttpTxn txnp, ink_hrtime *end_time)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;

  if (sm->milestones.ua_close == 0)
    return TS_ERROR;

  *end_time = sm->milestones.ua_close;
  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnCachedRespTimeGet(TSHttpTxn txnp, time_t *resp_time)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HTTPInfo *cached_obj = sm->t_state.cache_info.object_read;

  if (cached_obj == NULL || !cached_obj->valid())
    return TS_ERROR;

  *resp_time = cached_obj->response_received_time_get();
  return TS_SUCCESS;
}

int
TSHttpTxnLookingUpTypeGet(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  HttpTransact::State *s = &(sm->t_state);

  return (int)(s->current.request_to);
}

int
TSHttpCurrentClientConnectionsGet(void)
{
  int64_t S;

  HTTP_READ_DYN_SUM(http_current_client_connections_stat, S);
  return (int)S;
}

int
TSHttpCurrentActiveClientConnectionsGet(void)
{
  int64_t S;

  HTTP_READ_DYN_SUM(http_current_active_client_connections_stat, S);
  return (int)S;
}

int
TSHttpCurrentIdleClientConnectionsGet(void)
{
  int64_t total = 0;
  int64_t active = 0;

  HTTP_READ_DYN_SUM(http_current_client_connections_stat, total);
  HTTP_READ_DYN_SUM(http_current_active_client_connections_stat, active);

  if (total >= active)
    return (int)(total - active);

  return 0;
}

int
TSHttpCurrentCacheConnectionsGet(void)
{
  int64_t S;

  HTTP_READ_DYN_SUM(http_current_cache_connections_stat, S);
  return (int)S;
}

int
TSHttpCurrentServerConnectionsGet(void)
{
  int64_t S;

  HTTP_READ_GLOBAL_DYN_SUM(http_current_server_connections_stat, S);
  return (int)S;
}


/* HTTP alternate selection */
TSReturnCode
TSHttpAltInfoClientReqGet(TSHttpAltInfo infop, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_alt_info(infop) == TS_SUCCESS);

  HttpAltInfo *info = (HttpAltInfo *) infop;

  *(reinterpret_cast<HTTPHdr**>(bufp)) = &info->m_client_req;
  *obj = reinterpret_cast<TSMLoc>(info->m_client_req.m_http);

  return sdk_sanity_check_mbuffer(*bufp);
}

TSReturnCode
TSHttpAltInfoCachedReqGet(TSHttpAltInfo infop, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_alt_info(infop) == TS_SUCCESS);

  HttpAltInfo *info = (HttpAltInfo *) infop;

  *(reinterpret_cast<HTTPHdr**>(bufp)) = &info->m_cached_req;
  *obj = reinterpret_cast<TSMLoc>(info->m_cached_req.m_http);

  return sdk_sanity_check_mbuffer(*bufp);
}

TSReturnCode
TSHttpAltInfoCachedRespGet(TSHttpAltInfo infop, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_alt_info(infop) == TS_SUCCESS);

  HttpAltInfo *info = (HttpAltInfo *) infop;

  *(reinterpret_cast<HTTPHdr**>(bufp)) = &info->m_cached_resp;
  *obj = reinterpret_cast<TSMLoc>(info->m_cached_resp.m_http);

  return sdk_sanity_check_mbuffer(*bufp);
}

void
TSHttpAltInfoQualitySet(TSHttpAltInfo infop, float quality)
{
  sdk_assert(sdk_sanity_check_alt_info(infop) == TS_SUCCESS);

  HttpAltInfo *info = (HttpAltInfo *) infop;
  info->m_qvalue = quality;
}

extern HttpAcceptCont *plugin_http_accept;
extern HttpAcceptCont *plugin_http_transparent_accept;

TSVConn
TSHttpConnect(sockaddr const* addr)
{
  return TSHttpConnectWithProtoType(addr, TS_NET_PROTO_HTTP);
}

TSVConn
TSHttpConnectWithProtoType(sockaddr const* addr, TSNetProtoType proto_type)
{
  sdk_assert(addr);

  sdk_assert(ats_is_ip(addr));
  sdk_assert(ats_ip_port_cast(addr));

  if (plugin_http_accept) {
    PluginVCCore *new_pvc = PluginVCCore::alloc();

    new_pvc->set_active_addr(addr);
    new_pvc->set_accept_cont(plugin_http_accept);

    new_pvc->active_vc.proto_type = (NetProtoType)proto_type;
    new_pvc->passive_vc.proto_type = (NetProtoType)proto_type;

    PluginVC *return_vc = new_pvc->connect();

    if (return_vc != NULL) {
      PluginVC* other_side = return_vc->get_other_side();

      if(other_side != NULL) {
        other_side->set_is_internal_request(true);
      }
    }

    return reinterpret_cast<TSVConn>(return_vc);
  }

  return NULL;
}

TSVConn
TSHttpConnectTransparent(sockaddr const* client_addr, sockaddr const* server_addr)
{
  sdk_assert(ats_is_ip(client_addr));
  sdk_assert(ats_is_ip(server_addr));
  sdk_assert(!ats_is_ip_any(client_addr));
  sdk_assert(ats_ip_port_cast(client_addr));
  sdk_assert(!ats_is_ip_any(server_addr));
  sdk_assert(ats_ip_port_cast(server_addr));

  if (plugin_http_transparent_accept) {
    PluginVCCore *new_pvc = PluginVCCore::alloc();

    // set active address expects host ordering and the above casts do not
    // swap when it is required
    new_pvc->set_active_addr(client_addr);
    new_pvc->set_passive_addr(server_addr);
    new_pvc->set_transparent(true, true);
    new_pvc->set_accept_cont(plugin_http_transparent_accept);

    PluginVC *return_vc = new_pvc->connect();

    if (return_vc != NULL) {
      PluginVC* other_side = return_vc->get_other_side();

      if(other_side != NULL) {
        other_side->set_is_internal_request(true);
      }
    }

    return reinterpret_cast<TSVConn>(return_vc);
  }

  return NULL;
}

/* Actions */
void
TSActionCancel(TSAction actionp)
{
  Action *a;
  INKContInternal *i;

/* This is a hack. SHould be handled in ink_types */
  if ((uintptr_t) actionp & 0x1) {
    a = (Action *) ((uintptr_t) actionp - 1);
    i = (INKContInternal *) a->continuation;
    i->handle_event_count(EVENT_IMMEDIATE);
  } else {
    a = (Action *) actionp;
  }

  a->cancel();
}

// Currently no error handling necessary, actionp can be anything.
int
TSActionDone(TSAction actionp)
{
  return ((Action *)actionp == ACTION_RESULT_DONE) ? 1 : 0;
}

/* Connections */

/* Deprectated.
   Do not use this API.
   The reason is even if VConn is created using this API, it is still useless.
   For example, if we do TSVConnRead, the read operation returns read_vio, if
   we do TSVIOReenable (read_vio), it actually calls:
   void VIO::reenable()
   {
       if (vc_server) vc_server->reenable(this);
   }
   vc_server->reenable calls:
   VConnection::reenable(VIO)

   this function is virtual in VConnection.h. It is defined separately for
   UnixNet, NTNet and CacheVConnection.

   Thus, unless VConn is either NetVConnection or CacheVConnection, it can't
   be instantiated for functions like reenable.

   Meanwhile, this function has never been used.
   */
TSVConn
TSVConnCreate(TSEventFunc event_funcp, TSMutex mutexp)
{
  if (mutexp == NULL)
    mutexp = (TSMutex) new_ProxyMutex();

  // TODO: probably don't need this if memory allocations fails properly
  sdk_assert(sdk_sanity_check_mutex(mutexp) == TS_SUCCESS);

  INKVConnInternal *i = INKVConnAllocator.alloc();

  sdk_assert(sdk_sanity_check_null_ptr((void*)i) == TS_SUCCESS);

  i->init(event_funcp, mutexp);
  return reinterpret_cast<TSVConn>(i);
}

TSVIO
TSVConnReadVIOGet(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *)connp;
  TSVIO data;

  if (vc->get_data(TS_API_DATA_READ_VIO, &data)) {
    return data;
  }

  return NULL;
}

TSVIO
TSVConnWriteVIOGet(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *) connp;
  TSVIO data;

  if (vc->get_data(TS_API_DATA_WRITE_VIO, &data)) {
    return data;
  }

  return NULL;
}

int
TSVConnClosedGet(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *) connp;
  int data;

  vc->get_data(TS_API_DATA_CLOSED, &data); // Can not fail for this case
  return data;
}

TSVIO
TSVConnRead(TSVConn connp, TSCont contp, TSIOBuffer bufp, int64_t nbytes)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(bufp) == TS_SUCCESS);
  sdk_assert(nbytes >= 0);

  FORCE_PLUGIN_MUTEX(contp);
  VConnection *vc = (VConnection *) connp;

  return reinterpret_cast<TSVIO>(vc->do_io(VIO::READ, (INKContInternal *) contp, nbytes, (MIOBuffer *) bufp));
}

TSVIO
TSVConnWrite(TSVConn connp, TSCont contp, TSIOBufferReader readerp, int64_t nbytes)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_iocore_structure(readerp) == TS_SUCCESS);
  sdk_assert(nbytes >= 0);

  FORCE_PLUGIN_MUTEX(contp);
  VConnection *vc = (VConnection *) connp;

  return reinterpret_cast<TSVIO>(vc->do_io_write((INKContInternal *) contp, nbytes, (IOBufferReader *) readerp));
}

void
TSVConnClose(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *) connp;
  vc->do_io_close();
}

void
TSVConnAbort(TSVConn connp, int error)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *) connp;
  vc->do_io_close(error);
}

void
TSVConnShutdown(TSVConn connp, int read, int write)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *) connp;

  if (read && write) {
    vc->do_io_shutdown(IO_SHUTDOWN_READWRITE);
  } else if (read) {
    vc->do_io_shutdown(IO_SHUTDOWN_READ);
  } else if (write) {
    vc->do_io_shutdown(IO_SHUTDOWN_WRITE);
  }
}

int64_t
TSVConnCacheObjectSizeGet(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  CacheVC *vc = (CacheVC *)connp;
  return vc->get_object_size();
}

void
TSVConnCacheHttpInfoGet(TSVConn connp, TSCacheHttpInfo *infop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  CacheVConnection *vc = (CacheVConnection *) connp;
  vc->get_http_info((CacheHTTPInfo **) infop);
}

void
TSVConnCacheHttpInfoSet(TSVConn connp, TSCacheHttpInfo infop)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  CacheVC *vc = (CacheVC *) connp;
  if (vc->base_stat == cache_scan_active_stat)
    vc->set_http_info((CacheHTTPInfo *) infop);
}

/* Transformations */

TSVConn
TSTransformCreate(TSEventFunc event_funcp, TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  // TODO: This is somewhat of a leap of faith, but I think a TSHttpTxn is just another
  // fancy continuation?
  return TSVConnCreate(event_funcp, TSContMutexGet(reinterpret_cast<TSCont>(txnp)));
}

TSVConn
TSTransformOutputVConnGet(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  VConnection *vc = (VConnection *) connp;
  TSVConn data;

  vc->get_data(TS_API_DATA_OUTPUT_VC, &data); // This case can't fail.
  return data;
}

void
TSHttpTxnServerIntercept(TSCont contp, TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);

  HttpSM *http_sm = (HttpSM *)txnp;
  INKContInternal *i = (INKContInternal *)contp;

  // Must have a mutex
  sdk_assert(sdk_sanity_check_null_ptr((void*)i->mutex) == TS_SUCCESS);

  http_sm->plugin_tunnel_type = HTTP_PLUGIN_AS_SERVER;
  http_sm->plugin_tunnel = PluginVCCore::alloc();
  http_sm->plugin_tunnel->set_accept_cont(i);
}

void
TSHttpTxnIntercept(TSCont contp, TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);

  HttpSM *http_sm = (HttpSM *) txnp;
  INKContInternal *i = (INKContInternal *) contp;

  // Must have a mutex
  sdk_assert(sdk_sanity_check_null_ptr((void*)i->mutex) == TS_SUCCESS);

  http_sm->plugin_tunnel_type = HTTP_PLUGIN_AS_INTERCEPT;
  http_sm->plugin_tunnel = PluginVCCore::alloc();
  http_sm->plugin_tunnel->set_accept_cont(i);
}

/* Net VConnections */
void
TSVConnInactivityTimeoutSet(TSVConn connp, TSHRTime timeout)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  NetVConnection *vc = (NetVConnection *) connp;
  vc->set_inactivity_timeout(timeout);
}

void
TSVConnInactivityTimeoutCancel(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  NetVConnection *vc = (NetVConnection *) connp;
  vc->cancel_inactivity_timeout();
}

void
TSVConnActiveTimeoutSet(TSVConn connp, TSHRTime timeout)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  NetVConnection *vc = (NetVConnection *) connp;
  vc->set_active_timeout(timeout);
}

void
TSVConnActiveTimeoutCancel(TSVConn connp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);

  NetVConnection *vc = (NetVConnection *) connp;
  vc->cancel_active_timeout();
}

sockaddr const*
TSNetVConnLocalAddrGet(TSVConn connp) {
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);
  NetVConnection* vc = reinterpret_cast<NetVConnection*>(connp);
  return vc->get_local_addr();
}


sockaddr const*
TSNetVConnRemoteAddrGet(TSVConn connp) {
  sdk_assert(sdk_sanity_check_iocore_structure(connp) == TS_SUCCESS);
  NetVConnection* vc = reinterpret_cast<NetVConnection*>(connp);
  return vc->get_remote_addr();
}

TSAction
TSNetConnect(TSCont contp, sockaddr const* addr)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(ats_is_ip(addr));

  FORCE_PLUGIN_MUTEX(contp);

  return reinterpret_cast<TSAction>(
    netProcessor.connect_re(reinterpret_cast<INKContInternal*>(contp), addr)
  );
}

TSAction
TSNetAccept(TSCont contp, int port, int domain, int accept_threads)
{
  NetProcessor::AcceptOptions opt;

  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(port > 0);
  sdk_assert(accept_threads >= -1);

  // TODO: Does this imply that only one "accept thread" could be
  // doing an accept at any time?
  FORCE_PLUGIN_MUTEX(contp);

  // If it's not IPv6, force to IPv4.
  opt.ip_family = domain == AF_INET6 ? AF_INET6 : AF_INET;
  opt.accept_threads = accept_threads;
  opt.local_port = port;
  opt.frequent_accept = false;

  INKContInternal *i = (INKContInternal *) contp;
  return (TSAction)netProcessor.accept(i, opt);
}

TSAction
TSNetAcceptFrequent(TSCont contp, int port, int domain, int accept_threads, int frequent)
{
  NetProcessor::AcceptOptions opt;

  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(port > 0);
  sdk_assert(accept_threads >= -1);

  // TODO: Does this imply that only one "accept thread" could be
  // doing an accept at any time?
  FORCE_PLUGIN_MUTEX(contp);

  // If it's not IPv6, force to IPv4.
  opt.ip_family = domain == AF_INET6 ? AF_INET6 : AF_INET;
  opt.accept_threads = accept_threads;
  opt.local_port = port;
  opt.frequent_accept = (frequent != 0);

  INKContInternal *i = (INKContInternal *) contp;
  return (TSAction)netProcessor.accept(i, opt);
}

/* From proxy/http/HttpProxyServerMain.c: */
extern bool ssl_register_protocol(const char *, Continuation *);
extern bool ssl_unregister_protocol(const char *, Continuation *);

TSReturnCode
TSNetAcceptNamedProtocol(TSCont contp, const char * protocol)
{
#if TS_USE_TLS_NPN

  sdk_assert(protocol != NULL);
  sdk_assert(contp != NULL);
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);

  if (!ssl_register_protocol(protocol, (INKContInternal *)contp)) {
    ssl_unregister_protocol(protocol, (INKContInternal *)contp);
    return TS_ERROR;
  }

  return TS_SUCCESS;

#else /* TS_USE_TLS_NPN */

  NOWARN_UNUSED(contp);
  NOWARN_UNUSED(protocol);
  return TS_ERROR;

#endif /* TS_USE_TLS_NPN */
}

/* DNS Lookups */
TSAction
TSHostLookup(TSCont contp, const char *hostname, size_t namelen)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)hostname) == TS_SUCCESS);
  sdk_assert(namelen > 0);

  FORCE_PLUGIN_MUTEX(contp);

  INKContInternal *i = (INKContInternal *) contp;
  return (TSAction)hostDBProcessor.getbyname_re(i, hostname, namelen);
}

sockaddr const*
TSHostLookupResultAddrGet(TSHostLookupResult lookup_result)
{
  sdk_assert(sdk_sanity_check_hostlookup_structure(lookup_result) == TS_SUCCESS);
  HostDBInfo* di = reinterpret_cast<HostDBInfo*>(lookup_result);
  return di->ip();
}

/*
 * checks if the cache is ready
 */

/* Only TSCacheReady exposed in SDK. No need of TSCacheDataTypeReady */
/* because SDK cache API supports only the data type: NONE */
TSReturnCode
TSCacheReady(int *is_ready)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)is_ready) == TS_SUCCESS);
  return TSCacheDataTypeReady(TS_CACHE_DATA_TYPE_NONE, is_ready);
}

/* Private API (used by Mixt) */
TSReturnCode
TSCacheDataTypeReady(TSCacheDataType type, int *is_ready)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)is_ready) == TS_SUCCESS);

  CacheFragType frag_type;

  switch (type) {
  case TS_CACHE_DATA_TYPE_NONE:
    frag_type = CACHE_FRAG_TYPE_NONE;
    break;
  case TS_CACHE_DATA_TYPE_OTHER:      /* other maps to http */
  case TS_CACHE_DATA_TYPE_HTTP:
    frag_type = CACHE_FRAG_TYPE_HTTP;
    break;
  default:
    *is_ready = 0;
    return TS_ERROR;
  }

  *is_ready = cacheProcessor.IsCacheReady(frag_type);
  return TS_SUCCESS;
}

/*
 * To check whether the cache key belongs to me.
 * If yes, belong_to_me will be set to 1,
 * otherwise set to 0.
 * NOTE: user should make sure cache is ready
 *       before calling this function.
 */
TSReturnCode
TSCacheBelongToMe(TSCacheKey key, int *belong_to_me)
{
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)belong_to_me) == TS_SUCCESS);

  CacheInfo *info = (CacheInfo *) key;

  *belong_to_me = cacheProcessor.belong_to_me(&info->cache_key);

  return TS_SUCCESS;
}

/*
 * To check whether the cache clustering is enabled.
 * If yes, is_cache_clustering will be set to 1,
 * otherwise set to 0.
 */
TSReturnCode
TSIsCacheClustering(int *is_cache_clustering)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)is_cache_clustering) == TS_SUCCESS);

  *is_cache_clustering = CacheProcessor::IsCacheClustering();

  return TS_SUCCESS;
}

/*
 * Get the lastest left_time according the owner_ip.
 * left_time would be set to 0 in two situation:
 *   - This calling machine(not owner machine) restarted.
 *   - default cluster configuration is disable.
 */
TSReturnCode
TSCacheOwnerLeftTimeGet(unsigned int owner_ip, TSHRTime *left_time)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)left_time) == TS_SUCCESS);

  *left_time = owner_left_time_get(owner_ip);

  return TS_SUCCESS;
}

/*
 * To check whether the default cluster config is enabled.
 * If yes, default_cluster_config_enabled will be set to 1,
 * otherwise set to 0.
 */
TSReturnCode
TSDefaultClusterConfigEnabled(int *default_cluster_config_enabled)
{
  int enabled = 0;

  sdk_assert(sdk_sanity_check_null_ptr((void*)default_cluster_config_enabled) == TS_SUCCESS);

#ifndef INK_NO_CLUSTER
  enabled = (clusterProcessor.this_cluster->default_configuration != NULL);
#endif

  *default_cluster_config_enabled = enabled;

  return TS_SUCCESS;
}


/* Cache VConnections */
TSAction
TSCacheRead(TSCont contp, TSCacheKey key)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);

  CacheInfo *info = (CacheInfo *) key;
  Continuation *i = (INKContInternal *) contp;

  return (TSAction)cacheProcessor.open_read(i, &info->cache_key, info->cluster_cache_local,
        info->frag_type, info->hostname, info->len);
}

TSAction
TSHTTPCacheRead(TSCont contp, TSMBuffer url, int cluster_local,
    TSMBuffer request, TSCacheLookupHttpConfig params)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)url) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)request) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)params) == TS_SUCCESS);

  return (TSAction) cacheProcessor.open_read((Continuation *) contp, (URL *) url, cluster_local,
      (CacheHTTPHdr *) request, (CacheLookupHttpConfig *) params);
}

TSAction
TSCacheWrite(TSCont contp, TSCacheKey key)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);

  CacheInfo *info = (CacheInfo *) key;
  Continuation *i = (INKContInternal *) contp;

  return (TSAction)cacheProcessor.open_write(i, &info->cache_key, true, info->frag_type, 0, false, info->pin_in_cache,
                                             info->hostname, info->len);
}

TSAction
TSCacheRemove(TSCont contp, TSCacheKey key)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_cachekey(key) == TS_SUCCESS);

  FORCE_PLUGIN_MUTEX(contp);

  CacheInfo *info = (CacheInfo *) key;
  INKContInternal *i = (INKContInternal *) contp;

  return (TSAction)cacheProcessor.remove(i, &info->cache_key, info->cluster_cache_local, 
                        info->frag_type, true, false, info->hostname, info->len);
}

TSAction
TSCacheScan(TSCont contp, TSCacheKey key, int KB_per_second)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);
  // NOTE: key can be NULl here, so don't check for it.

  FORCE_PLUGIN_MUTEX(contp);

  INKContInternal *i = (INKContInternal *) contp;

  if (key) {
    CacheInfo *info = (CacheInfo *) key;
    return (TSAction)cacheProcessor.scan(i, info->hostname, info->len, KB_per_second);
  }
  return reinterpret_cast<TSAction>(cacheProcessor.scan(i, 0, 0, KB_per_second));
}


/************************   REC Stats API    **************************/
int
TSStatCreate(const char *the_name, TSRecordDataType the_type, TSStatPersistence persist, TSStatSync sync)
{
  int volatile id = ink_atomic_increment(&top_stat, 1);
  RecRawStatSyncCb syncer = RecRawStatSyncCount;

  // TODO: This only supports "int" data types at this point, since the "Raw" stats
  // interfaces only supports integers. Going forward, we could extend either the "Raw"
  // stats APIs, or make non-int use the direct (synchronous) stats APIs (slower).
  if ((sdk_sanity_check_null_ptr((void*)the_name) != TS_SUCCESS) ||
      (sdk_sanity_check_null_ptr((void*)api_rsb) != TS_SUCCESS))
    return TS_ERROR;

  switch (sync) {
  case TS_STAT_SYNC_SUM:
    syncer = RecRawStatSyncSum;
    break;
  case TS_STAT_SYNC_AVG:
    syncer = RecRawStatSyncAvg;
    break;
  case TS_STAT_SYNC_TIMEAVG:
    syncer = RecRawStatSyncHrTimeAvg;
    break;
  default:
    syncer = RecRawStatSyncCount;
    break;
  }
  RecRegisterRawStat(api_rsb, RECT_PLUGIN, the_name, (RecDataT)the_type, (RecPersistT)persist, id, syncer);

  return id;
}

void
TSStatIntIncrement(int the_stat, TSMgmtInt amount)
{
  RecIncrRawStat(api_rsb, NULL, the_stat, amount);
}

void
TSStatIntDecrement(int the_stat, TSMgmtInt amount)
{
  RecDecrRawStat(api_rsb, NULL, the_stat, amount);
}

TSMgmtInt
TSStatIntGet(int the_stat)
{
  TSMgmtInt value;

  RecGetGlobalRawStatSum(api_rsb, the_stat, &value);
  return value;
}

void
TSStatIntSet(int the_stat, TSMgmtInt value)
{
  RecSetGlobalRawStatSum(api_rsb, the_stat, value);
}

TSReturnCode
TSStatFindName(const char* name, int *idp)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)name) == TS_SUCCESS);

  if (RecGetRecordOrderAndId(name, NULL, idp) == REC_ERR_OKAY)
    return TS_SUCCESS;

  return TS_ERROR;
}


/**************************    Stats API    ****************************/
// THESE APIS ARE DEPRECATED, USE THE REC APIs INSTEAD
// #define ink_sanity_check_stat_structure(_x) TS_SUCCESS

inline TSReturnCode
ink_sanity_check_stat_structure(void *obj)
{
  if (obj == NULL)
    return TS_ERROR;

  return TS_SUCCESS;
}

INKStat
INKStatCreate(const char *the_name, INKStatTypes the_type)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)the_name) == TS_SUCCESS);

  StatDescriptor *n = NULL;

  switch (the_type) {
  case INKSTAT_TYPE_INT64:
    n = StatDescriptor::CreateDescriptor(the_name, (int64_t) 0);
    break;

  case INKSTAT_TYPE_FLOAT:
    n = StatDescriptor::CreateDescriptor(the_name, (float) 0);
    break;

  default:
    sdk_assert(!"Bad Input");
    break;
  };

  return (INKStat)n;
}

void
INKStatIntAddTo(INKStat the_stat, int64_t amount)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *)the_stat;
  statp->add(amount);
}

void
INKStatFloatAddTo(INKStat the_stat, float amount)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *)the_stat;
  statp->add(amount);
}

void
INKStatDecrement(INKStat the_stat)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *)the_stat;
  statp->decrement();
}

void
INKStatIncrement(INKStat the_stat)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *)the_stat;
  statp->increment();
}

int64_t
INKStatIntGet(INKStat the_stat)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *) the_stat;
  return statp->int_value();
}

float
INKStatFloatGet(INKStat the_stat)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *) the_stat;
  return statp->flt_value();
}

void
INKStatIntSet(INKStat the_stat, int64_t value)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *) the_stat;
  statp->set(value);
}

void
INKStatFloatSet(INKStat the_stat, float value)
{
  sdk_assert(ink_sanity_check_stat_structure(the_stat) == TS_SUCCESS);

  StatDescriptor *statp = (StatDescriptor *) the_stat;
  statp->set(value);
}

INKCoupledStat
INKStatCoupledGlobalCategoryCreate(const char *the_name)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)the_name) == TS_SUCCESS);

  CoupledStats *category = NEW(new CoupledStats(the_name));
  return (INKCoupledStat)category;
}

INKCoupledStat
INKStatCoupledLocalCopyCreate(const char *the_name, INKCoupledStat global_copy)
{
  sdk_assert(ink_sanity_check_stat_structure(global_copy) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)the_name) == TS_SUCCESS);

  CoupledStatsSnapshot *snap = NEW(new CoupledStatsSnapshot((CoupledStats *) global_copy));
return (INKCoupledStat)snap;
}

void
INKStatCoupledLocalCopyDestroy(INKCoupledStat stat)
{
  sdk_assert(ink_sanity_check_stat_structure(stat) == TS_SUCCESS);

  CoupledStatsSnapshot *snap = (CoupledStatsSnapshot *)stat;

  if (snap) {
    delete snap;
  }
}

INKStat
INKStatCoupledGlobalAdd(INKCoupledStat global_copy, const char *the_name, INKStatTypes the_type)
{
  sdk_assert(ink_sanity_check_stat_structure(global_copy) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)the_name) == TS_SUCCESS);

  CoupledStats *category = (CoupledStats *) global_copy;
  StatDescriptor *n = NULL;

  switch (the_type) {
  case INKSTAT_TYPE_INT64:
    n = category->CreateStat(the_name, (int64_t) 0);
    break;

  case INKSTAT_TYPE_FLOAT:
    n = category->CreateStat(the_name, (float) 0);
    break;

  default:
    sdk_assert(!"Bad stat type");
    break;
  };

  return (INKStat)n;
}

INKStat
INKStatCoupledLocalAdd(INKCoupledStat local_copy, const char *the_name, INKStatTypes the_type)
{
  sdk_assert(ink_sanity_check_stat_structure(local_copy) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)the_name) == TS_SUCCESS);
  sdk_assert((the_type == INKSTAT_TYPE_INT64) || (the_type == INKSTAT_TYPE_FLOAT));

  StatDescriptor *n = ((CoupledStatsSnapshot *) local_copy)->fetchNext();
  return (INKStat)n;
}

void
INKStatsCoupledUpdate(INKCoupledStat local_copy)
{
  sdk_assert(ink_sanity_check_stat_structure(local_copy) == TS_SUCCESS);
  ((CoupledStatsSnapshot *) local_copy)->CommitUpdates();
}


/**************************   Tracing API   ****************************/
// returns 1 or 0 to indicate whether TS is being run with a debug tag.
int
TSIsDebugTagSet(const char *t)
{
  return (diags->on(t, DiagsTagType_Debug)) ? 1 : 0;
}

void
TSDebugSpecific(int debug_flag, const char *tag, const char *format_str, ...)
{
  if (diags->on(tag, DiagsTagType_Debug) || (debug_flag  && diags->on())) {
    va_list ap;

    va_start(ap, format_str);
    diags->print_va(tag, DL_Diag, NULL, format_str, ap);
    va_end(ap);
  }
}

// Plugins would use TSDebug just as the TS internal uses Debug
// e.g. TSDebug("plugin-cool", "Snoopy is a cool guy even after %d requests.\n", num_reqs);
void
TSDebug(const char *tag, const char *format_str, ...)
{
  if (diags->on(tag, DiagsTagType_Debug)) {
    va_list ap;

    va_start(ap, format_str);
    diags->print_va(tag, DL_Diag, NULL, format_str, ap);
    va_end(ap);
  }
}

/**************************   Logging API   ****************************/

TSReturnCode
TSTextLogObjectCreate(const char *filename, int mode, TSTextLogObject *new_object)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)filename) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)new_object) == TS_SUCCESS);

  if (mode<0 || mode>= TS_LOG_MODE_INVALID_FLAG) {
    *new_object = NULL;
    return TS_ERROR;
  }

  TextLogObject *tlog = NEW(new TextLogObject(filename, Log::config->logfile_dir,
                                              (bool) mode & TS_LOG_MODE_ADD_TIMESTAMP,
                                              NULL,
                                              Log::config->rolling_enabled,
                                              Log::config->rolling_interval_sec,
                                              Log::config->rolling_offset_hr,
                                              Log::config->rolling_size_mb));
  if (tlog) {
    int err = (mode & TS_LOG_MODE_DO_NOT_RENAME ?
               Log::config->log_object_manager.manage_api_object(tlog, 0) :
               Log::config->log_object_manager.manage_api_object(tlog));
    if (err != LogObjectManager::NO_FILENAME_CONFLICTS) {
      delete tlog;
      *new_object = NULL;
      return TS_ERROR;
    }
  } else {
    *new_object = NULL;
    return TS_ERROR;
  }
  *new_object = (TSTextLogObject) tlog;

  return TS_SUCCESS;
}

TSReturnCode
TSTextLogObjectWrite(TSTextLogObject the_object, char *format, ...)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)format) == TS_SUCCESS);

  TSReturnCode retVal = TS_SUCCESS;

  va_list ap;
  va_start(ap, format);
  switch (((TextLogObject *) the_object)->va_write(format, ap)) {
  case (Log::LOG_OK):
  case (Log::SKIP):
  case (Log::AGGR):
    break;
  case (Log::FULL):
    retVal = TS_ERROR;
    break;
  case (Log::FAIL):
    retVal = TS_ERROR;
    break;
  default:
    ink_debug_assert(!"invalid return code");
  }
  va_end(ap);

  return retVal;
}

void
TSTextLogObjectFlush(TSTextLogObject the_object)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);

  ((TextLogObject *) the_object)->force_new_buffer();
}

TSReturnCode
TSTextLogObjectDestroy(TSTextLogObject the_object)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);

  if (Log::config->log_object_manager.unmanage_api_object((TextLogObject *) the_object))
    return TS_SUCCESS;

  return TS_ERROR;
}

void
TSTextLogObjectHeaderSet(TSTextLogObject the_object, const char *header)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);

  ((TextLogObject *) the_object)->set_log_file_header(header);
}

void
TSTextLogObjectRollingEnabledSet(TSTextLogObject the_object, int rolling_enabled)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);

  ((TextLogObject *) the_object)->set_rolling_enabled(rolling_enabled);
}

void
TSTextLogObjectRollingIntervalSecSet(TSTextLogObject the_object, int rolling_interval_sec)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);

  ((TextLogObject *) the_object)->set_rolling_interval_sec(rolling_interval_sec);
}

void
TSTextLogObjectRollingOffsetHrSet(TSTextLogObject the_object, int rolling_offset_hr)
{
  sdk_assert(sdk_sanity_check_iocore_structure(the_object) == TS_SUCCESS);

  ((TextLogObject *) the_object)->set_rolling_offset_hr(rolling_offset_hr);
}

TSReturnCode
TSHttpSsnClientFdGet(TSHttpSsn ssnp, int *fdp)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)fdp) == TS_SUCCESS);

  HttpClientSession *cs = (HttpClientSession *) ssnp;

  if (cs == NULL)
    return TS_ERROR;

  NetVConnection *vc = cs->get_netvc();
  if (vc == NULL)
    return TS_ERROR;

  *fdp = vc->get_socket();
  return TS_SUCCESS;
}
TSReturnCode
TSHttpTxnClientFdGet(TSHttpTxn txnp, int *fdp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)fdp) == TS_SUCCESS);

  TSHttpSsn ssnp = TSHttpTxnSsnGet(txnp);
  return TSHttpSsnClientFdGet(ssnp, fdp);
}

/* Matcher Utils */
char *
TSMatcherReadIntoBuffer(char *file_name, int *file_len)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)file_name) == TS_SUCCESS);
  return readIntoBuffer((char *) file_name, "TSMatcher", file_len);
}

char *
TSMatcherTokLine(char *buffer, char **last)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)buffer) == TS_SUCCESS);
  return tokLine(buffer, last);
}

char *
TSMatcherExtractIPRange(char *match_str, uint32_t *addr1, uint32_t *addr2)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)match_str) == TS_SUCCESS);
  return (char*)ExtractIpRange(match_str, addr1, addr2);
}
// Conflict in header due to overload (must be C compatible).
char *
TSMatcherExtractIPRange(char *match_str, sockaddr* addr1, sockaddr* addr2)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)match_str) == TS_SUCCESS);
  return (char*)ExtractIpRange(match_str, addr1, addr2);
}

TSMatcherLine
TSMatcherLineCreate(void)
{
  return reinterpret_cast<TSMatcherLine>(ats_malloc(sizeof(matcher_line)));
}

void
TSMatcherLineDestroy(TSMatcherLine ml)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)ml) == TS_SUCCESS);
  ats_free(ml);
}

const char *
TSMatcherParseSrcIPConfigLine(char *line, TSMatcherLine ml)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)line) == TS_SUCCESS);
  return parseConfigLine(line, (matcher_line *) ml, &ip_allow_tags);
}

char *
TSMatcherLineName(TSMatcherLine ml, int element)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)ml) == TS_SUCCESS);
  return (((matcher_line *) ml)->line)[0][element];
}

char *
TSMatcherLineValue(TSMatcherLine ml, int element)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)ml) == TS_SUCCESS);
  return (((matcher_line *) ml)->line)[1][element];
}

/* Configuration Setting */
TSReturnCode
TSMgmtConfigIntSet(const char *var_name, TSMgmtInt value)
{
  TSMgmtInt result;
  char *buffer;

  // is this a valid integer?
  if (TSMgmtIntGet(var_name, &result) != TS_SUCCESS)
    return TS_ERROR;

  // construct a buffer
  int buffer_size = strlen(var_name) + 1 + 32 + 1 + 64 + 1;

  buffer = (char *) alloca(buffer_size);
  snprintf(buffer, buffer_size, "%s %d %" PRId64 "", var_name, MGMT_INT, value);

  // tell manager to set the configuration; note that this is not
  // transactional (e.g. we return control to the plugin before the
  // value is commited to disk by the manager)
  RecSignalManager(MGMT_SIGNAL_PLUGIN_SET_CONFIG, buffer);

  return TS_SUCCESS;
}

TSCacheLookupHttpConfig
TSCacheLookupHttpConfigCreate(void)
{
  CacheLookupHttpConfig *config =
      new (CacheLookupHttpConfigAllocator.alloc()) CacheLookupHttpConfig();
  HttpConfigParams *http_config_param = HttpConfig::acquire();

  if (http_config_param) {
    config->cache_global_user_agent_header =
        http_config_param->global_user_agent_header ? true : false;
    config->ignore_accept_mismatch =
        http_config_param->ignore_accept_mismatch ? true : false;
    config->ignore_accept_language_mismatch =
        http_config_param->ignore_accept_language_mismatch ? true : false;
    config->ignore_accept_encoding_mismatch =
        http_config_param->ignore_accept_encoding_mismatch ? true : false;
    config->ignore_accept_charset_mismatch =
        http_config_param->ignore_accept_charset_mismatch ? true : false;
    config->cache_enable_default_vary_headers =
        http_config_param->cache_enable_default_vary_headers ? true : false;

    config->cache_vary_default_text =
        http_config_param->cache_vary_default_text;
    config->cache_vary_default_images =
        http_config_param->cache_vary_default_images;
    config->cache_vary_default_other =
        http_config_param->cache_vary_default_other;
  }

  return (TSCacheLookupHttpConfig) config;
}

TSReturnCode
TSCacheLookupHttpConfigDestory(TSCacheLookupHttpConfig conf)
{
  CacheLookupHttpConfig *config = (CacheLookupHttpConfig *) conf;
  delete config;
  return TS_SUCCESS;
}

/* Alarm */
/* return type is "int" currently, it should be TSReturnCode */
void
TSSignalWarning(TSAlarmType code, char *msg)
{
  sdk_assert(code >= TS_SIGNAL_WDA_BILLING_CONNECTION_DIED && code <= TS_SIGNAL_WDA_RADIUS_CORRUPTED_PACKETS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)msg) == TS_SUCCESS);

  REC_SignalWarning(code, msg);
}

void
TSICPFreshnessFuncSet(TSPluginFreshnessCalcFunc funcp)
{
  pluginFreshnessCalcFunc = (PluginFreshnessCalcFunc) funcp;
}

TSReturnCode
TSICPCachedReqGet(TSCont contp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)obj) == TS_SUCCESS);

  ICPPeerReadCont *sm = (ICPPeerReadCont *)contp;
  HTTPInfo *cached_obj;

  cached_obj = sm->_object_read;
  if (cached_obj == NULL || !cached_obj->valid())
    return TS_ERROR;

  HTTPHdr *cached_hdr = cached_obj->request_get();
  if (!cached_hdr->valid())
    return TS_ERROR;;

  // We can't use the HdrHeapSDKHandle structure in the RamCache since multiple
  //  threads can access.  We need to create our own for the transaction and return that.
  HdrHeapSDKHandle **handle = &(sm->_cache_req_hdr_heap_handle);

  if (*handle == NULL) {
    *handle = (HdrHeapSDKHandle *)ats_malloc(sizeof(HdrHeapSDKHandle));
    (*handle)->m_heap = cached_hdr->m_heap;
  }

  *(reinterpret_cast<HdrHeapSDKHandle**>(bufp)) = *handle;
  *obj = reinterpret_cast<TSMLoc>(cached_hdr->m_http);

  return sdk_sanity_check_mbuffer(*bufp);
}

TSReturnCode
TSICPCachedRespGet(TSCont contp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)obj) == TS_SUCCESS);

  ICPPeerReadCont *sm = (ICPPeerReadCont *) contp;
  HTTPInfo *cached_obj;

  cached_obj = sm->_object_read;
  if (cached_obj == NULL || !cached_obj->valid())
    return TS_ERROR;

  HTTPHdr *cached_hdr = cached_obj->response_get();
  if (!cached_hdr->valid())
    return TS_ERROR;

  // We can't use the HdrHeapSDKHandle structure in the RamCache since multiple
  //  threads can access.  We need to create our own for the transaction and return that.
  HdrHeapSDKHandle **handle = &(sm->_cache_resp_hdr_heap_handle);

  if (*handle == NULL) {
    *handle = (HdrHeapSDKHandle *)ats_malloc(sizeof(HdrHeapSDKHandle));
    (*handle)->m_heap = cached_hdr->m_heap;
  }

  *(reinterpret_cast<HdrHeapSDKHandle**>(bufp)) = *handle;
  *obj = reinterpret_cast<TSMLoc>(cached_hdr->m_http);

  return sdk_sanity_check_mbuffer(*bufp);
}

TSReturnCode
TSCacheUrlSet(TSHttpTxn txnp, const char *url, int length)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM *) txnp;
  if (sm->t_state.cache_info.lookup_url != NULL) {
    if (sm->t_state.cache_info.lookup_url == &(sm->t_state.cache_info.lookup_url_storage)) {
      sm->t_state.cache_info.lookup_url_storage.destroy();
    }
  }

  Debug("cache_url", "[TSCacheUrlSet] changing the cache url to: %s", url);

  if (length == -1) {
    length = strlen(url);
  }

  sm->t_state.cache_info.lookup_url_storage.create(NULL);
  sm->t_state.cache_info.lookup_url = &(sm->t_state.cache_info.lookup_url_storage);
  if (sm->t_state.cache_info.lookup_url->parse(url, length) == PARSE_ERROR) {
    return TS_ERROR;
  }
  else {
    return TS_SUCCESS;
  }
}

void
TSCacheHttpInfoKeySet(TSCacheHttpInfo infop, TSCacheKey keyp)
{
  // TODO: Check input ?
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;
  INK_MD5 *key = (INK_MD5 *)keyp;

  info->object_key_set(*key);
}

void
TSCacheHttpInfoSizeSet(TSCacheHttpInfo infop, int64_t size)
{
  // TODO: Check input ?
  CacheHTTPInfo *info = (CacheHTTPInfo *) infop;

  info->object_size_set(size);
}

// this function should be called at TS_EVENT_HTTP_READ_RESPONSE_HDR
void
TSRedirectUrlSet(TSHttpTxn txnp, const char* url, const int url_len)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)url) == TS_SUCCESS);

  HttpSM *sm = (HttpSM*) txnp;

  if (sm->redirect_url != NULL) {
    ats_free(sm->redirect_url);
    sm->redirect_url = NULL;
    sm->redirect_url_len = 0;
  }

  sm->redirect_url = (char*)ats_malloc(url_len + 1);
  ink_strlcpy(sm->redirect_url, (char*)url, url_len + 1);
  sm->redirect_url_len = url_len;
  // have to turn redirection on for this transaction if user wants to redirect to another URL
  if (sm->enable_redirection == false) {
    sm->enable_redirection = true;
    // max-out "redirection_tries" to avoid the regular redirection being turned on in
    // this transaction improperly. This variable doesn't affect the custom-redirection
    sm->redirection_tries = HttpConfig::m_master.number_of_redirections;
  }
}

const char*
TSRedirectUrlGet(TSHttpTxn txnp, int *url_len_ptr)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM*)txnp;

  *url_len_ptr = sm->redirect_url_len;
  return (const char*)sm->redirect_url;
}

void
TSHttpTxnSliceInfoSet(TSHttpTxn txnp, uint64_t global_idx, short slice_idx)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  ((HttpSM *)txnp)->slice_idx = slice_idx;
  ((HttpSM *)txnp)->global_idx = global_idx;
}

void
TSHttpTxnNetProtoTypeSet(TSHttpTxn txnp, TSNetProtoType proto_type)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  ((HttpSM *)txnp)->proto_type = (NetProtoType)proto_type;
}

char*
TSFetchRespGet(TSHttpTxn txnp, int *length)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);
  FetchSM *fetch_sm = (FetchSM*)txnp;
  return fetch_sm->resp_get(length);
}

TSReturnCode
TSFetchPageRespGet(TSHttpTxn txnp, TSMBuffer *bufp, TSMLoc *obj)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)bufp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)obj) == TS_SUCCESS);

  HTTPHdr *hptr = (HTTPHdr*) txnp;

  if (hptr->valid()) {
    *(reinterpret_cast<HTTPHdr**>(bufp)) = hptr;
    *obj = reinterpret_cast<TSMLoc>(hptr->m_http);
    return sdk_sanity_check_mbuffer(*bufp);
  }

  return TS_ERROR;
}

// Fetchpages SM
extern ClassAllocator<FetchSM> FetchSMAllocator;

void
TSFetchPages(TSFetchUrlParams_t *params)
{
  TSFetchUrlParams_t *myparams = params;

  while (myparams != NULL) {
    FetchSM *fetch_sm =  FetchSMAllocator.alloc();
    sockaddr* addr = ats_ip_sa_cast(&myparams->ip);
    in_addr_t ip = ats_ip4_addr_cast(addr);
    uint16_t port = ats_ip_port_host_order(addr);

    fetch_sm->init((Continuation*)myparams->contp, myparams->options,myparams->events, myparams->request, myparams->request_len, ip, port);
    fetch_sm->httpConnect();
    myparams= myparams->next;
  }
}

void
TSFetchUrl(const char* headers, int request_len, sockaddr const* ip , TSCont contp, TSFetchWakeUpOptions callback_options,TSFetchEvent events)
{
  if (callback_options != NO_CALLBACK) {
    sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  }
  sdk_assert(ats_is_ip4(ip));

  FetchSM *fetch_sm =  FetchSMAllocator.alloc();
  in_addr_t addr = ats_ip4_addr_cast(ip);
  unsigned short port = ats_ip_port_cast(ip);

  fetch_sm->init((Continuation*)contp, callback_options, events, headers, request_len, addr, port);
  fetch_sm->httpConnect();
}

TSFetchSM
TSFetchCreate(TSCont contp, TSFetchMethod method,
              const char *url, const char *version,
              struct sockaddr const* client_addr, int flags)
{
  sdk_assert(sdk_sanity_check_continuation(contp) == TS_SUCCESS);
  sdk_assert(ats_is_ip4(client_addr));

  FetchSM *fetch_sm = FetchSMAllocator.alloc();

  fetch_sm->ext_init((Continuation*)contp, method, url, version,
                     client_addr, flags);

  return (TSFetchSM)fetch_sm;
}

void
TSFetchHeaderAdd(TSFetchSM fetch_sm,
                 const char *name, int name_len,
                 const char *value, int value_len)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  ((FetchSM*)fetch_sm)->ext_add_header(name, name_len, value, value_len);
}

void
TSFetchWriteData(TSFetchSM fetch_sm, const void *data, size_t len)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  ((FetchSM*)fetch_sm)->ext_write_data(data, len);
}

ssize_t
TSFetchReadData(TSFetchSM fetch_sm, void *buf, size_t len)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  return ((FetchSM*)fetch_sm)->ext_read_data((char *)buf, len);
}

void
TSFetchLaunch(TSFetchSM fetch_sm)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  ((FetchSM*)fetch_sm)->ext_lanuch();
}

void
TSFetchDestroy(TSFetchSM fetch_sm)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  ((FetchSM*)fetch_sm)->ext_destroy();
}

void
TSFetchUserDataSet(TSFetchSM fetch_sm, void *data)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  ((FetchSM*)fetch_sm)->ext_set_user_data(data);
}

void*
TSFetchUserDataGet(TSFetchSM fetch_sm)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  return ((FetchSM*)fetch_sm)->ext_get_user_data();
}

void
TSFetchProtoTypeSet(TSFetchSM fetch_sm, TSNetProtoType proto_type)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  ((FetchSM*)fetch_sm)->ext_set_proto_type(proto_type);
}

TSNetProtoType
TSFetchProtoTypeGet(TSFetchSM fetch_sm)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  return ((FetchSM*)fetch_sm)->ext_get_proto_type();
}

TSMBuffer
TSFetchRespHdrMBufGet(TSFetchSM fetch_sm)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  return ((FetchSM*)fetch_sm)->resp_hdr_bufp();
}

TSMLoc
TSFetchRespHdrMLocGet(TSFetchSM fetch_sm)
{
  sdk_assert(sdk_sanity_check_fetch_sm(fetch_sm) == TS_SUCCESS);

  return ((FetchSM*)fetch_sm)->resp_hdr_mloc();
}

TSReturnCode
TSHttpIsInternalRequest(TSHttpTxn txnp)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  TSHttpSsn ssnp = TSHttpTxnSsnGet(txnp);
  HttpClientSession *cs = (HttpClientSession *) ssnp;
  if (!cs)
    return TS_ERROR;

  NetVConnection *vc = cs->get_netvc();
  if (!vc)
    return TS_ERROR;

  return vc->get_is_internal_request() ? TS_SUCCESS : TS_ERROR;
}


TSReturnCode
TSAIORead(int fd, off_t offset, char* buf, size_t buffSize, TSCont contp)
{
  sdk_assert(sdk_sanity_check_iocore_structure(contp) == TS_SUCCESS);

  Continuation* pCont = (Continuation*)contp;
  AIOCallback* pAIO = new_AIOCallback();

  if (pAIO == NULL)
    return TS_ERROR;

  pAIO->aiocb.aio_fildes = fd;
  pAIO->aiocb.aio_offset = offset;
  pAIO->aiocb.aio_nbytes = buffSize;


  pAIO->aiocb.aio_buf = buf;
  pAIO->action = pCont;
  pAIO->thread = ((ProxyMutex*) pCont->mutex)->thread_holding;

  if (ink_aio_read(pAIO, 1) == 1)
    return TS_SUCCESS;

  return TS_ERROR;
}

char*
TSAIOBufGet(TSAIOCallback data)
{
  AIOCallback* pAIO = (AIOCallback*)data;
  return (char*)pAIO->aiocb.aio_buf;
}

int
TSAIONBytesGet(TSAIOCallback data)
{
  AIOCallback* pAIO = (AIOCallback*)data;
  return (int)pAIO->aio_result;
}

TSReturnCode
TSAIOWrite(int fd, off_t offset, char* buf, const size_t bufSize, TSCont contp)
{
  sdk_assert(sdk_sanity_check_iocore_structure (contp) == TS_SUCCESS);
  
  Continuation* pCont = (Continuation*) contp;
  AIOCallback* pAIO = new_AIOCallback();

  // TODO: Might be able to remove this when allocations can never fail.
  sdk_assert(sdk_sanity_check_null_ptr((void*)pAIO) == TS_SUCCESS);

  pAIO->aiocb.aio_fildes = fd;
  pAIO->aiocb.aio_offset = offset;
  pAIO->aiocb.aio_buf = buf;
  pAIO->aiocb.aio_nbytes = bufSize;
  pAIO->action = pCont;
  pAIO->thread = ((ProxyMutex*) pCont->mutex)->thread_holding;

  if (ink_aio_write(pAIO, 1) == 1)
    return TS_SUCCESS;

  return TS_ERROR;
}

TSReturnCode
TSAIOThreadNumSet(int thread_num)
{
  if (ink_aio_thread_num_set(thread_num))
    return TS_SUCCESS;

  return TS_ERROR;
}

void
TSRecordDump(TSRecordType rec_type, TSRecordDumpCb callback, void *edata)
{
  RecDumpRecords((RecT)rec_type, (RecDumpEntryCb)callback, edata);
}

/* ability to skip the remap phase of the State Machine 
   this only really makes sense in TS_HTTP_READ_REQUEST_HDR_HOOK
*/
void
TSSkipRemappingSet(TSHttpTxn txnp, int flag)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *sm = (HttpSM*) txnp;
  sm->t_state.api_skip_all_remapping = (flag != 0);
}

// Little helper function to find the struct member
static void *
_conf_to_memberp(TSOverridableConfigKey conf, 
    OverridableHttpConfigParams *overridableHttpConfig, 
    OverridableDataType *typep)
{
  // The default is "Byte", make sure to override that for those configs which are "Int".
  OverridableDataType typ = OVERRIDABLE_TYPE_BYTE;
  void* ret = NULL;

  switch (conf) {
  case TS_CONFIG_URL_REMAP_PRISTINE_HOST_HDR:
    ret = &overridableHttpConfig->maintain_pristine_host_hdr;
    break;
  case TS_CONFIG_HTTP_CHUNKING_ENABLED:
    ret = &overridableHttpConfig->chunking_enabled;
    break;
  case TS_CONFIG_HTTP_ALLOW_ANYWAY:
    ret = &overridableHttpConfig->allow_anyway;
    break;
  case TS_CONFIG_HTTP_NEGATIVE_CACHING_ENABLED:
    ret = &overridableHttpConfig->negative_caching_enabled;
    break;
  case TS_CONFIG_HTTP_NEGATIVE_CACHING_LIFETIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->negative_caching_lifetime;
    break;
  case TS_CONFIG_HTTP_CACHE_WHEN_TO_REVALIDATE:
    ret = &overridableHttpConfig->cache_when_to_revalidate;
    break;
  case TS_CONFIG_HTTP_KEEP_ALIVE_ENABLED_IN:
    ret = &overridableHttpConfig->keep_alive_enabled_in;
    break;
  case TS_CONFIG_HTTP_KEEP_ALIVE_ENABLED_OUT:
    ret = &overridableHttpConfig->keep_alive_enabled_out;
    break;
  case TS_CONFIG_HTTP_KEEP_ALIVE_POST_OUT:
    ret = &overridableHttpConfig->keep_alive_post_out;
    break;
  case TS_CONFIG_HTTP_SHARE_SERVER_SESSIONS:
    ret = &overridableHttpConfig->share_server_sessions;
    break;
  case TS_CONFIG_NET_SOCK_RECV_BUFFER_SIZE_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_recv_buffer_size_out;
    break;
  case TS_CONFIG_NET_SOCK_SEND_BUFFER_SIZE_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_send_buffer_size_out;
    break;
  case TS_CONFIG_NET_SOCK_OPTION_FLAG_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_option_flag_out;
    break;
  case TS_CONFIG_NET_SOCK_PACKET_MARK_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_packet_mark_out;
    break;
  case TS_CONFIG_NET_SOCK_PACKET_TOS_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_packet_tos_out;
    break;
  case TS_CONFIG_NET_SOCK_FLOW_CTL_IN:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_flow_control_in;
    break;
  case TS_CONFIG_NET_SOCK_FLOW_CTL_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->sock_flow_control_out;
    break;
  case TS_CONFIG_HTTP_FORWARD_PROXY_AUTH_TO_PARENT:
    ret = &overridableHttpConfig->fwd_proxy_auth_to_parent;
    break;
  case TS_CONFIG_HTTP_ANONYMIZE_REMOVE_FROM:
    ret = &overridableHttpConfig->anonymize_remove_from;
    break;
  case TS_CONFIG_HTTP_ANONYMIZE_REMOVE_REFERER:
    ret = &overridableHttpConfig->anonymize_remove_referer;
    break;
  case TS_CONFIG_HTTP_ANONYMIZE_REMOVE_USER_AGENT:
    ret = &overridableHttpConfig->anonymize_remove_user_agent;
    break;
  case TS_CONFIG_HTTP_ANONYMIZE_REMOVE_COOKIE:
    ret = &overridableHttpConfig->anonymize_remove_cookie;
    break;
  case TS_CONFIG_HTTP_ANONYMIZE_REMOVE_CLIENT_IP:
    ret = &overridableHttpConfig->anonymize_remove_client_ip;
    break;
  case TS_CONFIG_HTTP_ANONYMIZE_INSERT_CLIENT_IP:
    ret = &overridableHttpConfig->anonymize_insert_client_ip;
    break;
  case TS_CONFIG_HTTP_RESPONSE_SERVER_ENABLED:
    ret = &overridableHttpConfig->proxy_response_server_enabled;
    break;
  case TS_CONFIG_HTTP_INSERT_SQUID_X_FORWARDED_FOR:
    ret = &overridableHttpConfig->insert_squid_x_forwarded_for;
    break;
  case TS_CONFIG_HTTP_SERVER_TCP_INIT_CWND:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->server_tcp_init_cwnd;
    break;
  case TS_CONFIG_HTTP_SEND_HTTP11_REQUESTS:
    ret = &overridableHttpConfig->send_http11_requests;
    break;
  case TS_CONFIG_HTTP_CACHE_HTTP:
    ret = &overridableHttpConfig->cache_http;
    break;
  case TS_CONFIG_HTTP_CACHE_FORCE_IN_RAM:
    ret = &overridableHttpConfig->cache_force_in_ram;
    break;
  case TS_CONFIG_HTTP_CACHE_CLUSTER_CACHE_LOCAL:
    ret = &overridableHttpConfig->cache_cluster_cache_local;
    break;
  case TS_CONFIG_HTTP_CACHE_IGNORE_CLIENT_NO_CACHE:
    ret = &overridableHttpConfig->cache_ignore_client_no_cache;
    break;
  case TS_CONFIG_HTTP_CACHE_IGNORE_CLIENT_CC_MAX_AGE:
    ret = &overridableHttpConfig->cache_ignore_client_cc_max_age;
    break;
  case TS_CONFIG_HTTP_CACHE_IMS_ON_CLIENT_NO_CACHE:
    ret = &overridableHttpConfig->cache_ims_on_client_no_cache;
    break;
  case TS_CONFIG_HTTP_CACHE_IGNORE_SERVER_NO_CACHE:
    ret = &overridableHttpConfig->cache_ignore_server_no_cache;
    break;
  case TS_CONFIG_HTTP_CACHE_CACHE_RESPONSES_TO_COOKIES:
    ret = &overridableHttpConfig->cache_responses_to_cookies;
    break;
  case TS_CONFIG_HTTP_CACHE_IGNORE_AUTHENTICATION:
    ret = &overridableHttpConfig->cache_ignore_auth;
    break;
  case TS_CONFIG_HTTP_CACHE_CACHE_URLS_THAT_LOOK_DYNAMIC:
    ret = &overridableHttpConfig->cache_urls_that_look_dynamic;
    break;
  case TS_CONFIG_HTTP_CACHE_REQUIRED_HEADERS:
    ret = &overridableHttpConfig->cache_required_headers;
    break;
  case TS_CONFIG_HTTP_INSERT_REQUEST_VIA_STR:
    ret = &overridableHttpConfig->insert_request_via_string;
    break;
  case TS_CONFIG_HTTP_INSERT_RESPONSE_VIA_STR:
    ret = &overridableHttpConfig->insert_response_via_string;
    break;
  case TS_CONFIG_HTTP_RANGE_ELIMINATION:
    ret = &overridableHttpConfig->range_elimination_enabled;
    break;
  case TS_CONFIG_HTTP_CACHE_HEURISTIC_MIN_LIFETIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_heuristic_min_lifetime;
    break;
  case TS_CONFIG_HTTP_CACHE_HEURISTIC_MAX_LIFETIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_heuristic_max_lifetime;
    break;
  case TS_CONFIG_HTTP_CACHE_GUARANTEED_MIN_LIFETIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_guaranteed_min_lifetime;
    break;
  case TS_CONFIG_HTTP_CACHE_GUARANTEED_MAX_LIFETIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_guaranteed_max_lifetime;
    break;
  case TS_CONFIG_HTTP_CACHE_MAX_RWW_DELAY:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_max_rww_delay;
    break;
  case TS_CONFIG_HTTP_CACHE_MAX_STALE_AGE:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_max_stale_age;
    break;
  case TS_CONFIG_HTTP_KEEP_ALIVE_NO_ACTIVITY_TIMEOUT_IN:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->keep_alive_no_activity_timeout_in;
    break;
  case TS_CONFIG_HTTP_KEEP_ALIVE_NO_ACTIVITY_TIMEOUT_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->keep_alive_no_activity_timeout_out;
    break;
  case TS_CONFIG_HTTP_TRANSACTION_NO_ACTIVITY_TIMEOUT_IN:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->transaction_no_activity_timeout_in;
    break;
  case TS_CONFIG_HTTP_TRANSACTION_NO_ACTIVITY_TIMEOUT_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->transaction_no_activity_timeout_out;
    break;
  case TS_CONFIG_HTTP_TRANSACTION_ACTIVE_TIMEOUT_OUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->transaction_active_timeout_out;
    break;
  case TS_CONFIG_HTTP_ORIGIN_MAX_CONNECTIONS:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->origin_max_connections;
    break;
  case TS_CONFIG_HTTP_MAX_ACTIVE_CLIENT_CONNECTIONS:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->max_active_client_connections;
    break;
  case TS_CONFIG_HTTP_MAX_BANDWIDTH:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->max_bandwidth;
    break;
  case TS_CONFIG_HTTP_CONNECT_ATTEMPTS_MAX_RETRIES:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->connect_attempts_max_retries;
    break;
  case TS_CONFIG_HTTP_CONNECT_ATTEMPTS_MAX_RETRIES_DEAD_SERVER:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->connect_attempts_max_retries_dead_server;
    break;
  case TS_CONFIG_HTTP_CONNECT_ATTEMPTS_RR_RETRIES:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->connect_attempts_rr_retries;
    break;
  case TS_CONFIG_HTTP_CONNECT_ATTEMPTS_TIMEOUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->connect_attempts_timeout;
    break;
  case TS_CONFIG_HTTP_POST_CONNECT_ATTEMPTS_TIMEOUT:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->post_connect_attempts_timeout;
    break;
  case TS_CONFIG_HTTP_DOWN_SERVER_CACHE_TIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->down_server_timeout;
    break;
  case TS_CONFIG_HTTP_DOWN_SERVER_ABORT_THRESHOLD:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->client_abort_threshold;
    break;
  case TS_CONFIG_HTTP_CACHE_FUZZ_TIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->freshness_fuzz_time;
    break;
  case TS_CONFIG_HTTP_CACHE_FUZZ_MIN_TIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->freshness_fuzz_min_time;
    break;
  case TS_CONFIG_HTTP_DOC_IN_CACHE_SKIP_DNS:
    ret = &overridableHttpConfig->doc_in_cache_skip_dns;
    break;

  case TS_CONFIG_HTTP_CACHE_HEURISTIC_LM_FACTOR:
    typ = OVERRIDABLE_TYPE_FLOAT;
    ret = &overridableHttpConfig->cache_heuristic_lm_factor;
    break;
  case TS_CONFIG_HTTP_CACHE_FUZZ_PROBABILITY:
    typ = OVERRIDABLE_TYPE_FLOAT;
    ret = &overridableHttpConfig->freshness_fuzz_prob;
    break;

  case TS_CONFIG_HTTP_RESPONSE_SERVER_STR:
    typ = OVERRIDABLE_TYPE_STRING;
    ret = &overridableHttpConfig->proxy_response_server_string;
    break;
  case TS_CONFIG_HTTP_CACHE_OPEN_READ_RETRY_TIME:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->cache_open_read_retry_time;
    break;
  case TS_CONFIG_HTTP_CACHE_MAX_OPEN_READ_RETRIES:
    typ = OVERRIDABLE_TYPE_INT;
    ret = &overridableHttpConfig->max_cache_open_read_retries;
    break;

    // This helps avoiding compiler warnings, yet detect unhandled enum members.
  case TS_CONFIG_NULL:
  case TS_CONFIG_LAST_ENTRY:
    typ = OVERRIDABLE_TYPE_NULL;
    ret = NULL;
    break;
  }

  *typep = typ;

  return ret;
}

/* APIs to manipulate the overridable configuration options.
*/
TSReturnCode
TSHttpTxnConfigIntSet(TSHttpTxn txnp, TSOverridableConfigKey conf, TSMgmtInt value)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *s = reinterpret_cast<HttpSM*>(txnp);
  OverridableDataType type;

  s->t_state.setup_per_txn_configs();

  void *dest = _conf_to_memberp(conf, s->t_state.txn_conf, &type);

  if (!dest)
    return TS_ERROR;

  switch (type) {
  case OVERRIDABLE_TYPE_INT:
    *(static_cast<TSMgmtInt*>(dest)) = value;
    break;
  case OVERRIDABLE_TYPE_BYTE:
    *(static_cast<TSMgmtByte*>(dest)) = static_cast<TSMgmtByte>(value);
    break;
  default:
    return TS_ERROR;
  }

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnConfigIntGet(TSHttpTxn txnp, TSOverridableConfigKey conf, TSMgmtInt *value)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)value) == TS_SUCCESS);

  OverridableDataType type;
  void* src = _conf_to_memberp(conf, ((HttpSM*)txnp)->t_state.txn_conf, &type);

  if (!src)
    return TS_ERROR;

  switch (type) {
  case OVERRIDABLE_TYPE_INT:
    *value = *(static_cast<TSMgmtInt*>(src));
    break;
  case OVERRIDABLE_TYPE_BYTE:
    *value = *(static_cast<TSMgmtByte*>(src));
    break;
  default:
    return TS_ERROR;
  }

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnConfigFloatSet(TSHttpTxn txnp, TSOverridableConfigKey conf, TSMgmtFloat value)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);

  HttpSM *s = reinterpret_cast<HttpSM*>(txnp);
  OverridableDataType type;

  s->t_state.setup_per_txn_configs();

  TSMgmtFloat* dest = static_cast<TSMgmtFloat*>(_conf_to_memberp(conf, s->t_state.txn_conf, &type));

  if (type != OVERRIDABLE_TYPE_FLOAT)
    return TS_ERROR;

  if (dest) {
    *dest = value;
    return TS_SUCCESS;
  }

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnConfigFloatGet(TSHttpTxn txnp, TSOverridableConfigKey conf, TSMgmtFloat *value)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)value) == TS_SUCCESS);

  OverridableDataType type;
  TSMgmtFloat* dest = static_cast<TSMgmtFloat*>(_conf_to_memberp(conf, ((HttpSM*)txnp)->t_state.txn_conf, &type));

  if (type != OVERRIDABLE_TYPE_FLOAT)
    return TS_ERROR;

  if (dest) {
    *value = *dest;
    return TS_SUCCESS;
  }

  return TS_SUCCESS;
}

TSReturnCode
TSHttpTxnConfigStringSet(TSHttpTxn txnp, TSOverridableConfigKey conf, const char* value, int length)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)value) == TS_SUCCESS);

  if (length == -1)
    length = strlen(value);

  HttpSM *s = (HttpSM*) txnp;

  s->t_state.setup_per_txn_configs();

  switch (conf) {
  case TS_CONFIG_HTTP_RESPONSE_SERVER_STR:
    s->t_state.txn_conf->proxy_response_server_string = const_cast<char*>(value); // The "core" likes non-const char*
    s->t_state.txn_conf->proxy_response_server_string_len = length;
    break;
  default:
    return TS_ERROR;
    break;
  }

  return TS_SUCCESS;
}


TSReturnCode
TSHttpTxnConfigStringGet(TSHttpTxn txnp, TSOverridableConfigKey conf, const char **value, int *length)
{
  sdk_assert(sdk_sanity_check_txn(txnp) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void**)value) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)length) == TS_SUCCESS);

  HttpSM *sm = (HttpSM*) txnp;

  switch (conf) {
  case TS_CONFIG_HTTP_RESPONSE_SERVER_STR:
    *value = sm->t_state.txn_conf->proxy_response_server_string;
    *length = sm->t_state.txn_conf->proxy_response_server_string_len;
    break;
  default:
    return TS_ERROR;
    break;
  }

  return TS_SUCCESS;
}


TSReturnCode
TSHttpConfigParamSet(OverridableHttpConfigParams *overridableHttpConfig, 
    const char* name, int nameLen, const char *value, int valueLen) 
{
  TSOverridableConfigKey conf;
  TSRecordDataType recordType;
  OverridableDataType dataType;
  TSReturnCode ret;

  if ((ret=TSHttpTxnConfigFind(name, nameLen, &conf, &recordType)) != TS_SUCCESS) {
    return ret;
  }

  void *dest = _conf_to_memberp(conf, overridableHttpConfig, &dataType);
  if (dest == NULL) {
    return TS_ERROR;
  }

  switch (dataType) {
    case OVERRIDABLE_TYPE_BYTE:
      *(static_cast<TSMgmtByte *>(dest)) = static_cast<TSMgmtByte>(atoi(value));
      break;
    case OVERRIDABLE_TYPE_INT:
      *(static_cast<TSMgmtInt *>(dest)) = static_cast<TSMgmtInt>(strtoll(value, NULL, 10));
      break;
    case OVERRIDABLE_TYPE_FLOAT:
      *(static_cast<TSMgmtFloat *>(dest)) = static_cast<TSMgmtFloat>(strtof(value, NULL));
      break;
    case OVERRIDABLE_TYPE_STRING:
      if (conf == TS_CONFIG_HTTP_RESPONSE_SERVER_STR) {
        if (overridableHttpConfig->proxy_response_server_string != NULL) {
          ats_free(overridableHttpConfig->proxy_response_server_string);
        }
        overridableHttpConfig->proxy_response_server_string = ats_strdup(value);
        overridableHttpConfig->proxy_response_server_string_len = valueLen;
        break;
      }
      else {
        return TS_ERROR;
      }
    default:
      return TS_ERROR;
  }

  return TS_SUCCESS;
}


// This is pretty suboptimal, and should only be used outside the critical path.
TSReturnCode
TSHttpTxnConfigFind(const char* name, int length, TSOverridableConfigKey *conf, TSRecordDataType *type)
{
  sdk_assert(sdk_sanity_check_null_ptr((void*)name) == TS_SUCCESS);
  sdk_assert(sdk_sanity_check_null_ptr((void*)conf) == TS_SUCCESS);

  TSOverridableConfigKey cnf = TS_CONFIG_NULL;
  TSRecordDataType typ = TS_RECORDDATATYPE_INT;

  if (length == -1)
    length = strlen(name);

  // Lots of string comparisons here, but we avoid quite a few by checking lengths
  switch (length) {
  case 28:
    if (!strncmp(name, "proxy.config.http.cache.http", length))
      cnf = TS_CONFIG_HTTP_CACHE_HTTP;
    break;

  case 30:
    if (!strncmp(name, "proxy.config.http.allow_anyway", length))
      cnf = TS_CONFIG_HTTP_ALLOW_ANYWAY;
    break;

  case 31:
    if (!strncmp(name, "proxy.config.http.max_bandwidth", length))
      cnf = TS_CONFIG_HTTP_MAX_BANDWIDTH;
    break;

  case 33:
    if (!strncmp(name, "proxy.config.http.cache.fuzz.time", length))
      cnf = TS_CONFIG_HTTP_CACHE_FUZZ_TIME;
    break;

  case 34:
    if (!strncmp(name, "proxy.config.http.chunking_enabled", length))
      cnf = TS_CONFIG_HTTP_CHUNKING_ENABLED;
    break;

  case 36:
    if (!strncmp(name, "proxy.config.net.sock_packet_tos_out", length))
      cnf = TS_CONFIG_NET_SOCK_PACKET_TOS_OUT;
    else if (!strncmp(name, "proxy.config.http.cache.force_in_ram", length))
      cnf = TS_CONFIG_HTTP_CACHE_FORCE_IN_RAM;
    break;

  case 37:
    switch (name[length-1]) {
    case 'e':
      if (!strncmp(name, "proxy.config.http.cache.max_stale_age", length))
        cnf = TS_CONFIG_HTTP_CACHE_MAX_STALE_AGE;
      else if (!strncmp(name, "proxy.config.http.cache.fuzz.min_time", length))
        cnf = TS_CONFIG_HTTP_CACHE_FUZZ_MIN_TIME;
      break;
    case 'r':
      if (!strncmp(name, "proxy.config.http.response_server_str", length)) {
        cnf = TS_CONFIG_HTTP_RESPONSE_SERVER_STR;
        typ = TS_RECORDDATATYPE_STRING;
      }
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.keep_alive_post_out", length))
        cnf = TS_CONFIG_HTTP_KEEP_ALIVE_POST_OUT;
      else if (!strncmp(name, "proxy.config.net.sock_option_flag_out", length))
        cnf = TS_CONFIG_NET_SOCK_OPTION_FLAG_OUT;
      else if (!strncmp(name, "proxy.config.net.sock_packet_mark_out", length))
        cnf = TS_CONFIG_NET_SOCK_PACKET_MARK_OUT;
      break;
    case 'n':
      if (!strncmp(name, "proxy.config.net.sock_flow_control_in", length))
        cnf = TS_CONFIG_NET_SOCK_FLOW_CTL_IN;
      break;
    }
    break;

  case 38:
    switch (name[length-1]) {
    case 'd':
      if (!strncmp(name, "proxy.config.http.server_tcp_init_cwnd", length))
        cnf = TS_CONFIG_HTTP_SERVER_TCP_INIT_CWND;
      break;
    case 's':
      if (!strncmp(name, "proxy.config.http.send_http11_requests", length))
        cnf = TS_CONFIG_HTTP_SEND_HTTP11_REQUESTS;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.net.sock_flow_control_out", length))
        cnf = TS_CONFIG_NET_SOCK_FLOW_CTL_OUT;
      break;
    }
    break;

  case 39:
    switch (name[length-1]) {
    case 'm':
      if (!strncmp(name, "proxy.config.http.anonymize_remove_from", length))
        cnf = TS_CONFIG_HTTP_ANONYMIZE_REMOVE_FROM;
      break;
    case 'n':
      if (!strncmp(name, "proxy.config.http.keep_alive_enabled_in", length))
        cnf = TS_CONFIG_HTTP_KEEP_ALIVE_ENABLED_IN;
      break;
    case 's':
      if (!strncmp(name, "proxy.config.http.doc_in_cache_skip_dns", length))
        cnf = TS_CONFIG_HTTP_DOC_IN_CACHE_SKIP_DNS;
      else if (!strncmp(name, "proxy.config.http.share_server_sessions", length))
        cnf = TS_CONFIG_HTTP_SHARE_SERVER_SESSIONS;
      break;
    }
    break;

  case 40:
    switch (name[length-1]) {
    case 'e':
      if (!strncmp(name, "proxy.config.http.down_server.cache_time", length))
        cnf = TS_CONFIG_HTTP_DOWN_SERVER_CACHE_TIME;
      break;
    case 'r':
      if (!strncmp(name, "proxy.config.url_remap.pristine_host_hdr", length))
        cnf = TS_CONFIG_URL_REMAP_PRISTINE_HOST_HDR;
      else if (!strncmp(name, "proxy.config.http.insert_request_via_str", length))
        cnf = TS_CONFIG_HTTP_INSERT_REQUEST_VIA_STR;
      break;
    case 's':
      if (!strncmp(name, "proxy.config.http.origin_max_connections", length))
        cnf = TS_CONFIG_HTTP_ORIGIN_MAX_CONNECTIONS;
      else if (!strncmp(name, "proxy.config.http.cache.required_headers", length))
        cnf = TS_CONFIG_HTTP_CACHE_REQUIRED_HEADERS;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.keep_alive_enabled_out", length))
        cnf = TS_CONFIG_HTTP_KEEP_ALIVE_ENABLED_OUT;
      break;
    case 'y':
      if (!strncmp(name, "proxy.config.http.cache.fuzz.probability", length))
        cnf = TS_CONFIG_HTTP_CACHE_FUZZ_PROBABILITY;
      break;
    }
    break;

  case 41:
    switch (name[length-1]) {
    case 'd':
      if (!strncmp(name, "proxy.config.http.response_server_enabled", length))
        cnf = TS_CONFIG_HTTP_RESPONSE_SERVER_ENABLED;
      break;
    case 'e':
      if (!strncmp(name, "proxy.config.http.anonymize_remove_cookie", length))
        cnf = TS_CONFIG_HTTP_ANONYMIZE_REMOVE_COOKIE;
      break;
    case 'r':
      if (!strncmp(name, "proxy.config.http.insert_response_via_str", length))
        cnf = TS_CONFIG_HTTP_INSERT_RESPONSE_VIA_STR;
      break;
    }
    break;

  case 42:
    switch (name[length-1]) {
    case 'd':
      if (!strncmp(name, "proxy.config.http.negative_caching_enabled", length))
        cnf = TS_CONFIG_HTTP_NEGATIVE_CACHING_ENABLED;
      break;
    case 'e':
      if (!strncmp(name, "proxy.config.http.cache.when_to_revalidate", length))
        cnf = TS_CONFIG_HTTP_CACHE_WHEN_TO_REVALIDATE;
      break;
    case 'r':
      if (!strncmp(name, "proxy.config.http.anonymize_remove_referer", length))
        cnf = TS_CONFIG_HTTP_ANONYMIZE_REMOVE_REFERER;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.net.sock_recv_buffer_size_out", length))
        cnf = TS_CONFIG_NET_SOCK_RECV_BUFFER_SIZE_OUT;
      else if (!strncmp(name, "proxy.config.net.sock_send_buffer_size_out", length))
        cnf = TS_CONFIG_NET_SOCK_SEND_BUFFER_SIZE_OUT;
      else if (!strncmp(name, "proxy.config.http.connect_attempts_timeout", length))
        cnf = TS_CONFIG_HTTP_CONNECT_ATTEMPTS_TIMEOUT;
      break;
    }
    break;

  case 43:
    switch (name[length-1]) {
    case 'e':
      if (!strncmp(name, "proxy.config.http.negative_caching_lifetime", length))
        cnf = TS_CONFIG_HTTP_NEGATIVE_CACHING_LIFETIME;
      break;
    case 'r':
      if (!strncmp(name, "proxy.config.http.cache.heuristic_lm_factor", length))
        cnf = TS_CONFIG_HTTP_CACHE_HEURISTIC_LM_FACTOR;
      break;
    case 'l':
      if (!strncmp(name, "proxy.config.http.cache.cluster_cache_local", length))
        cnf = TS_CONFIG_HTTP_CACHE_CLUSTER_CACHE_LOCAL;
      break;
    case 'd':
      if (!strncmp(name, "proxy.config.http.range_elimination_enabled", length))
        cnf = TS_CONFIG_HTTP_RANGE_ELIMINATION;
      break;
    }
    break;

  case 44:
    switch (name[length-1]) {
    case 'p':
      if (!strncmp(name, "proxy.config.http.anonymize_remove_client_ip", length))
        cnf = TS_CONFIG_HTTP_ANONYMIZE_REMOVE_CLIENT_IP;
      else if (!strncmp(name, "proxy.config.http.anonymize_insert_client_ip", length))
        cnf = TS_CONFIG_HTTP_ANONYMIZE_INSERT_CLIENT_IP;
      break;
    case 'e':
      if (!strncmp(name, "proxy.config.http.cache.open_read_retry_time", length))
        cnf = TS_CONFIG_HTTP_CACHE_OPEN_READ_RETRY_TIME;
      break;
    }
    break;

  case 45:
    switch (name[length-1]) {
    case 'd':
      if (!strncmp(name, "proxy.config.http.down_server.abort_threshold", length))
        cnf = TS_CONFIG_HTTP_DOWN_SERVER_ABORT_THRESHOLD;
      break;
    case 'n':
      if (!strncmp(name, "proxy.config.http.cache.ignore_authentication", length))
        cnf = TS_CONFIG_HTTP_CACHE_IGNORE_AUTHENTICATION;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.anonymize_remove_user_agent", length))
        cnf = TS_CONFIG_HTTP_ANONYMIZE_REMOVE_USER_AGENT;
      break;
    case 's':
      if (!strncmp(name, "proxy.config.http.connect_attempts_rr_retries", length))
        cnf = TS_CONFIG_HTTP_CONNECT_ATTEMPTS_RR_RETRIES;
      else if (!strncmp(name, "proxy.config.http.cache.max_open_read_retries", length))
        cnf = TS_CONFIG_HTTP_CACHE_MAX_OPEN_READ_RETRIES;
      break;
    }
    break;

  case 46:
    switch (name[length-1]) {
    case 'e':
      if (!strncmp(name, "proxy.config.http.cache.ignore_client_no_cache", length))
        cnf = TS_CONFIG_HTTP_CACHE_IGNORE_CLIENT_NO_CACHE;
      else if (!strncmp(name, "proxy.config.http.cache.ims_on_client_no_cache", length))
        cnf = TS_CONFIG_HTTP_CACHE_IMS_ON_CLIENT_NO_CACHE;
      else if (!strncmp(name, "proxy.config.http.cache.ignore_server_no_cache", length))
        cnf = TS_CONFIG_HTTP_CACHE_IGNORE_SERVER_NO_CACHE;
      else if (!strncmp(name, "proxy.config.http.cache.heuristic_min_lifetime", length))
        cnf = TS_CONFIG_HTTP_CACHE_HEURISTIC_MIN_LIFETIME;
      else if (!strncmp(name, "proxy.config.http.cache.heuristic_max_lifetime", length))
        cnf = TS_CONFIG_HTTP_CACHE_HEURISTIC_MAX_LIFETIME;
      break;
    case 'r':
      if (!strncmp(name, "proxy.config.http.insert_squid_x_forwarded_for", length))
        cnf = TS_CONFIG_HTTP_INSERT_SQUID_X_FORWARDED_FOR;
      break;
    case 's':
      if (!strncmp(name, "proxy.config.http.connect_attempts_max_retries", length))
        cnf = TS_CONFIG_HTTP_CONNECT_ATTEMPTS_MAX_RETRIES;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.forward.proxy_auth_to_parent", length))
        cnf = TS_CONFIG_HTTP_FORWARD_PROXY_AUTH_TO_PARENT;
      break;
    case 'y':
      if (!strncmp(name, "proxy.config.cache.read_while_writer.max_delay", length))
        cnf = TS_CONFIG_HTTP_CACHE_MAX_RWW_DELAY;
      break;
    }
    break;

  case 47:
    switch (name[length-1]) {
    case 'e':
      if (!strncmp(name, "proxy.config.http.cache.guaranteed_min_lifetime", length))
        cnf = TS_CONFIG_HTTP_CACHE_GUARANTEED_MIN_LIFETIME;
      else if (!strncmp(name, "proxy.config.http.cache.guaranteed_max_lifetime", length))
        cnf = TS_CONFIG_HTTP_CACHE_GUARANTEED_MAX_LIFETIME;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.post_connect_attempts_timeout", length))
        cnf = TS_CONFIG_HTTP_POST_CONNECT_ATTEMPTS_TIMEOUT;
      break;
    case 's':
      if (!strncmp(name, "proxy.config.http.max_active_client_connections", length))
        cnf = TS_CONFIG_HTTP_MAX_ACTIVE_CLIENT_CONNECTIONS;
      break;
    }
    break;

  case 48:
    switch (name[length-1]) {
    case 'e':
      if (!strncmp(name, "proxy.config.http.cache.ignore_client_cc_max_age", length))
        cnf = TS_CONFIG_HTTP_CACHE_IGNORE_CLIENT_CC_MAX_AGE;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.transaction_active_timeout_out", length))
        cnf = TS_CONFIG_HTTP_TRANSACTION_ACTIVE_TIMEOUT_OUT;
      break;
    }
    break;

  case 50:
    if (!strncmp(name, "proxy.config.http.cache.cache_responses_to_cookies", length))
      cnf = TS_CONFIG_HTTP_CACHE_CACHE_RESPONSES_TO_COOKIES;
    break;

  case 51:
    if (!strncmp(name, "proxy.config.http.keep_alive_no_activity_timeout_in", length))
      cnf = TS_CONFIG_HTTP_KEEP_ALIVE_NO_ACTIVITY_TIMEOUT_IN;
    break;

  case 52:
    switch (name[length-1]) {
    case 'c':
      if (!strncmp(name, "proxy.config.http.cache.cache_urls_that_look_dynamic", length))
        cnf = TS_CONFIG_HTTP_CACHE_CACHE_URLS_THAT_LOOK_DYNAMIC;
      break;
    case 'n':
      if (!strncmp(name, "proxy.config.http.transaction_no_activity_timeout_in", length))
        cnf = TS_CONFIG_HTTP_TRANSACTION_NO_ACTIVITY_TIMEOUT_IN;
      break;
    case 't':
      if (!strncmp(name, "proxy.config.http.keep_alive_no_activity_timeout_out", length))
        cnf = TS_CONFIG_HTTP_KEEP_ALIVE_NO_ACTIVITY_TIMEOUT_OUT;
      break;
    }
    break;

  case 53:
    if (!strncmp(name, "proxy.config.http.transaction_no_activity_timeout_out", length))
      cnf = TS_CONFIG_HTTP_TRANSACTION_NO_ACTIVITY_TIMEOUT_OUT;
    break;

  case 58:
   if (!strncmp(name, "proxy.config.http.connect_attempts_max_retries_dead_server", length))
     cnf = TS_CONFIG_HTTP_CONNECT_ATTEMPTS_MAX_RETRIES_DEAD_SERVER;
   break;
  }

  *conf = cnf;
  if (type)
    *type = typ;

  return ((cnf != TS_CONFIG_NULL) ? TS_SUCCESS: TS_ERROR);
}

TSReturnCode
TSHttpTxnPrivateSessionSet(TSHttpTxn txnp, int private_session)
{
  if (sdk_sanity_check_txn(txnp) != TS_SUCCESS) {
    return TS_ERROR;
  }
  HttpSM *sm = (HttpSM *) txnp;
  if (sm->set_server_session_private(private_session)) {
    return TS_SUCCESS;
  }
  return TS_ERROR;
}

// APIs to register new Mgmt (records.config) entries.
TSReturnCode
TSMgmtStringCreate(TSRecordType rec_type, const char *name, const TSMgmtString data_default,
                   TSRecordUpdateType update_type, TSRecordCheckType check_type,
                   const char *check_regex, TSRecordAccessType access_type)
{
  if (REC_ERR_OKAY != RecRegisterConfigString((enum RecT)rec_type, name, data_default, (enum RecUpdateT)update_type,
                                              (enum RecCheckT)check_type, check_regex, (enum RecAccessT)access_type))
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode
TSMgmtIntCreate(TSRecordType rec_type, const char *name, TSMgmtInt data_default,
                TSRecordUpdateType update_type, TSRecordCheckType check_type,
                const char *check_regex, TSRecordAccessType access_type)
{
  if (REC_ERR_OKAY != RecRegisterConfigInt((enum RecT)rec_type, name, (RecInt)data_default, (enum RecUpdateT)update_type,
                                           (enum RecCheckT)check_type, check_regex, (enum RecAccessT)access_type))
    return TS_ERROR;

  return TS_SUCCESS;
}

TSReturnCode TSCacheUrlGet(const char *in_url, const int in_url_len,
    char *out_url, const int out_size, int *out_url_len, int *flags)
{
  if (RemapProcessor::convert_cache_url(in_url, in_url_len,
        out_url, out_size, out_url_len, flags))
  {
    return TS_SUCCESS;
  }
  else {
    return TS_ERROR;
  }
}

TSReturnCode TSSpdyAcceptHandlerSet(TSSpdyAcceptHandler fn)
{
  if (spdy_accept != NULL || fn == NULL)
    return TS_ERROR;
  spdy_accept = (SpdyAcceptHandlerPtr) fn;
  return TS_SUCCESS;
}
#endif //TS_NO_API

