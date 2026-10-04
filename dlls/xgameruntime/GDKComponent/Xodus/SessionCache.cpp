/* Volatile Xbox bootstrap sessions shared through Xodus.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../../private.h"
#include "../../WineCoreUAP/Foundation/IWineAsync.hpp"
#include "Structs.h"
#include "robuffer.h"
#include <libxml/parser.h>
#include <new>

using namespace ABI;
using namespace ABI::Xodus;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;

BOOLEAN xodusSessionCacheAvailable = FALSE;

HRESULT xodus_session_cache( const char *operation, const char *puid, LONGLONG expiry,
                            const char *input, char **output, LONGLONG *returned_expiry )
{
    IBufferByteAccess *request_access = nullptr, *response_access = nullptr;
    IXodusIPCPacket *request = nullptr, *response = nullptr;
    IBuffer *request_buffer = nullptr, *response_buffer = nullptr;
    IAsyncOperation<IXodusIPCPacket *> *async = nullptr;
    IBufferFactory *factory = nullptr;
    HSTRING_HEADER class_header;
    HSTRING class_name;
    xmlDocPtr doc = nullptr, reply = nullptr;
    xmlNodePtr root, node;
    xmlChar *xml = nullptr, *content = nullptr;
    char title[16], expires[32];
    BYTE *buffer;
    UINT32 length;
    UINT16 type;
    int size;
    HRESULT hr = S_FALSE;

    if (output) *output = nullptr;
    if (returned_expiry) *returned_expiry = 0;
    /* Unknown messages on older brokers cannot be matched to a response. */
    if (!xodusSessionCacheAvailable || !puid || !*puid || !msaAppId || !*msaAppId) return S_FALSE;
    if (strlen(puid) > 256 || strlen(msaAppId) > 256 || (input && strlen(input) > 60000)) return S_FALSE;
    if (!(doc = xmlNewDoc( BAD_CAST "1.0" ))) return E_OUTOFMEMORY;
    root = xmlNewNode( nullptr, BAD_CAST "GdkSessionRequest" );
    if (!root) { xmlFreeDoc(doc); return E_OUTOFMEMORY; }
    xmlDocSetRootElement( doc, root );
    sprintf( title, "%u", titleId );
    sprintf( expires, "%lld", expiry );
    if (!xmlNewTextChild( root, nullptr, BAD_CAST "Operation", BAD_CAST operation ) ||
        !xmlNewTextChild( root, nullptr, BAD_CAST "Puid", BAD_CAST puid ) ||
        !xmlNewTextChild( root, nullptr, BAD_CAST "ClientId", BAD_CAST msaAppId ) ||
        !xmlNewTextChild( root, nullptr, BAD_CAST "TitleId", BAD_CAST title ) ||
        !xmlNewTextChild( root, nullptr, BAD_CAST "FullTrust", BAD_CAST (fullTrust ? "true" : "false") ) ||
        !xmlNewTextChild( root, nullptr, BAD_CAST "Expiry", BAD_CAST expires ) ||
        !xmlNewTextChild( root, nullptr, BAD_CAST "Data", BAD_CAST (input ? input : "") ))
    { hr = E_OUTOFMEMORY; goto done; }
    xmlDocDumpMemoryEnc( doc, &xml, &size, "UTF-8" );
    if (!xml) { hr = E_OUTOFMEMORY; goto done; }
    if (size > 65535) goto done;
    if (FAILED(hr = WindowsCreateStringReference( RuntimeClass_Windows_Storage_Streams_Buffer,
            wcslen(RuntimeClass_Windows_Storage_Streams_Buffer), &class_header, &class_name ))) goto done;
    if (FAILED(hr = RoGetActivationFactory( class_name, __uuidof(IBufferFactory), (void **)&factory ))) goto done;
    if (FAILED(hr = factory->Create( size, &request_buffer ))) goto done;
    if (FAILED(hr = request_buffer->QueryInterface<IBufferByteAccess>( &request_access ))) goto done;
    if (FAILED(hr = request_access->Buffer( &buffer ))) goto done;
    memcpy( buffer, xml, size );
    if (FAILED(hr = request_buffer->put_Length( size ))) goto done;
    request = new (std::nothrow) XodusIPCPacket( MagicHeaderType::XML, 11, request_buffer );
    if (!request) { hr = E_OUTOFMEMORY; goto done; }
    if (FAILED(hr = xodus_ipclayer->SendRequestAsync( request, &async ))) goto done;
    if (!async || AsyncOperationCompletedHandler<IXodusIPCPacket *>::await_AsyncOperation( async, INFINITE ))
    { hr = E_FAIL; goto done; }
    if (FAILED(hr = async->GetResults( &response ))) goto done;
    hr = E_FAIL;
    if (!response || FAILED(response->get_MessageType( &type )) || type != 12) goto done;
    if (FAILED(hr = response->get_Message( &response_buffer ))) goto done;
    if (FAILED(hr = response_buffer->get_Length( &length ))) goto done;
    if (FAILED(hr = response_buffer->QueryInterface<IBufferByteAccess>( &response_access ))) goto done;
    if (FAILED(hr = response_access->Buffer( &buffer ))) goto done;
    hr = S_FALSE;
    if (!length || length > 65535) goto done;
    reply = xmlReadMemory( (char *)buffer, length, nullptr, nullptr,
                          XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING );
    if (!reply || reply->intSubset || reply->extSubset || !(root = xmlDocGetRootElement(reply)) ||
        xmlStrcmp(root->name, BAD_CAST "GdkSessionResponse")) goto done;
    for (node = root->children; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE) continue;
        content = xmlNodeGetContent(node);
        if (!content) continue;
        if (!xmlStrcmp(node->name, BAD_CAST "Status") &&
            (!xmlStrcmp(content, BAD_CAST "hit") || !xmlStrcmp(content, BAD_CAST "stored") ||
             !xmlStrcmp(content, BAD_CAST "invalidated"))) hr = S_OK;
        if (!xmlStrcmp(node->name, BAD_CAST "Expiry") && returned_expiry)
            *returned_expiry = _strtoi64((char *)content, nullptr, 10);
        if (!xmlStrcmp(node->name, BAD_CAST "Data") && output && !*output && xmlStrlen(content) <= 60000)
            *output = strdup((char *)content);
        xmlFree(content);
        content = nullptr;
    }
done:
    if (response_access) response_access->Release();
    if (request_access) request_access->Release();
    if (response_buffer) response_buffer->Release();
    if (request_buffer) request_buffer->Release();
    if (response) response->Release();
    if (request) request->Release();
    if (async) async->Release();
    if (factory) factory->Release();
    if (xml) { SecureZeroMemory(xml, size); xmlFree(xml); }
    xmlFreeDoc(reply);
    xmlFreeDoc(doc);
    return hr;
}
