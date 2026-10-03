#!/usr/bin/env python3
"""Run native XML and async lifetime regressions against extracted implementation.

Requires a C++17 compiler with ASan/UBSan and libxml2 development files. WinRT,
COM reference counting, and threadpool dispatch are mocked; the tested method
bodies come directly from the source tree. No network or wineserver is needed.
Run: python3 dlls/xgameruntime/tests/runtime.py
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def method(source, prefix):
    """Extract one definition, ignoring braces in strings and comments."""
    start = source.index(prefix)
    brace = source.index("{", start)
    depth = 0
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]'
    for token in re.finditer(pattern, source[brace:], re.S):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                return source[start:brace + token.end()]
    raise ValueError(prefix)


def body(source, prefix):
    definition = method(source, prefix)
    return definition[definition.index("{"):]

XML_PRELUDE = r'''
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <new>
#include <cstdio>
#include <initializer_list>
using HRESULT = int32_t;
using boolean = unsigned char;
using LPSTR = char *;
using LPCSTR = const char *;
#define TRACE(...) ((void)0)
#define S_OK 0
#define E_POINTER ((HRESULT)0x80004003)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_UNEXPECTED ((HRESULT)0x8000ffff)
#define E_OUTOFMEMORY ((HRESULT)0x8007000e)
struct IMsaTokenResponse { virtual ~IMsaTokenResponse() = default; };
struct MsaTokenResponse : IMsaTokenResponse { const char *token; MsaTokenResponse():token(nullptr) {} HRESULT get_Token(const char **out); explicit MsaTokenResponse(const char *t):token(t) {} ~MsaTokenResponse() {free((void *)token);} };
'''

XML_TESTS = r'''
int main() {
    char *xml=nullptr;
    assert(build(nullptr,0,0,&xml)==E_INVALIDARG && !xml);
    assert(build("a<&b",1,0,nullptr)==E_POINTER);
    assert(build("a<&b",1,0,&xml)==S_OK);
    xmlDocPtr doc=xmlReadMemory(xml,strlen(xml),nullptr,nullptr,XML_PARSE_NONET);
    assert(doc);
    xmlNodePtr child=xmlDocGetRootElement(doc)->children;
    while(child && child->type!=XML_ELEMENT_NODE) child=child->next;
    xmlChar *text=xmlNodeGetContent(child);
    assert(!strcmp((const char *)text,"a<&b"));
    xmlFree(text); xmlFreeDoc(doc); free(xml);
    const char *invalid[]={nullptr,"", "<MSATokenResponse>","<wrong><Token>value</Token></wrong>","<MSATokenResponse/>","<MSATokenResponse><Token/></MSATokenResponse>","<!DOCTYPE MSATokenResponse [<!ENTITY secret 'bad'>]><MSATokenResponse><Token>&secret;</Token></MSATokenResponse>"};
    for (const char *input: invalid) { IMsaTokenResponse *result=(IMsaTokenResponse *)1; assert(parse(input,&result)==E_INVALIDARG); assert(!result); }
    assert(parse("<MSATokenResponse/>",nullptr)==E_POINTER);
    for (const char *input : {"<MSATokenResponse><Token>a&amp;b</Token></MSATokenResponse>","<MsaTokenResponse><Token>a&amp;b</Token></MsaTokenResponse>"}) {
      IMsaTokenResponse *result=nullptr; assert(parse(input,&result)==S_OK); assert(result); assert(!strcmp(static_cast<MsaTokenResponse *>(result)->token,"a&b")); delete result;
    }
    MsaTokenResponse missing;
    const char *value = reinterpret_cast<const char *>(1);
    assert(missing.get_Token(&value) == E_UNEXPECTED && !value);
    assert(missing.get_Token(nullptr) == E_POINTER);
    MsaTokenResponse present(strdup("token"));
    assert(present.get_Token(&value) == S_OK && !strcmp(value, "token"));
    free(const_cast<char *>(value));
    xmlCleanupParser(); puts("PASS: XML escaping, malformed responses, token ownership and NULL arguments");
}
'''

ASYNC_PRELUDE = r'''
#include <atomic>
#include <mutex>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <new>
#include <initializer_list>
using HRESULT=int32_t; using ULONG=unsigned long; using PVOID=void *;
#define WINAPI
#define CALLBACK
#define TRACE(...) ((void)0)
#define S_OK 0
#define E_FAIL ((HRESULT)0x80004005)
#define E_POINTER ((HRESULT)0x80004003)
#define E_ABORT ((HRESULT)0x80004004)
#define E_UNEXPECTED ((HRESULT)0x8000ffff)
#define E_ILLEGAL_METHOD_CALL ((HRESULT)0x8000000e)
#define E_ILLEGAL_STATE_CHANGE ((HRESULT)0x8000000d)
#define E_ILLEGAL_DELEGATE_ASSIGNMENT ((HRESULT)0x80000018)
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
#define VT_UNKNOWN 13
#define HANDLER_NOT_SET ((void *)~uintptr_t(0))
#define RO_INIT_MULTITHREADED 1
static int apartment_refs=0;
HRESULT RoInitialize(int) {++apartment_refs; return S_OK;}
void RoUninitialize() {--apartment_refs;}
struct IUnknown { virtual ULONG AddRef() noexcept=0; virtual ULONG Release() noexcept=0; virtual ~IUnknown()=default; };
using IInspectable=IUnknown;
using IErrorInfo=IUnknown;
struct PROPVARIANT {unsigned short vt=0; IUnknown *punkVal=nullptr;};
void PropVariantInit(PROPVARIANT *p) {*p={};}
HRESULT PropVariantCopy(PROPVARIANT *dst,const PROPVARIANT *src) {*dst=*src;if(dst->vt==VT_UNKNOWN && dst->punkVal)dst->punkVal->AddRef();return S_OK;}
void PropVariantClear(PROPVARIANT *p) {if(p->vt==VT_UNKNOWN && p->punkVal)p->punkVal->Release();*p={};}
void SetErrorInfo(int,IErrorInfo *) {}
void GetErrorInfo(int,IErrorInfo **) {}
struct TP_CALLBACK_INSTANCE {}; struct TP_WORK {};
static int closed_work = 0;
void CloseThreadpoolWork(TP_WORK *work) {assert(work); ++closed_work;}
/* ASYNC_STATUS_DECLARATION */
struct IWineAsyncOperationCompletedHandler : IUnknown {virtual HRESULT Invoke(IInspectable *,AsyncStatus)=0;};
using async_operation_callback=HRESULT (*)(IUnknown *,PVOID,PROPVARIANT *);
class AsyncInfo {
public:
 std::atomic_long ref{1}; std::mutex mutex;
 IWineAsyncOperationCompletedHandler *handler=(IWineAsyncOperationCompletedHandler *)HANDLER_NOT_SET;
 IInspectable *IInspectable_outer=nullptr; IErrorInfo *errorInfo=nullptr; IUnknown *invoker=nullptr;
 async_operation_callback callback=nullptr; AsyncStatus status=Started; bool closed=false; PROPVARIANT result{}; HRESULT hr=S_OK;
 TP_WORK *async_run_work=nullptr; PVOID param=nullptr;
 ULONG Release() noexcept;
 HRESULT put_Completed(IWineAsyncOperationCompletedHandler *) noexcept;
 HRESULT get_Result(PROPVARIANT *) noexcept;
 HRESULT get_Status(AsyncStatus *) noexcept;
 HRESULT get_ErrorCode(HRESULT *) noexcept;
 HRESULT get_Completed(IWineAsyncOperationCompletedHandler **) noexcept;
 HRESULT Cancel() noexcept;
 HRESULT Close() noexcept;
 static void async_info_callback(TP_CALLBACK_INSTANCE *,void *,TP_WORK *);
};
template<class T> class AsyncOperation : public IInspectable {
public:
 std::atomic_long ref{1}; AsyncInfo *info=nullptr;
 ULONG AddRef() noexcept override {return ++ref;}
 ULONG Release() noexcept override {ULONG current=--ref; if(!current)delete this;return current;}
 ~AsyncOperation() {if(info)info->Release();}
 HRESULT GetResults(T *) noexcept;
};
'''

ASYNC_TESTS = r'''
static int objects=0, handlers=0;
struct Value : IUnknown {int refs=1;Value(){++objects;}~Value(){--objects;} ULONG AddRef()noexcept override{return ++refs;} ULONG Release()noexcept override{auto n=--refs;if(!n)delete this;return n;}};
struct Handler : IWineAsyncOperationCompletedHandler {int refs=1; bool *called;Handler(bool *c):called(c){++handlers;}~Handler(){--handlers;} ULONG AddRef()noexcept override{return ++refs;}ULONG Release()noexcept override{auto n=--refs;if(!n)delete this;return n;}HRESULT Invoke(IInspectable *outer,AsyncStatus status)override{*called=true;IUnknown *value=nullptr;assert(status==Completed);assert(static_cast<AsyncOperation<IUnknown*> *>(outer)->GetResults(&value)==S_OK);assert(value);value->Release();return S_OK;}};
static HRESULT callback(IUnknown *,void *,PROPVARIANT *result){assert(apartment_refs==1);result->vt=VT_UNKNOWN;result->punkVal=new Value();return S_OK;}
static HRESULT failure(IUnknown *,void *,PROPVARIANT *){return E_FAIL;}
static AsyncOperation<IUnknown*> *create(async_operation_callback fn=callback){auto op=new AsyncOperation<IUnknown*>();op->info=new AsyncInfo();op->info->IInspectable_outer=op;op->info->callback=fn;op->info->async_run_work=reinterpret_cast<TP_WORK *>(1);op->AddRef();return op;}
int main(){
 for(bool completed_first: {false,true}) {
  auto op=create(); auto info=op->info; IUnknown *out=(IUnknown*)1; bool called=false;
  assert(op->GetResults(&out)==E_ILLEGAL_METHOD_CALL && !out);
  if(completed_first) AsyncInfo::async_info_callback(nullptr,info,nullptr);
  auto handler=new Handler(&called); assert(info->put_Completed(handler)==S_OK);handler->Release();
  if(!completed_first) AsyncInfo::async_info_callback(nullptr,info,nullptr);
  assert(called && handlers==0 && objects==1 && apartment_refs==0);
  assert(op->GetResults(&out)==S_OK && out);
  op->Release(); assert(objects==1); out->Release();assert(objects==0);
 }
 {auto op=create();assert(op->info->Cancel()==S_OK);AsyncInfo::async_info_callback(nullptr,op->info,nullptr);IUnknown *out=(IUnknown*)1;assert(op->GetResults(&out)==E_ABORT && !out);op->Release();assert(!objects);}
 {auto op=create(failure);AsyncInfo::async_info_callback(nullptr,op->info,nullptr);IUnknown *out=(IUnknown*)1;assert(op->GetResults(&out)==E_FAIL && !out);op->Release();assert(!objects);}
 {auto op=create();auto info=op->info;op->Release();AsyncInfo::async_info_callback(nullptr,info,nullptr);assert(!objects);}
 {
  auto op=create(); auto info=op->info; AsyncStatus status=Error;
  assert(info->get_Status(nullptr)==E_POINTER);
  assert(info->get_Status(&status)==S_OK && status==Started);
  assert(info->Close()==E_ILLEGAL_STATE_CHANGE);
  AsyncInfo::async_info_callback(nullptr,info,nullptr);
  assert(info->get_Status(&status)==S_OK && status==Completed);
  int previous_closed=closed_work;
  assert(info->Close()==S_OK && closed_work==previous_closed+1);
  assert(info->Close()==S_OK && closed_work==previous_closed+1);
  status=Error;
  assert(info->get_Status(&status)==E_ILLEGAL_METHOD_CALL && status==Error);
  HRESULT error=S_OK;
  assert(info->get_ErrorCode(&error)==E_ILLEGAL_METHOD_CALL);
  IWineAsyncOperationCompletedHandler *completion=nullptr;
  assert(info->get_Completed(&completion)==E_ILLEGAL_METHOD_CALL);
  assert(info->Cancel()==E_ILLEGAL_METHOD_CALL);
  IUnknown *out=(IUnknown *)1;
  assert(op->GetResults(&out)==E_ILLEGAL_METHOD_CALL && !out);
  bool called=false;
  auto handler=new Handler(&called);
  assert(info->put_Completed(handler)==E_ILLEGAL_METHOD_CALL);
  handler->Release();
  op->Release();
  assert(!called && !handlers && !objects && closed_work==previous_closed+1);
 }
 {
  auto op=create(); auto info=op->info; AsyncStatus status=Started; bool called=false;
  auto handler=new Handler(&called);
  assert(info->put_Completed(handler)==S_OK);
  handler->Release();
  assert(info->Cancel()==S_OK);
  assert(info->get_Status(&status)==S_OK && status==Canceled);
  int previous_closed=closed_work;
  assert(info->Close()==S_OK && closed_work==previous_closed+1);
  assert(info->get_Status(&status)==E_ILLEGAL_METHOD_CALL && status==Canceled);
  AsyncInfo::async_info_callback(nullptr,info,nullptr);
  assert(!called && !handlers && !objects);
  assert(info->get_Status(&status)==E_ILLEGAL_METHOD_CALL && status==Canceled);
  op->Release();
  assert(closed_work==previous_closed+1);
 }
 assert(!objects && !handlers && !apartment_refs);puts("PASS: async result ownership, reentrant completion, cancellation, closure and errors");
}
'''

def main():
    xml = (ROOT / "GDKComponent/Xodus/XodusXMLBuilder.cpp").read_text()
    structs = (ROOT / "GDKComponent/Xodus/Structs.cpp").read_text()
    async_source = (ROOT / "WineCoreUAP/Foundation/IWineAsync.cpp").read_text()
    xml_parts = [XML_PRELUDE,
                 "HRESULT build(const char *clientId, boolean allowUi, boolean fullTrust, LPSTR *xml_string) "
                 + body(xml, "BuildMsaTokenRequestXml("),
                 "HRESULT parse(LPCSTR xml_string, IMsaTokenResponse **response) "
                 + body(xml, "FromMsaTokenResponseXml("),
                 "HRESULT " + method(structs, "MsaTokenResponse::get_Token("), XML_TESTS]
    status_idl = (ROOT.parents[1] / "include/asyncinfo.idl").read_text()
    status_enum = method(status_idl, "enum AsyncStatus") + ";"
    async_parts = [ASYNC_PRELUDE.replace("/* ASYNC_STATUS_DECLARATION */", status_enum)]
    for declaration, prefix in (("ULONG ", "AsyncInfo::Release("),
                                ("HRESULT ", "AsyncInfo::put_Completed("),
                                ("HRESULT ", "AsyncInfo::get_Result("),
                                ("HRESULT ", "AsyncInfo::get_Status("),
                                ("HRESULT ", "AsyncInfo::get_ErrorCode("),
                                ("HRESULT ", "AsyncInfo::get_Completed("),
                                ("HRESULT ", "AsyncInfo::Cancel("),
                                ("HRESULT ", "AsyncInfo::Close("),
                                ("void ", "AsyncInfo::async_info_callback("),
                                ("template<class T> HRESULT ", "AsyncOperation<T>::GetResults(")):
        async_parts.append(declaration + method(async_source, prefix))
    async_parts.append(ASYNC_TESTS)
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    xml_flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "libxml-2.0"], text=True))
    with tempfile.TemporaryDirectory(prefix="winegdk-runtime-") as tmp:
        for name, parts, flags in (("xml", xml_parts, xml_flags), ("async", async_parts, [])):
            source, binary = Path(tmp) / (name + ".cpp"), Path(tmp) / name
            source.write_text("\n".join(parts))
            subprocess.run(compiler + ["-std=c++17", "-g", "-Wall", "-Wextra", "-Werror",
                                       "-Wno-unused-parameter", "-fsanitize=address,undefined",
                                       "-fno-omit-frame-pointer", str(source), "-o", str(binary)] + flags,
                           check=True)
            subprocess.run([str(binary)], check=True, timeout=20)


if __name__ == "__main__":
    main()
