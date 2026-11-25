#!/usr/bin/env python3
"""K3 ACPI PCC mock endpoint (host-side development harness).

Simulates the ESOS side of the dedicated type-3 PCC management channel so the
resource-service ABI (docs/k3-maintained-efi/pcc-resource-registry.md) and the
AML transaction flow (Power.asl PSND) can be validated before a real ESOS
endpoint exists.

This is NOT the production endpoint. It models:
  - the type-3 PCC shared-memory command/status handshake,
  - idempotent set-state vote semantics with multi-client arbitration,
  - the result/QUERY codes from the registry.

The transport envelope here mirrors Power.asl's DRAFT field offsets; both move
together and are frozen only after the RHEL 6.12 acpi_pcc.c/ACPICA review.
"""

import struct
import sys

# --- ABI (frozen parts: ops, result codes) ----------------------------------
OP_ACQUIRE, OP_RELEASE, OP_QUERY = 1, 2, 3
RES_SUCCESS, RES_INVALID, RES_UNSUPPORTED, RES_DENIED, RES_BUSY, RES_HWFAIL = range(6)

# A small slice of the resource registry for the harness.
KNOWN_RESOURCES = {0x010001, 0x010010, 0x010040}  # USB2, PCIE_PORTA, EMAC0

# --- DRAFT shared-memory layout (matches Power.asl PCCR field offsets) -------
OFF_CMD, OFF_STS, OFF_RID, OFF_ARG, OFF_RES, OFF_VST = 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C
STS_CMD_COMPLETE = 0x1
STS_ERROR = 0x4


class MockEsosEndpoint:
    """Models ESOS: holds per-client votes and the physical state per resource."""

    def __init__(self):
        # votes[resource_id] = set of client ids requiring it ON
        self.votes = {rid: set() for rid in KNOWN_RESOURCES}

    def _physical_on(self, rid):
        return 1 if self.votes[rid] else 0

    def service(self, op, rid, client="acpi"):
        """Idempotent set-state service. Returns (result_code, voted_state)."""
        if rid not in KNOWN_RESOURCES:
            return RES_INVALID, 0
        # All ops return THIS client's vote (vote semantics); physical state may
        # differ if other clients vote and is observable via _physical_on().
        if op == OP_ACQUIRE:
            self.votes[rid].add(client)           # idempotent: set membership
            return RES_SUCCESS, 1
        if op == OP_RELEASE:
            self.votes[rid].discard(client)       # idempotent
            return RES_SUCCESS, 0
        if op == OP_QUERY:
            return RES_SUCCESS, 1 if client in self.votes[rid] else 0
        return RES_UNSUPPORTED, 0

    # --- PCC shared-memory transaction (what the doorbell would trigger) -----
    def handle_doorbell(self, shmem: bytearray, client="acpi"):
        op = struct.unpack_from("<I", shmem, OFF_CMD)[0]
        rid = struct.unpack_from("<I", shmem, OFF_RID)[0]
        res, vst = self.service(op, rid, client)
        struct.pack_into("<I", shmem, OFF_RES, res)
        struct.pack_into("<I", shmem, OFF_VST, vst)
        sts = STS_CMD_COMPLETE | (STS_ERROR if res != RES_SUCCESS else 0)
        struct.pack_into("<I", shmem, OFF_STS, sts)


def _aml_send(shmem, esos, op, rid, client="acpi"):
    """Emulate Power.asl PSND: write request fields, ring doorbell, read result."""
    struct.pack_into("<I", shmem, OFF_CMD, op)
    struct.pack_into("<I", shmem, OFF_RID, rid)
    struct.pack_into("<I", shmem, OFF_STS, 0)
    esos.handle_doorbell(shmem, client)           # completion interrupt analogue
    assert struct.unpack_from("<I", shmem, OFF_STS)[0] & STS_CMD_COMPLETE
    res = struct.unpack_from("<I", shmem, OFF_RES)[0]
    vst = struct.unpack_from("<I", shmem, OFF_VST)[0]
    return res, vst


def _selftest():
    esos = MockEsosEndpoint()
    shmem = bytearray(0x50)
    USB2 = 0x010001

    # QUERY before ACQUIRE -> off
    assert _aml_send(shmem, esos, OP_QUERY, USB2) == (RES_SUCCESS, 0)
    # ACQUIRE -> on
    assert _aml_send(shmem, esos, OP_ACQUIRE, USB2) == (RES_SUCCESS, 1)
    # idempotent ACQUIRE retry -> still on, success
    assert _aml_send(shmem, esos, OP_ACQUIRE, USB2) == (RES_SUCCESS, 1)
    # QUERY -> voted on
    assert _aml_send(shmem, esos, OP_QUERY, USB2) == (RES_SUCCESS, 1)
    # another client's vote keeps it physically on after acpi releases
    esos.service(OP_ACQUIRE, USB2, client="esos_other")
    assert _aml_send(shmem, esos, OP_RELEASE, USB2) == (RES_SUCCESS, 0)  # acpi vote=0
    assert esos._physical_on(USB2) == 1  # still on due to other client
    # invalid resource
    assert _aml_send(shmem, esos, OP_ACQUIRE, 0xDEAD00) == (RES_INVALID, 0)
    print("k3-pcc-mock-endpoint: all self-tests passed")


if __name__ == "__main__":
    sys.exit(0 if _selftest() is None else 0)
