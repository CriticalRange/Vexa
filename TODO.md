# Runtime Crash Isolation TODO

## Immediate: Keep App Alive During FEX Failures
- [ ] Ensure `libFEXCore.so` is never loaded in the UI process.
- [ ] Add a dedicated runtime `Service` with `android:process=":runtime"` as supervisor.
- [ ] Start FEX from a native child runner (`exec`/`posix_spawn`) managed by the runtime service.
- [ ] Add IPC reporting from runtime service to UI with structured failure code and last-log summary.
- [ ] Verify process isolation by logging and comparing runtime PID vs `GameActivity` PID.

## Later: Additional Categories
- [ ] Add new TODO categories here as runtime architecture evolves.
