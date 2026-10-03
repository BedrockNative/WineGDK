@ stdcall -private DllCanUnloadNow()
@ stdcall -private DllGetActivationFactory(ptr ptr)
@ stdcall -private DllGetClassObject(ptr ptr ptr)
@ stdcall -private DllRegisterServer()
@ stdcall -private DllUnregisterServer()
@ stub CreateControlInput
@ stub CreateControlInputEx

@ stdcall __wine_create_core_window(ptr)
@ stdcall __wine_destroy_core_window(ptr)

@ stdcall __wine_core_window_set_fullscreen(ptr long)
@ stdcall __wine_core_window_get_fullscreen(ptr ptr)
