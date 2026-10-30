#!/usr/bin/env python3
"""Static pre-push checks for the zmk-spacemouse module.

There is no local Zephyr toolchain, so GitHub Actions is the only compiler. This catches the
mistakes that would otherwise cost a CI round: missing files, Kconfig symbols used but never
defined, binding/compatible/DT_DRV_COMPAT drift, header constants that disagree, and a HID
report map whose report sizes don't match the C structs.

Usage: python tools/check-module.py [module_root]   (exit 0 = all checks passed)
"""
import re
import sys
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent)
COMPAT = "zmk,input-processor-spacemouse"
REQUIRED = [
    "zephyr/module.yml", "CMakeLists.txt", "Kconfig", "LICENSE", "README.md",
    "include/zmk/spacemouse/hid.h", "include/zmk/spacemouse/engine.h",
    "include/zmk/spacemouse/spacemouse.h", "include/dt-bindings/zmk/spacemouse.h",
    "src/sm_engine.c", "src/spacemouse.c", "src/usb_hid.c", "src/hog.c",
    "src/input_processor_spacemouse.c",
    "dts/bindings/input_processors/zmk,input-processor-spacemouse.yaml",
    "dts/input/processors/spacemouse.dtsi",
]
# Symbols this module may use that ZMK or Zephyr define.
EXTERNAL_SYMBOLS = {
    "ZMK_USB", "ZMK_BLE", "ZMK_POINTING", "ZMK_SPLIT", "ZMK_SPLIT_ROLE_CENTRAL", "ZMK_LOG_LEVEL",
    "ZMK_USB_HID_INIT_PRIORITY", "ZMK_BLE_INIT_PRIORITY", "ZMK_BLE_THREAD_STACK_SIZE",
    "ZMK_BLE_THREAD_PRIORITY", "USB_HID_DEVICE_COUNT", "KERNEL_INIT_PRIORITY_DEFAULT",
    "DT_HAS_ZMK_INPUT_PROCESSOR_SPACEMOUSE_ENABLED",
}

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
    return cond


def read(rel):
    p = ROOT / rel
    return p.read_text(encoding="utf-8") if p.is_file() else ""


def kconfig_defined():
    return set(re.findall(r"^\s*(?:menu)?config\s+(\w+)", read("Kconfig"), re.M))


def used_symbols(text):
    return set(re.findall(r"CONFIG_(\w+)", text))


def hid_report_bits(desc):
    """Walk HID short items; return {(kind, report_id): bits} for Input/Output main items."""
    i, size, count, rid, depth = 0, 0, 0, 0, 0
    bits = {}
    while i < len(desc):
        prefix = desc[i]
        n = prefix & 0x03
        n = 4 if n == 3 else n
        data = int.from_bytes(bytes(desc[i + 1:i + 1 + n]), "little") if n else 0
        tag = prefix & 0xFC
        if tag == 0x74:
            size = data
        elif tag == 0x94:
            count = data
        elif tag == 0x84:
            rid = data
        elif tag in (0x80, 0x90):
            kind = "in" if tag == 0x80 else "out"
            bits[(kind, rid)] = bits.get((kind, rid), 0) + size * count
        elif tag == 0xA0:
            depth += 1
        elif tag == 0xC0:
            depth -= 1
        i += 1 + n
    return bits, depth


def main():
    for rel in REQUIRED:
        check((ROOT / rel).is_file(), f"missing file: {rel}")

    mod = read("zephyr/module.yml")
    for needle in ("cmake: .", "kconfig: Kconfig", "dts_root: ."):
        check(needle in mod, f"zephyr/module.yml lacks '{needle}'")

    defined = kconfig_defined()
    for sym in ("ZMK_SPACEMOUSE", "ZMK_SPACEMOUSE_USB_HID_INDEX", "ZMK_SPACEMOUSE_REPORT_INTERVAL_MS",
                "ZMK_SPACEMOUSE_KEEP_PER_MILLE", "ZMK_SPACEMOUSE_DEADBAND",
                "ZMK_SPACEMOUSE_ZERO_REPEATS", "ZMK_SPACEMOUSE_BLE_QUEUE_SIZE",
                "ZMK_INPUT_PROCESSOR_SPACEMOUSE"):
        check(sym in defined, f"Kconfig does not define {sym}")

    cmake = read("CMakeLists.txt")
    for src in re.findall(r"(src/[\w./-]+\.c)", cmake):
        check((ROOT / src).is_file(), f"CMakeLists.txt lists missing {src}")
    for src in sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "src").glob("*.c")):
        check(src in cmake, f"{src} is not compiled by CMakeLists.txt")

    sources = [p for p in list((ROOT / "src").glob("*.c")) + list((ROOT / "include").rglob("*.h"))]
    all_used = used_symbols(cmake)
    for p in sources:
        all_used |= used_symbols(p.read_text(encoding="utf-8"))
    for sym in sorted(all_used):
        check(sym in defined or sym in EXTERNAL_SYMBOLS,
              f"CONFIG_{sym} is used but defined neither in Kconfig nor in the known ZMK/Zephyr set")

    for p in sources:
        for inc in re.findall(r'#include\s+<(zmk/spacemouse/[\w./]+)>', p.read_text(encoding="utf-8")):
            check((ROOT / "include" / inc).is_file(), f"{p.name} includes missing <{inc}>")

    binding = read("dts/bindings/input_processors/zmk,input-processor-spacemouse.yaml")
    check(f'compatible: "{COMPAT}"' in binding, "binding compatible is wrong")
    check("ip_two_param.yaml" in binding, "binding must include ip_two_param.yaml (2 cells)")
    check(re.search(r"gain:\s*\n(\s+\w+:.*\n)*?\s+default:\s*768", binding) is not None,
          "binding must define 'gain' with default 768")

    proc = read("src/input_processor_spacemouse.c")
    check("#define DT_DRV_COMPAT zmk_input_processor_spacemouse" in proc,
          "processor DT_DRV_COMPAT does not match the binding")
    check("DT_INST_FOREACH_STATUS_OKAY" in proc and "DEVICE_DT_INST_DEFINE" in proc,
          "processor must define one device per DT instance")
    check("ZMK_INPUT_PROC_STOP" in proc, "processor must consume REL_X/REL_Y (ZMK_INPUT_PROC_STOP)")
    check("zmk_spacemouse_add" in proc, "processor must feed zmk_spacemouse_add()")

    dtsi = read("dts/input/processors/spacemouse.dtsi")
    check("zip_spacemouse:" in dtsi, "dtsi lacks the zip_spacemouse label")
    check(f'compatible = "{COMPAT}"' in dtsi, "dtsi compatible is wrong")
    check("#input-processor-cells = <2>" in dtsi, "dtsi must declare 2 cells")
    check("/omit-if-no-ref/" in dtsi, "dtsi node must be /omit-if-no-ref/")
    check("dt-bindings/zmk/spacemouse.h" in dtsi, "dtsi must include the dt-bindings header")

    dtb = read("include/dt-bindings/zmk/spacemouse.h")
    expected = {"SM_TX": 0, "SM_TY": 1, "SM_TZ": 2, "SM_RX": 3, "SM_RY": 4, "SM_RZ": 5,
                "SM_INV": 0x80, "SM_NONE": 0xFF}
    for name, val in expected.items():
        m = re.search(rf"#define\s+{name}\s+(\(?0x[0-9A-Fa-f]+\)?|\(?\d+\)?)", dtb)
        if check(m is not None, f"dt-bindings header lacks {name}"):
            check(int(m.group(1).strip("()"), 0) == val, f"{name} must be {val:#x}")
    eng = read("include/zmk/spacemouse/engine.h")
    check(re.search(r"#define\s+SM_ENGINE_INV\s+0x80", eng) is not None, "engine SM_ENGINE_INV != 0x80")
    check(re.search(r"#define\s+SM_ENGINE_NONE\s+0xFF", eng, re.I) is not None, "engine SM_ENGINE_NONE != 0xFF")

    hid = read("include/zmk/spacemouse/hid.h")
    m = re.search(r"zmk_spacemouse_report_desc\[\]\s*=\s*\{(.*?)\};", hid, re.S)
    if check(m is not None, "hid.h lacks zmk_spacemouse_report_desc"):
        body = re.sub(r"//.*", "", m.group(1))
        desc = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", body)]
        bits, depth = hid_report_bits(desc)
        check(depth == 0, f"report map collections unbalanced (depth {depth})")
        check(bits.get(("in", 1)) == 96, f"report 1 must be 96 bits (6 x int16), got {bits.get(('in', 1))}")
        check(bits.get(("in", 3)) == 32, f"report 3 must be 32 bits, got {bits.get(('in', 3))}")
        check(bits.get(("out", 4)) == 8, f"report 4 must be 8 bits, got {bits.get(('out', 4))}")
    check(re.search(r"int16_t\s+axes\[6\]", hid) is not None, "motion body must be int16_t axes[6]")
    check(re.search(r"uint8_t\s+bits\[4\]", hid) is not None, "buttons body must be uint8_t bits[4]")

    api = read("include/zmk/spacemouse/spacemouse.h")
    impl = "".join(p.read_text(encoding="utf-8") for p in (ROOT / "src").glob("*.c"))
    for fn in re.findall(r"^\w[\w\s\*]*?\b(zmk_spacemouse_\w+)\s*\(", api, re.M):
        check(re.search(rf"\b{fn}\s*\([^;]*\)\s*\{{", impl) is not None, f"{fn}() is declared but never defined")

    if failures:
        for f in failures:
            print("FAIL", f)
        print(f"FAILED {len(failures)} check(s)")
        return 1
    print("OK all module checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
