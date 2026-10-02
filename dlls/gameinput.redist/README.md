# GameInput redistributable runtime

Runtime version 2.2.26100.6106, extracted from the user-supplied
`GameInputRedist.msi` (product 10.1.26100.6106). These Microsoft binaries
retain their original licensing; they are not covered by Wine's LGPL.
The MSI itself is not bundled or executed during installation or prefix setup.

`make install` installs the extracted files under `share/wine/native/gameinput.redist`.
On x64 prefix creation/update, `loader/gameinput.inf.in` installs the x64 runtime,
bridge, input proxy and service under `Program Files/Microsoft GameInput/x64`,
and the x86 client under `Program Files/Microsoft GameInput/x86`.
The redist DLLs are also available in system32/syswow64. Setup imports all
3,902 runtime registry values from the MSI (device mappings, COM registration
and the 32-bit RedistDir), expanding MSI paths to prefix paths, and registers
GameInputRedistService as LocalSystem with demand start.

The registry was extracted with `msiinfo export GameInputRedist.msi Registry`,
with the GameInput3Main_RegKey and GameInput3RedistDir_RegKey components selected;
files were extracted with `msiextract`. Installer dependency/catalog entries
are omitted because this is not an MSI installation. The service-config helper
is not run: its relevant file/registry setup is expressed directly in the INF.

## Compatibility limitation

Wine's builtin `gameinput.dll` remains the default. The native service hits
an unimplemented `ntdll.NtQueryWnfStateData`; therefore setup does not perform
the MSI's immediate service start or restart GameInputSvc. Applications that
explicitly use native GameInputRedist can still fail. This bundle prepares the
runtime but does not implement the missing native-service APIs.

`MANIFEST.json` records the original MSI, binary and generated INF checksums.
