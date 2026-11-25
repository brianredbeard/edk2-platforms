# K3 EFI platform configuration design

Date: 2026-11-18. Status: proposed implementation design; no firmware or hardware
changes made by this document. Scope: Pico-ITX first, with explicit capability
gates for other boards.

Review revision: addresses recovery, mode scope, dependency validation, evidence,
updates, error presentation, performance, build policy, testing and security.

## Goal and selected approach

Expose useful platform preferences, hardware status and quirk decisions through
the existing EFI Platform Manager. Keep one validated effective configuration
for firmware initialization and OS hardware descriptions. The user agreed to
start with read-only information, then persistence/resolution, one qualified USB
speed setting, and diagnostic quirk overrides.

The selected approach adds a native HII page and a shared configuration resolver.
A separate setup application would duplicate existing UI infrastructure. Direct
editing of PCD values or `_DSD` properties from UI callbacks would bypass ownership,
dependencies and boot ordering. Neither alternative is selected.

This builds on [the peripheral binding profile](acpi-peripheral-binding-profile.md)
and [the platform-management architecture](acpi-platform-management-architecture.md).
Hardware facts remain firmware-owned. Functional bring-up precedes runtime power
management. No new `_DSD` UUID, runtime hardware control interface, or arbitrary
register editor is introduced.

Mode scope is explicit: stage 1 read-only information supports both ACPI and DT
builds. Stages 2–4 writable settings target ACPI builds only in this iteration.
DT builds ignore saved requests, report “Settings not applied in DT mode”, preserve
the variable without rewriting it, and use existing board defaults. They expose
no save action. DT regression remains required. Extending writable configuration
to DT needs a separate design for equivalent description and initialization paths.

## Existing integration and gaps

Paths are relative to `edk2-platforms/`.

- `Platform/Spacemit/K3/MUSE-Pico/MUSE-Pico.dsc` includes UiApp with
  `Silicon/Spacemit/Library/PlatformUiLib/PlatformManagerUiLib.inf`. F2 opens
  the existing boot-menu path. HII database and the vendor setup browser are
  included through `Silicon/Spacemit/Spacemit.dsc.inc` and its FV include.
- `PlatformUiLib/PlatformManager.c` discovers registered formsets and creates
  links for `gPlatformManagerFormsetGuid`. Its class test is correct:
  `CompareGuid()` returns BOOLEAN, so testing equality to zero skips nonmatches.
  Register the new page with that class; no filter reversal is needed.
- `SkuSelectDxe` selects dynamic PCD SKU defaults from the internal FDT model,
  including fallback to DEFAULT. It does not establish verified board identity
  or silicon revision. Unknown identity must remain visibly unknown.
- `K3.dec` defines activation masks for GMAC, USB and PCIe and a USB HS-only
  mask. These are inputs, not proof that each controller initialized successfully.
  USB mask `0xFF` has legacy sentinel semantics; resolve controller defaults before
  presenting a list of enabled ports or applying a user constraint.
- `K3AcpiNvsDxe` copies PCDs to NVS at ReadyToBoot. `Platform.asl` has mask-based
  `_STA` helpers, but the reduced active `Dsdt.asl` does not include that machinery.
  NVS allocation alone does not establish OS-visible configuration.
- The Pico DSC selects non-emulated variables. Confirm actual firmware persistence
  and recovery on hardware before promising saved settings survive power loss.

## User interface

Add one formset titled **Platform Configuration**, linked from Platform Manager.
Use standard text HII forms, compatible with serial and graphical consoles.
Do not require native DRM. Show board-facing names when a connector mapping is
verified; otherwise use controller identifiers rather than inventing port labels.

Sections:

1. **System information:** firmware version, selected SKU, identity source and
   verification status, hardware-description mode, configuration revision.
2. **Peripherals:** configured availability, initialization result if supplied
   by the owner, USB speed ceiling, and known retained clock rates with provenance.
3. **Hardware workarounds:** requested policy, resolved decision, delivery status,
   reason, affected hardware and owner. Kernel application is “Not confirmed”
   unless an actual feedback mechanism exists.
4. **Pending changes:** saved values differing from this boot, next-boot effect,
   validation errors and restore-defaults action once persistence is implemented.

Example wording:

```text
USB suspend workaround
  Policy:           Automatic
  Decision:         Enabled
  Delivery:         ACPI property published
  OS application:   Not confirmed
  Reason:           Platform firmware default; revision rule not yet qualified
```

“Configured”, “applied by firmware”, “published to OS”, and “verified on hardware”
are distinct states. The page must not turn PCD defaults into claims of operational
hardware, or infer clock measurements from a constant in experimental AML.

Error presentation is persistent in the page, with a concise serial diagnostic:

| Condition | Message and behavior |
|---|---|
| Invalid record | “Saved settings rejected: invalid size/value. Platform defaults active.” Preserve original; allow an explicit replacement/defaults save |
| Newer unsupported version | “Settings require newer firmware. Platform defaults active.” Never silently downgrade or rewrite |
| Unsupported request | Name the option and reason; retain edits in browser, reject the entire save |
| Variable write failure | “Settings were not saved: <EFI status>.” Saved/pending indicators remain unchanged; browser edits may be retried |
| Missing configuration evidence | “Unknown” or “Not confirmed”; disable affected controls rather than guess |
| Safe resolution/application failure | Show the owner's failure; do not claim configuration ready or continue affected initialization with partially applied settings |
| Recovery image active | “Recovery mode: saved settings ignored.” Offer only explicit reset of this configuration variable |

The failure path must preserve a setup/recovery console. Invalid user input falls
back to complete platform defaults; an internal failure to apply those defaults
is a firmware error and prevents the affected boot path, not a partial success.

## Model and ownership

Maintain three separate records:

- **Capabilities:** board identity and confidence, silicon revision when known,
  physically present controllers, supported options, mandatory workarounds,
  dependencies and provenance. Not writable through setup.
- **Requested configuration:** persisted user preferences and allowed overrides.
  Contains no raw MMIO addresses, clock divisors, GPIO assignments or revision IDs.
- **Effective configuration:** validated decisions and reasons frozen for the boot,
  plus owner-reported application/publication status. Status may be updated;
  resolved hardware decisions cannot change after initialization begins.

Initially publish a versioned boot-services protocol exposing a read-only snapshot
and enumeration of quirk/status entries. Do not expose raw pointers to mutable
PCD storage. The precise C layout and GUID allocation belong in the implementation
plan; use explicit size/revision fields and stable identifiers, not display strings
as keys. Keep the resolver independent of HII so host tests can exercise it.

Publish the completed decision record once, before signaling readiness. Consumers
read copied snapshots; they cannot mutate decisions. Owner status updates are
serialized at a documented TPL and snapshots carry a generation counter. No
allocation, HII call or variable I/O occurs while holding the short status lock.
UI saves are serialized and update only the pending record. DXE callback execution
and protocol notification can interleave: do not assume “single-threaded boot”
eliminates reentrancy. The implementation plan specifies API TPL requirements and
tests update/read interleavings and notification during publication.

### Quirk registry

Each entry defines stable internal ID, label/help, hardware applicability,
evidence source, owner, automatic decision rule, override permissions, dependencies,
delivery mechanism and reset requirement. These IDs are project configuration IDs,
not new standard ACPI property names.

Allowed policy values are Automatic, Force enabled and Force disabled. Offer only
those accepted for that entry. A mandatory erratum workaround is read-only in
normal firmware. Diagnostic builds may offer qualified overrides only when an
independent recovery route exists. The resolver enforces this regardless of UI.

“Diagnostic build” means a dedicated build-time feature flag, provisionally
`K3_CONFIG_DIAGNOSTIC_OVERRIDES`, default FALSE, recorded in the image manifest
and displayed in setup. It is independent of SKU and DEBUG/RELEASE compiler mode.
Normal builds reject forced quirk requests even if left in a variable by a
diagnostic build. Merely hiding controls is insufficient. The recovery-image flag
is separate and overrides all saved requests, including diagnostic ones.

Unknown silicon revision does not match a guessed revision rule. Preserve the
current qualified platform default where one exists, labeling its provenance;
otherwise report Unknown/Unqualified and expose no override. A quirk missing an
OS delivery mechanism is informational, not a writable setting.

Initial candidates:

| Quirk family | Owner/delivery | Initial treatment |
|---|---|---|
| DWC3 suspend and PHY quirks | OS via existing `_DSD` properties | Display current source defaults; writable only after consumer and publication validation |
| UART fixed-XScale behavior | Kernel match/quirk path | Read-only; no invented `_DSD` control |
| PCIe ghost-device filtering | Kernel MCFG quirk | Read-only; kernel application unconfirmed from EFI |
| GMAC retained clocks | Firmware initialization/handoff | Required platform contract, not a user-disableable workaround |
| PCIe bus-zero ATU window | Firmware PCIe initialization | Required when that root is enabled; not an independent preference |

### Saved configuration

Use one vendor-GUID variable, provisionally named `K3PlatformConfig`, for the
requested configuration. Use NV and boot-service access initially; runtime access
and OS-side editing are out of scope. Select a controlled ConfigAccess-backed HII
buffer store so validation and persistence use the same path; do not rely on UI
constraints to validate arbitrary variable contents.

Define a bounded versioned serialization with explicit field widths, length and
reserved-field rules. Do not serialize compiler padding or an array indexed by
changeable registry order. Validate variable attributes, size, version, board
applicability, enums, IDs and dependencies at every boot and save. Missing data
means Automatic/defaults. Invalid or unsupported data selects safe platform defaults
with a visible reason; preserve the rejected variable for diagnosis rather than
silently overwriting it. No automatic writes just because setup was opened.

A save validates the complete pending record and uses one SetVariable operation.
Report write failure and keep current effective state unchanged. Restore defaults
saves the all-Automatic request; it does not clear unrelated firmware variables.
Define explicit migration for known older versions and test downgrade handling.

### Firmware update and downgrade policy

Updates preserve the variable store unless an explicitly documented recovery
operation requests otherwise. Preserve identity/key provisioning independently;
never clear the whole store to reset platform preferences. The new firmware reads
the old record, applies only a registered version migration in memory, and
revalidates every result against its current capabilities and permissions.
Persist migrated format only on an explicit user save. Unknown versions and
unmigratable records remain untouched and select defaults with the message above.
Downgrading firmware therefore remains recoverable without requiring an older
image to parse new fields. Removed options are not silently reused for new meanings.

Qualification includes the actual update tool/capsule path and its flash-region
policy, not just resolver unit tests. Test old-to-new, new-to-old, diagnostic-to-
normal, board mismatch and interrupted update recovery with preserved settings.
This design does not assert that the current updater already preserves the store.

## Boot ordering and application

For writable configuration, resolve after trustworthy board capability selection
and variable availability, before the first affected driver or library initializes
hardware. Publish a new configuration-ready protocol only after validation and
PCD projections complete. Consumers depend on that protocol, rather than assuming
SKU-select-done or ReadyToBoot implies user settings were applied.

Inventory library constructors and early consumers as well as driver entry points;
otherwise a DXE dependency change can leave an earlier initialization path behind.
Do not introduce a dependency cycle through a storage driver needed to read variables.
The phase-one read-only observer adds no new dependencies to existing device drivers.

Before stage 2 is wired, produce a reviewed dependency inventory from selected
DSC/FDF modules, resolved INF DEPEX, library constructors, protocol production
sites and Driver Binding Start paths. Include variable-read/write availability,
SKU/capability production and all first PCD consumers. Source inspection identifies
FlashFvbDxe, QspiDxe, SpiNorFlashDxe and VariableRuntimeDxe as paths to audit; verify
their actual protocol edges rather than infer ordering from their FV listing.
Check the resulting graph for cycles and retain a topological order as evidence;
DEPEX alone cannot reveal all runtime LocateProtocol/constructor dependencies.

The variable-storage and recovery-console paths must not wait on configuration
that itself needs those paths. Keep them outside user control in this iteration.
Instrument dispatch/configuration-ready/first-consumer events in diagnostic builds
and check ordering in a boot log. A first consumer encountering missing readiness
must fail/defer according to its entry mechanism, never continue with guessed
PCDs. Exercise delayed or absent producers in integration fixtures. No new
readiness dependency is accepted without both the graph audit and execution trace.

Resolve availability by intersecting actual board capabilities with permitted user
requests and dependency rules. A user cannot enable unwired hardware. Required
management devices and a recovery console remain available. Shared clock ownership
does not change when a controller is hidden. Disabling an ACPI node alone does not
disable DMA or make hardware safe.

Firmware drivers, clock handoff, NVS and table construction consume the same frozen
decisions. Reconcile `_STA`, `_CRS` and applicable `_DSD` values before OS handoff.
For USB speed limiting, both the firmware controller limit and OS `maximum-speed`
must agree. DT-mode writes are outside this iteration, as specified above. A
build-time ACPI/DT selection is read-only; no runtime mode selector is implied.

Changes saved in setup apply on the next boot, even if the user chooses Continue
instead of resetting. Current ACPI and initialized hardware retain this boot's
configuration. Mark questions reset-required and provide an explicit save-and-reset
flow only once reset behavior is qualified. Never reset as a side effect of browsing.

## Delivery stages and acceptance

### 1. Read-only foundation — first implementation scope

Create `Silicon/Spacemit/K3/Drivers/K3PlatformConfigDxe/` for the observational
snapshot/protocol and `Silicon/Spacemit/K3/Drivers/K3PlatformConfigHiiDxe/` for forms
and presentation. Add the protocol declaration under `Silicon/Spacemit/K3/Include/`
and GUID/build entries to K3.dec, MUSE-Pico.dsc and MUSE-Pico.fdf. The initial
snapshot is populated after SKU selection; no PCD writes, setup variables or
controller initialization changes are permitted in this stage.

Use an explicit registry of known entries. Do not read arbitrary MMIO to populate
the screen. Show unknown/unconfirmed fields honestly; do not parse remote kernel
logs or assume a source DSDT is the loaded one. Publication status stays unknown
until the firmware table owner supplies evidence.

Register the formset with the platform-manager class and a unique formset identity.
Preserve the existing discovery logic and confirm the new page appears exactly
once. Do not change the manager's own standard platform-setup class or broadly
refactor the setup browser. Any independently demonstrated IFR parsing defect is
a separate fix with a focused malformed-package regression fixture.

Acceptance: successful firmware build; verify the compiled formset identity/class
and snapshot unknown/sentinel cases; page reachable once under Platform Manager; serial-console
navigation and return work; opening/closing setup produces no variable changes.
Hardware navigation is deferred until an authorized reboot/test window.

### 2. Resolver and persistence

Add the pure resolver, validated variable storage and current/pending UI. Test
missing/truncated/oversized/unknown-version variables, unknown boards, unsupported
overrides, mandatory quirks, contradictory requests, defaults and failed writes.
Wire configuration-ready dependencies before enabling a writable hardware setting.
Prove persistence and recovery in an authorized hardware test window.

### 3. One USB speed setting

Select a physically identified non-recovery port with verified high/super-speed
wiring and an existing firmware/OS speed-limit consumer. If no such port is
qualified, keep this stage blocked; USB2-only hardware cannot demonstrate a
SuperSpeed restriction. Options are Automatic and High-speed only. Show the
board-specific ceiling and reset requirement. Verify pending/current separation,
both descriptions, actual negotiated speed, defaults restoration and core boot
regressions. No arbitrary connector index is selected by this design.

### 4. Diagnostic quirk overrides

Choose one existing DWC3 property with a verified consumer and independent recovery
access. Test automatic/forced decisions and property omission/encoding; absence
must not be mistaken for force-disabled if the driver enables that quirk internally.
A force-off control is unavailable unless its semantics can actually be delivered.
Other kernel-owned quirks remain read-only pending a separately reviewed interface.

## Evidence and qualification

The implementing engineer collects reproducible evidence; a reviewer checks source
traces and manifests; the platform maintainer accepts board-specific qualification.
Firmware source inspection alone is not a hardware qualification. Use these terms:

| Term | Required evidence |
|---|---|
| Verified consumer | Exact kernel/firmware revision and function, parsed type/default, call path and isolated valid/invalid input tests |
| Verified wiring | Board/revision-specific schematic or vendor record tied to connector/controller mapping; discrepancies block enablement |
| Published | Loaded/installed table or owner report proves the resolved value was delivered in that boot |
| Confirmed operation | Timestamped operation/interrupt/negotiated-speed result on the identified hardware and image |
| Qualified setting | Verified wiring and consumer, confirmed operation, recovery, cold boot, defaults restoration and core regression results reviewed together |

Store per-stage manifests and results under the existing qualification artifact
layout referenced by [validation.md](validation.md), with board ID, source/build
hashes, variable schema, AML hashes, commands, logs and explicit pass/fail/unrun.
Do not inherit qualification across a changed board revision or consumer without
an impact review. Unknown source or missing evidence remains Unknown/Unqualified.

## Test strategy and boot cost

| Layer | Coverage |
|---|---|
| Host tests | Parser bounds/versions, resolver/defaults/dependencies, quirk permissions, migration, snapshot interleavings and failed saves |
| Build/integration | Both mode builds; normal/diagnostic/recovery flags; IFR identity/class; protocol ordering; actual PCD-to-description projection |
| Hardware stage 1 | Serial HII navigation, correct provenance/unknown status, no variable writes, existing boot regressions |
| Hardware stages 2–4 | Persistence/power loss, update/downgrade, recovery before initialization, pending/current separation, cold boot, actual USB/quirk effect, DT unchanged |

Every result distinguishes tested from deferred. Build checks do not substitute
for hardware checks. Reserve destructive/interruption tests for a recovery-ready
test window; no such test is authorized by this documentation change.

Resolution runs once per boot from one bounded variable read; no bus scan, network
lookup, sleep, retry loop or per-consumer variable read is permitted. Stage 1 does
not read the new variable. Build HII strings lazily when the page is opened.
Record resolver duration and total boot-time delta over repeated baseline and
candidate boots using the existing performance/timer facilities. Expected cost
is small, but unmeasured; investigate material regressions before qualification.
The serialized format's maximum size and registry count are fixed in the plan.

## Security assumptions

The variable is untrusted input even with NV/boot-service-only attributes. Those
attributes restrict runtime access; they do not authenticate a preboot writer or
protect against physical flash modification. Repeat semantic validation and
permission checks outside HII. Never let saved values expand physical capabilities,
disable mandatory errata, change Secure Boot policy or alter key provisioning.

This iteration assumes trusted firmware/preboot execution and controlled physical
access. It does not introduce authenticated-variable signatures or claim resistance
to a malicious privileged preboot component. Authenticated administrative settings
would require a separate enrollment/key/recovery design. A CRC would detect damage,
not establish trust. Follow existing variable-check/locking facilities where
compatible with setup saves; verify actual enforcement rather than assume it.

## Validation boundaries and recovery decision

No flash, reboot, boot-variable edit or live hardware manipulation is authorized
by preparing this design. Build outputs stay isolated from deployed firmware.
Hardware validation requires a preserved known-good image and a recovery path
that bypasses the saved configuration before affected hardware initialization.
For diagnostic overrides, define and test that bypass before enabling the control;
late entry to F2 is not sufficient if a bad setting prevents reaching BDS.

Recovery candidates and disposition:

| Mechanism | Feasibility/constraints | Decision |
|---|---|---|
| Physical download mode plus FDL/RAM agent and known-good EFI image | Existing Pico flashing/recovery evidence in validation.md; requires host, cable, physical access and preserved flash layout | Selected engineering recovery path |
| Dedicated GPIO bypass | Requires a confirmed accessible pin, early pinmux/read path and no conflict with controlled hardware; none qualified here | Not selected |
| Early serial command | Requires always-available UART and an input hook before affected initialization; current F2 handling is too late | Possible future convenience, not recovery guarantee |
| Timeout/watchdog rollback | Needs independent watchdog/reset, durable attempt accounting and reliable success signal; elapsed time alone cannot identify failure | Not selected |

The selected path uses a separately built recovery EFI image with a build-time
`K3_CONFIG_IGNORE_SAVED` flag. It never applies the configuration variable, including
when returning from setup, and uses the pre-configuration known-good hardware
defaults. The resolver checks that flag before reading requests. The image can
offer explicit deletion/reset of only `K3PlatformConfig`, followed by restoring
the normal image. This cannot depend on parsing a corrupt configuration record.

The existing FDL/RAM-agent history establishes a feasible route, not qualification
of this new recovery image. Before any writable hardware control is enabled,
demonstrate entering physical download mode without reaching EFI/BDS, loading or
flashing the recovery image using the documented board-specific route, preserving
unrelated NOR regions/variables, recovering a configuration that prevents normal
boot, and restoring the normal image. Retain image/readback hashes and serial logs.
If physical entry or this test fails, stages 3–4 remain disabled; stage 1 is unaffected.
Do not substitute blanket variable-store erasure. This is an engineering-service
recovery model; unattended field configuration requires a later recovery design.
No automatic rollback or watchdog recovery is claimed.
