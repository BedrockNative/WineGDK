/*
 * Xbox Game runtime Library
 *  Xodus Interopability Layer -> XodusXMLBuilder
 * 
 * Written by Weather
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "../../private.h"
#include "../../WineCoreUAP/Foundation/IWineAsync.hpp"
#include "../../WineCoreUAP/Foundation/IWineVector.hpp"
#include "Structs.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <atomic>
#include <new>
#include <windows.h>
#include <winstring.h>

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

WINE_DEFAULT_DEBUG_CHANNEL(xodus);

using namespace ABI;
using namespace ABI::Xodus;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;

class ABI::Xodus::XodusXMLBuilder :
    public IXodusXMLBuilder
{
public:
    XodusXMLBuilder() noexcept
    {
        xmlInitParser();
        LIBXML_TEST_VERSION
    }

    ~XodusXMLBuilder()
    {
        xmlCleanupParser();
    }

    /* IUnknown Methods */
    HRESULT WINAPI 
    QueryInterface( REFIID iid, void **out )
    {
        TRACE( "iface %p, iid %s, out %p.\n", this, debugstr_guid( &iid ), out );

        if (!out) return E_POINTER;
        *out = nullptr;

        if ( iid == __uuidof( IUnknown ) ||
             iid == __uuidof( IInspectable ) ||
             iid == __uuidof( IAgileObject ) ||
             iid == __uuidof( IXodusXMLBuilder ) )
        {
            AddRef();
            *out = static_cast<IXodusXMLBuilder *>(this);
            return S_OK;
        }

        FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( &iid ) );
        *out = nullptr;
        return E_NOINTERFACE;
    }

    ULONG WINAPI 
    AddRef() noexcept override
    {
        ULONG curr = static_cast<ULONG>(++ref);
        TRACE( "iface %p increasing refcount to %lu.\n", this, curr );
        return curr;
    }

    ULONG WINAPI 
    Release() noexcept override
    {
        ULONG curr = static_cast<ULONG>(--ref);
        TRACE( "iface %p decreasing refcount to %lu.\n", this, curr );

        // Polymorphic classes should not be deleted.
        /*
        if ( !curr )
            delete this;
        */

        return curr;
    }

    /* IInspectable Methods */
    HRESULT WINAPI
    GetIids( ULONG *iidCount, IID **iids ) override
    {
        FIXME("iface %p, iidCount %p, iids %p stub!\n", this, iidCount, iids);
        return E_NOTIMPL;
    }

    HRESULT WINAPI
    GetRuntimeClassName( HSTRING *className ) override 
    {
        FIXME("iface %p, className %p stub!\n", this, className);
        return E_NOTIMPL;
    }

    HRESULT WINAPI
    GetTrustLevel( TrustLevel *trustLevel ) override
    {
        FIXME("iface %p, trustLevel %p stub!\n", this, trustLevel);
        return E_NOTIMPL;
    }

    HRESULT WINAPI
    BuildMsaTokenRequestXml( const char *clientId, boolean allowUi, boolean fullTrust, LPSTR *xml_string ) override
    {
        xmlNodePtr root;
        xmlDocPtr doc;
        xmlChar *buffer = nullptr;
        int bufferSize = 0;
        HRESULT hr = E_OUTOFMEMORY;

        TRACE( "clientId %s, xml_string %p.\n", debugstr_a( clientId ), xml_string );
        if (!xml_string) return E_POINTER;
        *xml_string = nullptr;
        if (!clientId) return E_INVALIDARG;
        if (!(doc = xmlNewDoc( BAD_CAST "1.0" ))) return E_OUTOFMEMORY;
        if (!(root = xmlNewNode( nullptr, BAD_CAST "MsaTokenRequest" ))) goto cleanup;
        xmlDocSetRootElement( doc, root );
        if (!xmlNewTextChild( root, nullptr, BAD_CAST "ClientId", BAD_CAST clientId ) ||
            !xmlNewTextChild( root, nullptr, BAD_CAST "AllowUi", BAD_CAST (allowUi ? "true" : "false") ) ||
            !xmlNewTextChild( root, nullptr, BAD_CAST "MsaFullTrust", BAD_CAST (fullTrust ? "true" : "false") ))
            goto cleanup;

        xmlDocDumpFormatMemory( doc, &buffer, &bufferSize, 1 );
        if (!buffer || bufferSize <= 0) goto cleanup;
        if (!(*xml_string = static_cast<char *>(malloc( bufferSize + 1 )))) goto cleanup;
        memcpy( *xml_string, buffer, bufferSize );
        (*xml_string)[bufferSize] = 0;
        hr = S_OK;

    cleanup:
        xmlFree( buffer );
        xmlFreeDoc( doc );
        return hr;
    }

    HRESULT WINAPI
    FromMsaTokenResponseXml( LPCSTR xml_string, IMsaTokenResponse **response ) override
    {
        xmlNodePtr child, root;
        xmlChar *content = nullptr;
        char *token = nullptr, *puid = nullptr, *device_rps = nullptr;
        xmlDocPtr doc;
        HRESULT hr = E_INVALIDARG;

        TRACE( "response %p.\n", response );
        if (!response) return E_POINTER;
        *response = nullptr;
        xodusSessionCacheAvailable = FALSE;
        if (!xml_string) return E_INVALIDARG;
        if (!(doc = xmlReadMemory( xml_string, strlen( xml_string ), nullptr, nullptr, XML_PARSE_NONET )))
            return E_INVALIDARG;
        if (doc->intSubset || doc->extSubset) goto cleanup;
        if (!(root = xmlDocGetRootElement( doc ))) goto cleanup;
        if (xmlStrcmp( root->name, BAD_CAST "MSATokenResponse" ) &&
            xmlStrcmp( root->name, BAD_CAST "MsaTokenResponse" )) goto cleanup;

        for (child = root->children; child; child = child->next)
            if (child->type == XML_ELEMENT_NODE && !xmlStrcmp( child->name, BAD_CAST "Token" ))
            {
                content = xmlNodeGetContent( child );
                break;
            }
        if (!content || !*content) goto cleanup;
        if (!(token = strdup( reinterpret_cast<char *>(content) )))
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        xmlFree( content );
        content = nullptr;
        for (child = root->children; child; child = child->next)
            if (child->type == XML_ELEMENT_NODE && !xmlStrcmp( child->name, BAD_CAST "Puid" ))
            {
                content = xmlNodeGetContent( child );
                break;
            }
        if (content && *content && !(puid = strdup( reinterpret_cast<char *>(content) )))
        {
            free( token );
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        xmlFree( content );
        content = nullptr;
        for (child = root->children; child; child = child->next)
            if (child->type == XML_ELEMENT_NODE && !xmlStrcmp( child->name, BAD_CAST "DeviceRps" ))
            {
                content = xmlNodeGetContent( child );
                break;
            }
        if (content && *content && !(device_rps = strdup( reinterpret_cast<char *>(content) )))
        {
            free( token );
            free( puid );
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        if (!(*response = new (std::nothrow) MsaTokenResponse( token, puid, device_rps )))
        {
            free( token );
            free( puid );
            free( device_rps );
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        for (child = root->children; child; child = child->next)
            if (child->type == XML_ELEMENT_NODE && !xmlStrcmp( child->name, BAD_CAST "GdkSessionCacheVersion" ))
            {
                xmlChar *version = xmlNodeGetContent( child );
                xodusSessionCacheAvailable = version && !xmlStrcmp( version, BAD_CAST "1" );
                xmlFree( version );
                break;
            }
        hr = S_OK;

    cleanup:
        xmlFree( content );
        xmlFreeDoc( doc );
        return hr;
    }

private:
    std::atomic_long ref{ 1 };
};

static XodusXMLBuilder g_xodus_xml_builder;
IXodusXMLBuilder *xodus_xml_builder = static_cast<IXodusXMLBuilder*>(&g_xodus_xml_builder);
