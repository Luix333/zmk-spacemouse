# zmk-spacemouse

A ZMK module that makes a trackball (or any REL_X/REL_Y pointing device) act as a 3Dconnexion-compatible SpaceMouse, on its own USB HID interface and its own BLE HID service, so the keyboard's normal mouse keeps working. The cursor stays still while a 3D layer is active.

## How it works

A SpaceMouse reports how far its cap is pushed, and CAD programs treat that as a velocity. The module counts ball motion as if it pushed a virtual cap that springs back once the ball stops: every `CONFIG_ZMK_SPACEMOUSE_REPORT_INTERVAL_MS` milliseconds the current deflection is sent, then multiplied by `CONFIG_ZMK_SPACEMOUSE_KEEP_PER_MILLE` / 1000 for the next tick. Values within `CONFIG_ZMK_SPACEMOUSE_DEADBAND` units of zero are sent as 0. After motion stops, `CONFIG_ZMK_SPACEMOUSE_ZERO_REPEATS` all-zero reports follow to signal the host that the device is at rest.

## Install

Add the module to your ZMK keymap repository:

```yaml
# config/west.yml
manifest:
  remotes:
    - name: luix333
      url-base: https://github.com/Luix333
  projects:
    - name: zmk-spacemouse
      remote: luix333
      revision: main  # pin to a commit SHA for reproducibility
```

Then add the following to your `.conf` file (central half only):

```
CONFIG_ZMK_SPACEMOUSE=y
CONFIG_USB_HID_DEVICE_COUNT=2
```

### Optional: 3Dconnexion identity

The official 3DxWare driver only accepts 3Dconnexion vendor IDs. To make the driver recognise the device, set:

```
CONFIG_USB_DEVICE_VID=0x256F
CONFIG_USB_DEVICE_PID=0xC631
CONFIG_USB_DEVICE_MANUFACTURER="3Dconnexion"
CONFIG_USB_DEVICE_PRODUCT="SpaceMouse Pro Wireless (cabled)"
CONFIG_BT_DIS_PNP_VID_SRC=2
CONFIG_BT_DIS_PNP_VID=0x256F
CONFIG_BT_DIS_PNP_PID=0xC63A
```

After changing these IDs, remove the keyboard from the host's Bluetooth list and pair again so the new identity is advertised.

## Map the ball

Include the SpaceMouse input processor in your devicetree overlay:

```dts
#include <input/processors/spacemouse.dtsi>
```

Then add `zmk,input-listener` overrides on the layers where you want 3D control. The first matching override in file order wins, so place these above other overrides and list pan/zoom before orbit:

```dts
/ {
  trackball_listener {
    compatible = "zmk,input-listener";
    device = <&trackball>;

    sm_zoom {
        layers = <11>;
        input-processors = <&zip_spacemouse SM_RY SM_TY>;
    };

    sm_pan {
        layers = <10>;
        input-processors = <&zip_spacemouse SM_TX SM_TZ>;
    };

    sm_orbit {
        layers = <9>;
        input-processors = <&zip_spacemouse SM_RZ SM_RX>;
    };
  };
};
```

### Axis constants

| Constant | Meaning |
|----------|---------|
| `SM_TX`  | Translation X |
| `SM_TY`  | Translation Y |
| `SM_TZ`  | Translation Z |
| `SM_RX`  | Rotation about X |
| `SM_RY`  | Rotation about Y |
| `SM_RZ`  | Rotation about Z |
| `SM_INV` | OR onto an axis to invert it |
| `SM_NONE`| Drop that ball axis |

### Gain

The `gain` property scales ball motion into SpaceMouse deflection units. The default is `768` (equivalent to a 3.0× multiplier). Override it on the node, for example `&zip_spacemouse { gain = <512>; };`.

## Tuning

| Kconfig option | Default | Meaning |
|---|---|---|
| `CONFIG_ZMK_SPACEMOUSE` | `y` (when processor enabled) | Enable the SpaceMouse HID interface |
| `CONFIG_ZMK_SPACEMOUSE_USB_HID_INDEX` | `1` | USB HID device index for the SpaceMouse interface (ZMK uses index 0) |
| `CONFIG_ZMK_SPACEMOUSE_REPORT_INTERVAL_MS` | `16` | Milliseconds between SpaceMouse reports (range 4–100) |
| `CONFIG_ZMK_SPACEMOUSE_KEEP_PER_MILLE` | `700` | Deflection kept each tick, in thousandths (range 0–1000; lower springs back faster) |
| `CONFIG_ZMK_SPACEMOUSE_DEADBAND` | `3` | Output dead band in report units (range 0–50) |
| `CONFIG_ZMK_SPACEMOUSE_ZERO_REPEATS` | `3` | All-zero reports sent after motion stops (range 1–10) |
| `CONFIG_ZMK_SPACEMOUSE_BLE_QUEUE_SIZE` | `8` | Queued BLE SpaceMouse reports (range 2–32) |

Motion too slow to clear the dead band is dropped. For finer control at low speed, raise `gain` or lower `CONFIG_ZMK_SPACEMOUSE_DEADBAND`.

## Testing

1. Open `tools/spacemouse-monitor.html` in Chrome or Edge (WebHID required), click **Connect**, and pick the device. The bars move while a 3D layer is held.
2. Then install 3DxWare 10 and check that 3Dconnexion Home lists a SpaceMouse.

## Caveats

- BLE acceptance by 3DxWare is untested.
- Emulating another vendor's IDs is for personal use.
- Linux spacenav "jiggle" is not implemented.

## Credits

- [AndunHH/spacemouse](https://github.com/AndunHH/spacemouse), for the reverse-engineered report map.
- [badjeff/zmk-hid-joystick](https://github.com/badjeff/zmk-hid-joystick), for showing a second HID service works as a ZMK module.
- ZMK's `hog.c` / `usb_hid.c`, which the transports mirror (MIT).

## License

MIT
