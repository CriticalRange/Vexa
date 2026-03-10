## Runtime TODO: FEX Bring-up Sequence

- [x] First priority: Compile VEXA runtime wrapper directly against FEX headers/libs
  (`target_include_directories` + `target_link_libraries`) and avoid string-based `dlsym` calls
  for mangled C++ symbols.
- [ ] Integrate `FEXCore::Config::Initialize()` in runtime startup and log success/failure as
  `FEX` category.
- [ ] Integrate `FEXCore::Config::Load()` after config layering, with explicit error reporting if
  config paths are invalid.
- [ ] Integrate `FEX::FetchHostFeatures()` and log key host feature flags used for runtime decisions
  (for example AVX and cache/atomic related flags).
- [ ] Create FEX context with
  `FEXCore::Context::Context::CreateNewContext(HostFeatures)` and fail fast if context creation
  returns null.
- [ ] Wire and set signal delegator (`CTX->SetSignalDelegator(...)`) and verify delegated signal
  path logging before guest execution.
- [ ] Wire and set syscall handler (`CTX->SetSyscallHandler(...)`) with structured logs for handler
  creation and attachment.
- [ ] Call `CTX->InitCore()` and treat non-success as a hard startup failure with dedicated failure
  code and diagnostics.
- [ ] Add initial thread lifecycle bring-up (`CreateThread` + `ExecuteThread`) behind preflight
  success, with guarded logging around enter/exit and crash boundaries.

## Later: Additional Categories

- [ ] Add new TODO categories here as runtime architecture evolves.
