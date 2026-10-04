@ cdecl -norelay ObjectStublessClient3() rpcrt4.ObjectStublessClient3
@ cdecl -norelay ObjectStublessClient4() rpcrt4.ObjectStublessClient4
@ cdecl -norelay ObjectStublessClient5() rpcrt4.ObjectStublessClient5
@ cdecl -norelay ObjectStublessClient6() rpcrt4.ObjectStublessClient6
@ cdecl -norelay ObjectStublessClient7() rpcrt4.ObjectStublessClient7
@ cdecl -norelay ObjectStublessClient8() rpcrt4.ObjectStublessClient8
@ cdecl -norelay ObjectStublessClient9() rpcrt4.ObjectStublessClient9
@ cdecl -norelay ObjectStublessClient10() rpcrt4.ObjectStublessClient10
@ cdecl -norelay ObjectStublessClient11() rpcrt4.ObjectStublessClient11
@ cdecl -norelay ObjectStublessClient12() rpcrt4.ObjectStublessClient12
@ cdecl -norelay ObjectStublessClient13() rpcrt4.ObjectStublessClient13
@ cdecl -norelay ObjectStublessClient14() rpcrt4.ObjectStublessClient14
@ cdecl -norelay ObjectStublessClient15() rpcrt4.ObjectStublessClient15
@ cdecl -norelay ObjectStublessClient16() rpcrt4.ObjectStublessClient16
@ cdecl -norelay ObjectStublessClient17() rpcrt4.ObjectStublessClient17
@ cdecl -norelay ObjectStublessClient18() rpcrt4.ObjectStublessClient18
@ cdecl -norelay ObjectStublessClient19() rpcrt4.ObjectStublessClient19
@ cdecl -norelay ObjectStublessClient20() rpcrt4.ObjectStublessClient20
@ cdecl -norelay ObjectStublessClient21() rpcrt4.ObjectStublessClient21
@ cdecl -norelay ObjectStublessClient22() rpcrt4.ObjectStublessClient22
@ cdecl -norelay ObjectStublessClient23() rpcrt4.ObjectStublessClient23
@ cdecl -norelay ObjectStublessClient24() rpcrt4.ObjectStublessClient24
@ cdecl -norelay ObjectStublessClient25() rpcrt4.ObjectStublessClient25
@ cdecl -norelay ObjectStublessClient26() rpcrt4.ObjectStublessClient26
@ cdecl -norelay ObjectStublessClient27() rpcrt4.ObjectStublessClient27
@ cdecl -norelay ObjectStublessClient28() rpcrt4.ObjectStublessClient28
@ cdecl -norelay ObjectStublessClient29() rpcrt4.ObjectStublessClient29
@ cdecl -norelay ObjectStublessClient30() rpcrt4.ObjectStublessClient30
@ cdecl -norelay ObjectStublessClient31() rpcrt4.ObjectStublessClient31
@ cdecl -norelay ObjectStublessClient32() rpcrt4.ObjectStublessClient32
@ cdecl -norelay NdrProxyForwardingFunction3() rpcrt4.NdrProxyForwardingFunction3
@ cdecl -norelay NdrProxyForwardingFunction4() rpcrt4.NdrProxyForwardingFunction4
@ cdecl -norelay NdrProxyForwardingFunction5() rpcrt4.NdrProxyForwardingFunction5
@ cdecl -norelay NdrProxyForwardingFunction6() rpcrt4.NdrProxyForwardingFunction6
@ cdecl -norelay NdrProxyForwardingFunction7() rpcrt4.NdrProxyForwardingFunction7
@ cdecl -norelay NdrProxyForwardingFunction8() rpcrt4.NdrProxyForwardingFunction8
@ cdecl -norelay NdrProxyForwardingFunction9() rpcrt4.NdrProxyForwardingFunction9
@ cdecl -norelay NdrProxyForwardingFunction10() rpcrt4.NdrProxyForwardingFunction10
@ cdecl -norelay NdrProxyForwardingFunction11() rpcrt4.NdrProxyForwardingFunction11
@ cdecl -norelay NdrProxyForwardingFunction12() rpcrt4.NdrProxyForwardingFunction12
@ cdecl -norelay NdrProxyForwardingFunction13() rpcrt4.NdrProxyForwardingFunction13
@ cdecl -norelay NdrProxyForwardingFunction14() rpcrt4.NdrProxyForwardingFunction14
@ cdecl -norelay NdrProxyForwardingFunction15() rpcrt4.NdrProxyForwardingFunction15
@ cdecl -norelay NdrProxyForwardingFunction16() rpcrt4.NdrProxyForwardingFunction16
@ cdecl -norelay NdrProxyForwardingFunction17() rpcrt4.NdrProxyForwardingFunction17
@ cdecl -norelay NdrProxyForwardingFunction18() rpcrt4.NdrProxyForwardingFunction18
@ cdecl -norelay NdrProxyForwardingFunction19() rpcrt4.NdrProxyForwardingFunction19
@ cdecl -norelay NdrProxyForwardingFunction20() rpcrt4.NdrProxyForwardingFunction20
@ cdecl -norelay NdrProxyForwardingFunction21() rpcrt4.NdrProxyForwardingFunction21
@ cdecl -norelay NdrProxyForwardingFunction22() rpcrt4.NdrProxyForwardingFunction22
@ cdecl -norelay NdrProxyForwardingFunction23() rpcrt4.NdrProxyForwardingFunction23
@ cdecl -norelay NdrProxyForwardingFunction24() rpcrt4.NdrProxyForwardingFunction24
@ cdecl -norelay NdrProxyForwardingFunction25() rpcrt4.NdrProxyForwardingFunction25
@ cdecl -norelay NdrProxyForwardingFunction26() rpcrt4.NdrProxyForwardingFunction26
@ cdecl -norelay NdrProxyForwardingFunction27() rpcrt4.NdrProxyForwardingFunction27
@ cdecl -norelay NdrProxyForwardingFunction28() rpcrt4.NdrProxyForwardingFunction28
@ cdecl -norelay NdrProxyForwardingFunction29() rpcrt4.NdrProxyForwardingFunction29
@ cdecl -norelay NdrProxyForwardingFunction30() rpcrt4.NdrProxyForwardingFunction30
@ cdecl -norelay NdrProxyForwardingFunction31() rpcrt4.NdrProxyForwardingFunction31
@ cdecl -norelay NdrProxyForwardingFunction32() rpcrt4.NdrProxyForwardingFunction32
@ stub NdrOleInitializeExtension
@ stdcall RoFailFastWithErrorContextInternal2(long long ptr)
@ stub RoFailFastWithErrorContextInternal
@ stub UpdateProcessTracing
@ stdcall CLIPFORMAT_UserFree(ptr ptr)
@ stdcall CLIPFORMAT_UserMarshal(ptr ptr ptr)
@ stdcall CLIPFORMAT_UserSize(ptr long ptr)
@ stdcall CLIPFORMAT_UserUnmarshal(ptr ptr ptr)
@ stub CLSIDFromOle1Class
@ stdcall CLSIDFromProgID(wstr ptr)
@ stdcall CLSIDFromProgIDEx(wstr ptr)
@ stdcall CLSIDFromString(wstr ptr)
@ stub CleanupOleStateInAllTls
@ stdcall CleanupTlsOleState(ptr)
@ stub ClearCleanupFlag
@ stdcall CoAddRefServerProcess()
@ stub CoAllowUnmarshalerCLSID
@ stub CoCancelCall
@ stdcall CoCopyProxy(ptr ptr)
@ stdcall CoCreateErrorInfo(ptr) CreateErrorInfo
@ stdcall CoCreateFreeThreadedMarshaler(ptr ptr)
@ stdcall CoCreateGuid(ptr)
@ stdcall CoCreateInstance(ptr ptr long ptr ptr)
@ stdcall CoCreateInstanceEx(ptr ptr long ptr long ptr)
@ stdcall CoCreateInstanceFromApp(ptr ptr long ptr long ptr)
@ stub CoCreateObjectInContext
@ stub CoDeactivateObject
@ stdcall CoDecodeProxy(long int64 ptr)
@ stdcall CoDecrementMTAUsage(ptr)
@ stdcall CoDisableCallCancellation(ptr)
@ stub CoDisconnectContext
@ stdcall CoDisconnectObject(ptr long)
@ stdcall CoEnableCallCancellation(ptr)
@ stdcall CoFileTimeNow(ptr)
@ stdcall CoFreeUnusedLibraries()
@ stdcall CoFreeUnusedLibrariesEx(long long)
@ stdcall CoGetActivationState(int128 long ptr)
@ stub CoGetApartmentID
@ stdcall CoGetApartmentType(ptr ptr)
@ stdcall CoGetCallContext(ptr ptr)
@ stdcall CoGetCallState(long ptr)
@ stdcall CoGetCallerTID(ptr)
@ stub CoGetCancelObject
@ stdcall CoGetClassObject(ptr long ptr ptr ptr)
@ stub CoGetClassVersion
@ stdcall CoGetContextToken(ptr)
@ stdcall CoGetCurrentLogicalThreadId(ptr)
@ stdcall CoGetCurrentProcess()
@ stdcall CoGetDefaultContext(long ptr ptr)
@ stdcall CoGetErrorInfo(long ptr) GetErrorInfo
@ stdcall CoGetInstanceFromFile(ptr ptr ptr long long wstr long ptr)
@ stdcall CoGetInstanceFromIStorage(ptr ptr ptr long ptr long ptr)
@ stdcall CoGetInterfaceAndReleaseStream(ptr ptr ptr)
@ stdcall CoGetMalloc(long ptr)
@ stdcall CoGetMarshalSizeMax(ptr ptr ptr long ptr long)
@ stub CoGetModuleType
@ stdcall CoGetObjectContext(ptr ptr)
@ stdcall CoGetPSClsid(ptr ptr)
@ stub CoGetProcessIdentifier
@ stdcall CoGetStandardMarshal(ptr ptr long ptr long ptr)
@ stub CoGetStdMarshalEx
@ stub CoGetSystemSecurityPermissions
@ stdcall CoGetTreatAsClass(ptr ptr)
@ stdcall CoImpersonateClient()
@ stdcall CoIncrementMTAUsage(ptr)
@ stdcall CoInitializeEx(ptr long)
@ stdcall CoInitializeSecurity(ptr long ptr ptr long long ptr long ptr)
@ stdcall CoInitializeWOW(long long)
@ stub CoInvalidateRemoteMachineBindings
@ stdcall CoIsHandlerConnected(ptr)
@ stdcall CoIsOle1Class(ptr)
@ stdcall CoLockObjectExternal(ptr long long)
@ stdcall CoMarshalHresult(ptr long)
@ stdcall CoMarshalInterThreadInterfaceInStream(ptr ptr ptr)
@ stdcall CoMarshalInterface(ptr ptr ptr long ptr long)
@ stub CoPopServiceDomain
@ stub CoPushServiceDomain
@ stub CoQueryAuthenticationServices
@ stdcall CoQueryClientBlanket(ptr ptr ptr ptr ptr ptr ptr)
@ stdcall CoQueryProxyBlanket(ptr ptr ptr ptr ptr ptr ptr ptr)
@ stub CoReactivateObject
@ stdcall CoRegisterActivationFilter(ptr)
@ stdcall CoRegisterChannelHook(ptr ptr)
@ stdcall CoRegisterClassObject(ptr ptr long long ptr)
@ stdcall CoRegisterInitializeSpy(ptr ptr)
@ stdcall CoRegisterMallocSpy(ptr)
@ stdcall CoRegisterMessageFilter(ptr ptr)
@ stdcall CoRegisterPSClsid(ptr ptr)
@ stdcall CoRegisterSurrogate(ptr)
@ stdcall CoRegisterSurrogateEx(ptr ptr)
@ stdcall CoReleaseMarshalData(ptr)
@ stdcall CoReleaseServerProcess()
@ stdcall CoResumeClassObjects()
@ stub CoRetireServer
@ stdcall CoRevertToSelf()
@ stdcall CoRevokeClassObject(long)
@ stdcall CoRevokeInitializeSpy(int64)
@ stdcall CoRevokeMallocSpy()
@ stub CoSetCancelObject
@ stdcall CoSetErrorInfo(long ptr) SetErrorInfo
@ stdcall CoSetProxyBlanket(ptr long long ptr long long ptr long)
@ stdcall CoSuspendClassObjects()
@ stdcall CoSwitchCallContext(ptr ptr)
@ stdcall CoTaskMemAlloc(long)
@ stdcall CoTaskMemFree(ptr)
@ stdcall CoTaskMemRealloc(ptr long)
@ stub CoTestCancel
@ stdcall CoTreatAsClass(ptr ptr)
@ stdcall CoUninitialize()
@ stub CoUnloadingWOW
@ stdcall CoUnmarshalHresult(ptr ptr)
@ stdcall CoUnmarshalInterface(ptr ptr ptr)
@ stub CoVrfCheckThreadState
@ stub CoVrfGetThreadState
@ stub CoVrfReleaseThreadState
@ stdcall CoWaitForMultipleHandles(long long long ptr ptr)
@ stub CoWaitForMultipleObjects
@ stdcall CreateErrorInfo(ptr)
@ stdcall CreateStreamOnHGlobal(ptr long ptr)
@ stub DcomChannelSetHResult
@ stdcall DllDebugObjectRPCHook(long ptr)
@ stdcall DllGetActivationFactory(ptr ptr)
@ stdcall -private DllGetClassObject(ptr ptr ptr)
@ stub EnableHookObject
@ stdcall FreePropVariantArray(long ptr)
@ stub FreePropVariantArrayWorker
@ stub GetCatalogHelper
@ stdcall GetErrorInfo(long ptr)
@ stub GetFuncDescs
@ stdcall GetHGlobalFromStream(ptr ptr)
@ stub GetHookInterface
@ stdcall GetRestrictedErrorInfo(ptr)
@ stdcall HACCEL_UserFree(ptr ptr)
@ stdcall HACCEL_UserMarshal(ptr ptr ptr)
@ stdcall HACCEL_UserSize(ptr long ptr)
@ stdcall HACCEL_UserUnmarshal(ptr ptr ptr)
@ stdcall HBITMAP_UserFree(ptr ptr)
@ stdcall HBITMAP_UserMarshal(ptr ptr ptr)
@ stdcall HBITMAP_UserSize(ptr long ptr)
@ stdcall HBITMAP_UserUnmarshal(ptr ptr ptr)
@ stdcall HBRUSH_UserFree(ptr ptr)
@ stdcall HBRUSH_UserMarshal(ptr ptr ptr)
@ stdcall HBRUSH_UserSize(ptr long ptr)
@ stdcall HBRUSH_UserUnmarshal(ptr ptr ptr)
@ stdcall HDC_UserFree(ptr ptr)
@ stdcall HDC_UserMarshal(ptr ptr ptr)
@ stdcall HDC_UserSize(ptr long ptr)
@ stdcall HDC_UserUnmarshal(ptr ptr ptr)
@ stdcall HGLOBAL_UserFree(ptr ptr)
@ stdcall HGLOBAL_UserMarshal(ptr ptr ptr)
@ stdcall HGLOBAL_UserSize(ptr long ptr)
@ stdcall HGLOBAL_UserUnmarshal(ptr ptr ptr)
@ stdcall HICON_UserFree(ptr ptr)
@ stdcall HICON_UserMarshal(ptr ptr ptr)
@ stdcall HICON_UserSize(ptr long ptr)
@ stdcall HICON_UserUnmarshal(ptr ptr ptr)
@ stdcall HMENU_UserFree(ptr ptr)
@ stdcall HMENU_UserMarshal(ptr ptr ptr)
@ stdcall HMENU_UserSize(ptr long ptr)
@ stdcall HMENU_UserUnmarshal(ptr ptr ptr)
@ stdcall HPALETTE_UserFree(ptr ptr)
@ stdcall HPALETTE_UserMarshal(ptr ptr ptr)
@ stdcall HPALETTE_UserSize(ptr long ptr)
@ stdcall HPALETTE_UserUnmarshal(ptr ptr ptr)
@ stdcall HSTRING_UserFree(ptr ptr)
@ stub -arch=win64 HSTRING_UserFree64
@ stdcall HSTRING_UserMarshal(ptr ptr ptr)
@ stub -arch=win64 HSTRING_UserMarshal64
@ stdcall HSTRING_UserSize(ptr long ptr)
@ stub -arch=win64 HSTRING_UserSize64
@ stdcall HSTRING_UserUnmarshal(ptr ptr ptr)
@ stub -arch=win64 HSTRING_UserUnmarshal64
@ stdcall HWND_UserFree(ptr ptr)
@ stdcall HWND_UserMarshal(ptr ptr ptr)
@ stdcall HWND_UserSize(ptr long ptr)
@ stdcall HWND_UserUnmarshal(ptr ptr ptr)
@ stub HkOleRegisterObject
@ stdcall IIDFromString(wstr ptr)
@ stub InternalAppInvokeExceptionFilter
@ stub InternalCCFreeUnused
@ stub InternalCCGetClassInformationForDde
@ stub InternalCCGetClassInformationFromKey
@ stub InternalCCSetDdeServerWindow
@ stub InternalCMLSendReceive
@ stub InternalCallAsProxyExceptionFilter
@ stub InternalCallFrameExceptionFilter
@ stub InternalCallerIsAppContainer
@ stub InternalCanMakeOutCall
@ stub InternalCoIsSurrogateProcess
@ stub InternalCoRegisterDisconnectCallback
@ stub InternalCoRegisterSurrogatedObject
@ stdcall InternalCoStdMarshalObject(ptr long ptr ptr)
@ stub InternalCoUnregisterDisconnectCallback
@ stub InternalCompleteObjRef
@ stub InternalCreateCAggId
@ stub InternalCreateIdentityHandler
@ stub InternalDoATClassCreate
@ stub InternalFillLocalOXIDInfo
@ stub InternalFreeObjRef
@ stub InternalGetWindowPropInterface
@ stdcall InternalIrotEnumRunning(ptr)
@ stdcall InternalIrotGetObject(ptr ptr ptr)
@ stdcall InternalIrotGetTimeOfLastChange(ptr ptr)
@ stdcall InternalIrotIsRunning(ptr)
@ stdcall InternalIrotNoteChangeTime(long ptr)
@ stdcall InternalIrotRegister(ptr ptr ptr ptr long ptr ptr)
@ stdcall InternalIrotRevoke(long ptr ptr ptr)
@ stub InternalIsApartmentInitialized
@ stdcall InternalIsProcessInitialized()
@ stub InternalMarshalObjRef
@ stub InternalNotifyDDStartOrStop
@ stub InternalOleModalLoopBlockFn
@ stub InternalRegisterWindowPropInterface
@ stub InternalReleaseMarshalObjRef
@ stub InternalSTAInvoke
@ stub InternalServerExceptionFilter
@ stub InternalSetAptCallCtrlOnTlsIfRequired
@ stub InternalSetOleThunkWowPtr
@ stub InternalStubInvoke
@ stdcall InternalTlsAllocData(ptr)
@ stub InternalUnmarshalObjRef
@ stub IsErrorPropagationEnabled
@ stub NdrExtStubInitialize
@ stub NdrOleDllGetClassObject
@ stub NdrpFindInterface
@ stdcall ProgIDFromCLSID(ptr ptr)
@ stdcall PropVariantClear(ptr)
@ stdcall PropVariantCopy(ptr ptr)
@ stub ReleaseFuncDescs
@ stdcall RoActivateInstance(ptr ptr)
@ stub RoCaptureErrorContext
@ stub RoClearError
@ stdcall RoFailFastWithErrorContext(long)
@ stub RoFreeParameterizedTypeExtra
@ stub RoGetActivatableClassRegistration
@ stdcall RoGetActivationFactory(ptr ptr ptr)
@ stdcall RoGetAgileReference(long ptr ptr ptr)
@ stdcall RoGetApartmentIdentifier(ptr)
@ stdcall RoGetErrorReportingFlags(ptr)
@ stub RoGetMatchingRestrictedErrorInfo
@ stdcall RoGetParameterizedTypeInstanceIID(long ptr ptr ptr ptr)
@ stdcall RoGetServerActivatableClasses(ptr ptr ptr)
@ stdcall RoInitialize(long)
@ stub RoInspectCapturedStackBackTrace
@ stub RoInspectThreadErrorInfo
@ stdcall RoOriginateError(long ptr)
@ stdcall RoOriginateErrorW(long long ptr)
@ stdcall RoOriginateLanguageException(long ptr ptr)
@ stub RoParameterizedTypeExtraGetTypeSignature
@ stdcall RoRegisterActivationFactories(ptr ptr long ptr)
@ stdcall RoRegisterForApartmentShutdown(ptr ptr ptr)
@ stub RoReportCapabilityCheckFailure
@ stub RoReportFailedDelegate
@ stdcall RoReportUnhandledError(ptr)
@ stub RoResolveRestrictedErrorInfoReference
@ stub RoRevokeActivationFactories
@ stdcall RoSetErrorReportingFlags(long)
@ stub RoTransformError
@ stub RoTransformErrorW
@ stdcall RoUninitialize()
@ stdcall RoUnregisterForApartmentShutdown(ptr)
@ stub SetCleanupFlag
@ stdcall SetErrorInfo(long ptr)
@ stdcall SetRestrictedErrorInfo(ptr)
@ stdcall StringFromCLSID(ptr ptr)
@ stdcall StringFromGUID2(ptr ptr long)
@ stdcall StringFromIID(ptr ptr) StringFromCLSID
@ stub UpdateDCOMSettings
@ stdcall WdtpInterfacePointer_UserFree(ptr)
@ stub -arch=win64 WdtpInterfacePointer_UserFree64
@ stdcall WdtpInterfacePointer_UserMarshal(ptr long ptr ptr ptr)
@ stub -arch=win64 WdtpInterfacePointer_UserMarshal64
@ stdcall WdtpInterfacePointer_UserSize(ptr long long ptr ptr)
@ stub -arch=win64 WdtpInterfacePointer_UserSize64
@ stdcall WdtpInterfacePointer_UserUnmarshal(ptr ptr ptr ptr)
@ stub -arch=win64 WdtpInterfacePointer_UserUnmarshal64
@ stdcall WindowsCompareStringOrdinal(ptr ptr ptr)
@ stdcall WindowsConcatString(ptr ptr ptr)
@ stdcall WindowsCreateString(wstr long ptr)
@ stdcall WindowsCreateStringReference(wstr long ptr ptr)
@ stdcall WindowsDeleteString(ptr)
@ stdcall WindowsDeleteStringBuffer(ptr)
@ stdcall WindowsDuplicateString(ptr ptr)
@ stdcall WindowsGetStringLen(ptr)
@ stdcall WindowsGetStringRawBuffer(ptr ptr)
@ stub WindowsInspectString
@ stdcall WindowsIsStringEmpty(ptr)
@ stdcall WindowsPreallocateStringBuffer(long ptr ptr)
@ stdcall WindowsPromoteStringBuffer(ptr ptr)
@ stub WindowsReplaceString
@ stdcall WindowsStringHasEmbeddedNull(ptr ptr)
@ stdcall WindowsSubstring(ptr long ptr)
@ stdcall WindowsSubstringWithSpecifiedLength(ptr long long ptr)
@ stdcall WindowsTrimStringEnd(ptr ptr ptr)
@ stdcall WindowsTrimStringStart(ptr ptr ptr)
