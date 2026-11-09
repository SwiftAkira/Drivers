# MADLIONS MAD LIGHT 60 HE: USB HID configuration protocol

Reverse-engineered by static analysis of the FGG / FIERCE GAMING GEAR WebHID driver (`https://hub.fgg.com.cn/`). Nothing here was tested against hardware.

## Changelog

**rev 2**: the device UI chunks were downloaded and deobfuscated. The UI for this board lives in `ConfigPage-eQr_MPuY.js` (`./duck-bread-keyboard/ConfigPage.vue`), with shared widgets in `vue-draggable-plus-dU8dfF6a.js`. Resolved items are marked **[RESOLVED r2]**.

* **Layout**: the real row/col → key map comes from the vendor layout object `Ft` (§10, `mad60he-layout.json`). Row 3 is `col1` = Left Shift, `col3..12` = Z../, `col13` = Right Shift. Fn is matrix `4,12`, the right-most bottom key.
* **Ranges**:
  * Actuation 0.10–3.50 mm, step 0.02; step 0.01 when competitive mode is on.
  * RT 0.02–3.51 mm, step 0.01; min 0.01 when competitive mode is on.
  * Dead zones 0–1.00 mm, step 0.02.
  * RS/SOCD level: 0.01 mm units, UI range 0.1–3.5 mm, step 0.02.
  * The UI clamps values to `limit` = 3.5 mm.
* **DKS**: the 10-bit mask is decoded (§5.3). A DKS slot is bound by keycode `0x7E05 + slot` (20 slots). DKS trigger depths are not configurable on this board.
* **MT/TGL**: slot allocation, timers and keycode restrictions are documented.
* **Advanced keys**: at most 20 per profile, enforced by the UI.
* **Lighting** (§8):
  * The effect id list for this board, plus brightness (1–200) and speed (1–255) ranges.
  * Per-key RGB ("Customization", effect 1) **is** offered on this model.
  * The box/side-light zone **exists** in the UI for this model, with 5 modes, brightness 0–160 and speed 0–40.
* **Vendor keycodes** (§6.1, `mad60he-keycodes.json`):
  * `0x7E01–0x7E04`: WASD swap, Win/Mac toggle, Win lock, 6KRO toggle.
  * `MO(1)`/`MO(3)` = Win Fn / Mac Fn.
  * Lighting keycodes, and 16 macros.
* **Macro delay**: the web driver sends **base-256 with +1 offset**. Every key event is followed by a delay record (§6.3).
* **Persistence**: the UI never uses the single-key actuation/RT commands. It always rewrites all keys with the erase/write bulk commands. Per-key RGB needs an explicit save; nothing else is saved explicitly (§4.6).
* **`axosome`**: index into the vendor's switch-parameter table. 6 = "禾金磁珀PRO" (Magnetic Amber Pro); it supplies the ranges above (§2, §10.2).
* **Data reports**: the UI handler confirms the type values (§9).
* **Feature-bit polarity**: confirmed, e.g. `nKroSwitch` 1 = NKRO (§3.2).
* **Calibration**: the UI flow (§7) is documented: 150 ms polling, 500 ms settle.
* **RT `on`**: confirmed to be 0/1. Continuous RT is not offered for this board.
* **Per-key switch type (MixAxle)**: read at start-up but never written by this board's UI.

**rev 1**: protocol classes from `index-D9yt26aK.js`.

## Conventions

* **Device**: VID `0x373B`, PID `0x10F6`. Bootloader PID `0x2085` (usagePage `0xFF`, usage `0x01`, `firmwareMark` `"mad60-light"`).
* **Classes**:
  * Protocol controller: `DuckBread` (extends `HidDevice2` → `WebHidGeneral`).
  * UI: `ConfigPage-eQr_MPuY.js`.
  * The device-table "digital" entry `Kl` is only a host-side profile store (IndexedDB `db-mad-light-60-profile`). Macros are also cached host-side, in `duck-bread-keyboard-macro`.
* **Source references**: deobfuscated files are in `scratchpad/pretty/fgg-deob/`.
  * `L<n>` = line in `index-D9yt26aK.js` (protocol).
  * `C<n>` = line in `ConfigPage-eQr_MPuY.js` (board UI).
  * `V<n>` = line in `vue-draggable-plus-dU8dfF6a.js` (shared widgets).
  * Tools: `scratchpad/tools/deob.js`, `strip_alias.py`, `parse_layout.js`, `gen_layout.js`, `gen_keycodes.js`.
  * Scratchpad path: `/tmp/claude-1000/-home-swift--claude-spares-spare-hvPZje/e56eb4a3-a7a6-4731-8c8f-c13c2e3889cc/scratchpad/`
* **UNVERIFIED** = inferred, not read directly from code. "The UI does X" = the vendor app's behaviour; that is the only behaviour known to work.

---

## 0. Device table entry (L16384–16425)

```js
{ controller: DuckBread, digital: Kl, meta: { deviceName: "Keyboard", component: "duck-bread-keyboard" },
  filters: [{ vendorId: 0x373b, productId: 0x10f6, usagePage: 0xff60, usage: 0x61,
    custom: { layer: 4, row: 5, col: 14, name: "MAD LIGHT 60HE", shaft: "magneto", title: "MAD LIGHT 60HE",
              cover: "mad60-light", themeConfigName: "mad60-10f6", theme: [white, black, red], axosome: 6,
              firmwareMark: "mad60-light" },
    boot: { vendorId: 0x373b, productId: 0x2085, usagePage: 0xff, usage: 0x1, firmwareMark: "mad60-light" } }] }
```

* The UI looks up its board config in `Ut[custom.title]` (C8539). For this board the entry is `{ layout: Ft, effectItems: Bt, switchTabItems: Rt, extraLightItems: Pt }`.
* Tabs (`Rt`): ChangeKey, Lighting, Performance ("MagneticShaftTravel"), AdvancedKey, Macro, Other.
* No `advancedKeyVersion` is set, so `hasAdvancedKey` = true and RS/SOCD are offered.

---

## 1. Transport and framing

### 1.1 Opening (L6418, L23787)
* Open the hidraw node of the collection with usagePage `0xFF60` and usage `0x61` (interface 1; the descriptor starts `06 60 FF 09 61`).
* `connect()` only opens the device. No handshake is sent.

### 1.2 Reports (L13135 `Pu`, L14548 `HidDevice2`)
* Requests are **32 bytes**, sent with `sendReport(0x00, data)`. The descriptor has no report IDs.
  * **Linux hidraw write**: 33 bytes = `0x00` + 32-byte payload.
  * **Linux hidraw read**: 32 bytes, no prefix.
* Payload layout: byte0 = command id (§1.4). For vendor commands under `0x02/0x03`: byte1 = `0x96`, byte2 = sub-command (§1.5).
* **No checksum.** Unused bytes are `0x00`.
* **u16 fields are big-endian.** The only exception is the macro delay (§6.3).
* **Responses**: the reply has the request layout with the data fields filled in. The web app reads every field at the offset it wrote it.

### 1.3 Reply matching, async reports, timing (L14553–14585)
* The web app keeps a FIFO of `{requestCode = byte0}`. For each input report:
  1. If byte0 == `00`: ignored.
  2. If bytes 0..2 == `02 96 14`: an **async data report** (§9), not a reply.
  3. Otherwise it resolves the first pending request with the same byte0.
* There is **no timeout or retry** in the web app.
  Recommendation: one request in flight, ~1 s timeout, check that bytes 1–2 echo for vendor commands (UNVERIFIED), and treat `FF` (QMK `id_unhandled`) as an error (UNVERIFIED).
* **Delays the web app inserts:**

  | where | delay |
  |---|---|
  | between items of the initial read (§3.0) | 50 ms |
  | after the first and the last chunk of a bulk actuation/RT write | 200 ms |
  | after the per-key-RGB save | 100 ms (driver) + 500 ms (UI) |
  | after `finishCalibration` | 500 ms |

### 1.4 Command ids (enum `Mu`, L13166; = QMK VIA)
| id | name | used by the UI |
|---|---|---|
| 01 | id_get_protocol_version | no |
| 02 | id_get_keyboard_value (vendor GET `02 96 sub`) | yes |
| 03 | id_set_keyboard_value (vendor SET `03 96 sub`) | yes |
| 04 / 05 | dynamic_keymap get / set keycode | get: no / **set: yes** |
| 06 | dynamic_keymap_reset | yes ("restore default keys") |
| 07 / 08 | custom/lighting set / get (`0x41` lighting, `0x42/0x45` per-key RGB) | yes |
| 09 | custom/lighting save | no |
| 0A | eeprom_reset | no |
| 0B | bootloader_jump | no |
| 0C | macro_get_count | yes |
| 0D | macro_get_buffer_size | yes |
| 0E / 0F | macro get / set buffer | yes |
| 10 | macro_reset | no |
| 11 | get_layer_count | no |
| 12 / 13 | dynamic_keymap get / set buffer | get: yes / set: only on profile restore |
| FE | vial prefix (MT/TGL slots) | yes |

### 1.5 Vendor sub-commands (enum `Bu`, L13170)
| sub | name | § | used by the UI for this board |
|---|---|---|---|
| 00 | firmware/protocol version | 3.1 | yes |
| 03 | single-key actuation | 4.1 | **no** (bulk only) |
| 07 | single-key RT | 4.3 | **no** (bulk only) |
| 08 | legacy calibrate | 7.1 | no |
| 0B | dead zones | 4.5 | yes |
| 0D | bulk actuation | 4.2 | yes |
| 0E | bulk RT | 4.4 | yes |
| 0F | DKS | 5.3 | yes |
| 10 | mixAxle (per-key switch type) | 7.3 | read only |
| 11 | feature flags | 3.2 | yes |
| 12 | save per-key RGB | 8.3 | yes |
| 13 | current default layer | 3.3 | yes |
| 14 | data report (device→host) | 9 | yes |
| 15 | factory reset | 3.4 | yes |
| 18 / 19 | calibration start / finish | 7.1 | yes |
| 1B | completeStatusBuffer | 7.2 | no |
| 1C | live ADC/travel buffer | 7.2 | yes |
| 1D | bottom optimisation | 3.5 | yes |
| 1E | competitive (game) mode | 3.5 | yes |
| 1F | calibration init-status buffer | 7.2 | yes |
| 20 | RS / SOCD / OKS | 5.2 | yes |
| 21 | box light | 8.4 | yes |
| 09, 0A, 16, 17 | realTime* | — | no |
| 98 | iap | — | no |
| (cmd 07/08) 41 | lighting | 8.1 | yes |
| (cmd 07) 42 / (cmd 08) 45 | per-key RGB set / get | 8.2 | yes |
| (cmd FE) 0D 01/02 | tap-dance get / set | 5.1 | yes |

* **`flashOp`** (enum `Eu`): `0` normal, `1` erase, `2` write, `3` eraseAndWrite.
* `DuckBread.isV2` is hard-coded `true` (L14591), so **all V2 encodings apply** (u16, 0.01 mm).

---

## 2. Units and ranges **[RESOLVED r2]**

**Units.** All distances are **u16 BE in 0.01 mm**: `mm = raw/100`, `raw = round(mm*100)`.
* Actuation, RT press/release, dead zones, calibration "trip" and RS level all use this unit.
* The web app truncates (`setUint16(100*mm)`), so e.g. 0.29 mm can become 28. Use `round()`.

**Where the ranges come from.** `dl(custom.name, version)` (C10259) returns the entry of `sl.default` (C10138) whose `value == custom.axosome` (6). `ul` has no "MAD LIGHT 60HE" entry, so no version-specific override applies.

| parameter | min | max | step | step / min with competitive mode on (`02 96 1E` = 1) | UI source |
|---|---|---|---|---|---|
| actuation point (`travel*`) | 0.10 | 3.50 | 0.02 | step 0.01 (`travelUpgradeStep`) | C14267–14272, `ASingleBondTriggered` C13300 |
| RT press / release sensitivity (`rt*`) | 0.02 | 3.51 | 0.01 | min 0.01 (`rtUpgradeMin`), step 0.01 | C14269–14273, `SettingQuickTrigger` C13483 |
| `limit` (clamp before writing RT; display clamp) | — | 3.50 | — | — | C14271, C14554 |
| top / bottom dead zone | 0.00 | 1.00 | 0.02 | — | `AdvancedSettings` defaults C13175 (min 0, max 1, interval 0.02) |
| RS/SOCD actuation (`rsApcLv`) | 0.10 | 3.50 | 0.02 | — | C10878–10892 (min-point 0.1, max-point = limit), C9492 |
| DKS trigger depths | fixed, not editable (UI `ban-change-point`) | | | | C10848 |

* **Factory defaults are not in the UI.** It reads all values from the device; the actuation widget's prop default of 1.5 mm is never used.
* The "global" slider initial values come from the matrix position `2,12`, which has no physical key (§10.1).
* The UI shows dead-zone warnings when a value equals the minimum (0.00).

---

## 3. Identification and global settings

### 3.0 What the UI reads on connect (C15022–15054)
The UI reads these in order, with a 50 ms sleep after each:

1. `getProtocolVersion` (§3.1)
2. `getAllKeyAction(4,5,14)` (§6.2)
3. `getTogThAll` (§4.2)
4. `getRTKeyInfoAll` (§4.4)
5. `getMixAxleAll` (§7.3)
6. `getDeadBand` (§4.5)
7. `getLightInfo` (§8.1)
8. `getKeyboardFeature` (§3.2)
9. `getMacroBuffer` (§6.3)
10. `getCustomLamplight(5,14)` (§8.2)
11. `getLayer` (§3.3)
12. `getBottomOptimizeSwitch` (§3.5)
13. `getGameModeSwitch` (§3.5)
14. `getBoxLightInfo` (§8.4)
15. Advanced keys:
    * `getTapDance(i)` for every TD keycode found in the keymap (all layers).
    * `getDynamicKeyStroke(keyId-5)` for every DKS keycode found.
    * `getRappySnappy(0..19)` (always 20 slots).

On failure the UI shows "加载键盘数据异常，请确保键盘固件是最新版" ("error loading keyboard data, make sure the firmware is up to date").

### 3.1 Firmware/protocol version (L14004)
* **Request**: `02 96 00`.
* **Response**: version = `hex(b[5]) + "." + hex(b[6]).padStart(2,'0')`, e.g. `01 05` → "1.05".
* The UI compares this with the server's firmware list to show "update available".

### 3.2 Feature flags (L14018) **[polarity RESOLVED r2, C13007–13087]**
* **GET**: `02 96 11` → bytes 3..7.
* **SET**: `03 96 11 rgbArea wasd mac winLock nkro`. Always read–modify–write.

| byte | field | meaning |
|---|---|---|
| 3 | rgbArea | Read-only in the UI (preserved on write). When `1`, the main lighting-effect panel is greyed out (C12424, C12634). Probably toggled from the keyboard; meaning UNVERIFIED |
| 4 | wasdSwitch | 1 = WASD↔arrows swapped |
| 5 | macSwitch | 1 = Mac mode. The default layer changes to Mac Base; the UI re-reads §3.3 after writing |
| 6 | winLock | 1 = Win key locked |
| 7 | nKroSwitch | **1 = NKRO, 0 = 6KRO**. The UI's "Switch to 6-key rollover" switch = `!nKroSwitch` |

### 3.3 Current default layer (L14076), GET only
* **Request**: `02 96 13`. Response byte 3 = raw.
* Mapping `{1:0, 2:0, 4:2, 8:2}`: raw 1 or 2 → layer 0 **Win Base**; raw 4 or 8 → layer 2 **Mac Base**.
* Layer names from the UI (`Nt`, C8533): 0 Win Base, 1 Win Fn, 2 Mac Base, 3 Mac Fn **[RESOLVED r2]**.
* The UI opens the keymap and lighting views on this layer.

### 3.4 Resets (L14067; UI C14765)
* **Keymap reset**: `06`. Restores the factory keymap.
* **Factory reset**: `03 96 15`. The UI also clears its host-side macro cache, waits 3 s, then reloads the page.

### 3.5 Switches
| UI name | GET | SET | value |
|---|---|---|---|
| "False touch prevention mode" (bottom always triggers) | `02 96 1D` → b[3] | `03 96 1D v` | 1 = on |
| "Competitive mode" | `02 96 1E` → b[3] | `03 96 1E v` | 1 = on. Unlocks the 0.01 mm actuation step and 0.01 mm RT minimum (§2). Offered for this board |

### 3.6 Not present for this board
The DuckBread UI has no setting for any of the following:

* polling rate
* sleep / light timer
* on-board profiles: profiles are host-side only (export file `*.mad-keyboard-config`)
* debounce
* per-key switch-type editing

---

## 4. Magnetic-switch travel

The UI's bulk transfers use `index = row*14 + col` over all 70 positions. Bulk offsets are **key indices**, u16 BE at bytes 5–6.

### 4.1 Single-key actuation (L13674). The UI never uses it.
| | bytes |
|---|---|
| GET request | `02 96 03 row col` |
| GET response | `[5..6]` apc u16 BE (0.01 mm) |
| SET request | `03 96 03 row col apcHi apcLo` |

### 4.2 All-key actuation (L13715, L14709, L14916)
* **GET**: `02 96 0D 00 00 offHi offLo 0C`, with off = 0, 12, …, 60 (6 requests).
  * Response: 12 × u16 BE apc at bytes 8..31. Entries past index 69 are padding.
* **SET**: `03 96 0D 00 00 offHi offLo 0B flashOp data[22]`.
  * Data starts at byte 9: 11 × u16 BE.
  * off = 0, 11, …, 66 → 7 chunks. The last chunk holds 4 keys plus zero padding; size stays 11.
  * `flashOp` = 1 (erase) on the first chunk, 2 (write) on the last, 0 otherwise.
  * Wait 200 ms after the first and after the last chunk.
* **UI usage (C14508)**: the user selects keys and moves the slider. The UI changes `apc` for the selected positions in its full 5×14 copy and **re-sends all 70 keys**.

### 4.3 Single-key RT (L13805). The UI never uses it.
| | bytes |
|---|---|
| GET request | `02 96 07 row col` |
| GET response | `[5]` on, `[6..7]` release u16 BE, `[8..9]` press u16 BE |
| SET request | `03 96 07 row col on relHi relLo pressHi pressLo` |

* **`on`: 0 = RT off, 1 = RT on** **[RESOLVED r2]**. The UI writes `+switch` (C14554). No continuous-RT value exists for this board.
* `press` = "Rapid Trigger Sensitivity" (re-press distance). `release` = "Rapid Reset Sensitivity" (lift distance).

### 4.4 All-key RT (L13860, L14926, L14941)
* **GET**: `02 96 0E 00 00 offHi offLo 04`, with off = 0, 4, …, 68 (18 requests).
  * Response: 4 × `[on, relHi, relLo, pressHi, pressLo]` from byte 8.
* **SET**: `03 96 0E 00 00 offHi offLo 04 flashOp data[20]` (data from byte 9).
  * 18 chunks; the last holds 2 keys.
  * `flashOp` and waits as in §4.2.
* The UI clamps press and release to `limit` (3.5) and always re-sends all keys (C14546).

### 4.5 Dead zones (L13929)
| | bytes |
|---|---|
| GET request | `02 96 0B` |
| GET response | `[3..4]` top u16 BE, `[5..6]` bottom u16 BE |
| SET request | `03 96 0B topHi topLo botHi botLo` |

* UI meaning (C13175, C14529):
  * **top** = "Press (trigger)" dead zone: initial travel that is ignored.
  * **bottom** = "Lift (reset)" dead zone.
* Range 0–1.00 mm, step 0.02.

### 4.6 Persistence **[RESOLVED r2 as far as the UI goes]**
* Actuation and RT are only ever written with the bulk erase/write sequence. The single-key SET commands are unused, so **whether they persist is UNVERIFIED**. Use the bulk commands.
* Keymap single-key set (`05`), MT/TGL, DKS, RS, lighting, box light, features and switches are written once, with **no save command**. The UI relies on them persisting.
* **Per-key RGB is the only exception**: the colours must be followed by `03 96 12` ("Save custom light effects").

---

## 5. Advanced keys **[RESOLVED r2]**

The UI caps the total at **20 advanced keys** (DKS + MT + TGL + RS/SOCD), shown as "Current binding item: n/20" (C11135, C11461).

### 5.1 MT (hold/tap) and TGL (toggle): Vial tap-dance entries (L14360; UI C11271–11301, V7001)
| | bytes |
|---|---|
| GET request | `FE 0D 01 idx` |
| GET response | `[4..5]` tap keycode, `[6..7]` hold keycode, `[8..9]` timer u16 BE (ms) |
| SET request | `FE 0D 02 idx tapHi tapLo holdHi holdLo timerHi timerLo` |

* **Binding**: write keycode `0x5700 | idx` into the keymap at the chosen (layer, row, col) with `05 layer row col 57 idx`.
* **MT** = idx 0..19 (`keyId ≤ 0x13`):
  * `tap` = click key, `hold` = hold key.
  * `timer` = "Press and hold time", **10–1000 ms, step 10**, UI default 10.
  * New slot = (highest MT idx in the keymap) + 1, starting at 0.
* **TGL** = idx ≥ 20:
  * `tap` = the toggled key, `hold` = 0, `timer` fixed at **200 ms**.
  * New slot = (highest TGL idx) + 1, starting at 20.
* The UI only stores **basic keycodes** for tap and hold. It rebuilds them as `0x00kk`.
* **Delete**: write the key's default keycode back with `05`. The TD entry itself is left alone.

### 5.2 RS / SOCD / OKS (L14466; UI C11303–11414, C9336–9862)
Request and response layout:

| byte | field | value the UI writes |
|---|---|---|
| 0–2 | `02`/`03`, `96`, `20` | |
| 3–4 | `00 00` | |
| 5 | subField | `01` get, `03` set |
| 6 | index | slot 0..19 |
| 7 | mode | `1` RS; `2` SOCD "Last Input Priority"; `3` "Absolute Key1 Priority"; `4` "Absolute Key2 Priority"; `5` "Neutral"; `6` OKS (not offered); `0` NONE = deleted |
| 8 | id | = index. The setter subtracts 26 if ≥ 26; delete writes id 0xFF, which goes on the wire as `0xE5` |
| 9–10 | rsApcLv | u16 BE **= 100 × mm** (0.01 mm). UI range 0.10–3.50, step 0.02. With "follow global" it is set to the global actuation |
| 11 | gapcSw | 1 = "follow global settings" (actuation/RT follow the global values) |
| 12 | rtSw | 1 = RT enabled for the pair |
| 13, 14 | key1Row, key1Col | first selected key |
| 15, 16 | key2Row, key2Col | second ("bind") key |
| 17 | layer | always 0 |

* **Slots**: the UI reads slots 0..19 at start-up. A new pair takes the lowest slot whose mode is 0.
* Pairs are bound **by row/col**; no keymap entry is written.
* The two keys must differ.
* The UI only allows keys whose layer-0 keycode is one of: letters, digits, Space, `/ . , \` - = [ ] \ ' ;`, arrows (0x4F–0x52), or numpad 0x54–0x63 (C11570–11585, C11780). Others are greyed out.
* **Delete**: `03 96 20 00 00 03 idx 00 E5 …`, i.e. mode 0 with the remaining fields 0.

### 5.3 DKS (L14272, action class L13483; UI C11246–11270, C10557–10580, V7317–7405, V7580)
| | bytes |
|---|---|
| GET request | `02 96 0F 00 00 01 slot` |
| GET response | bytes 7..22 = 4 actions |
| SET request | `03 96 0F 00 00 03 slot` + 16 bytes at 7..22 |

* Each action is 4 bytes: `[statusHi, statusLo, kcHi, kcLo]`.
  * `kc` is a **basic** keycode (`0x00kk`); `0x0000` = unused row.
  * `status` is a u16 BE bitmask; bit *i* = `keyStatus[i]` in the UI.
* **Binding a slot to a key**: write keycode **`0x7E05 + slot`** (slot 0..19, keyId 0x05–0x18) into the keymap with `05`.
  * New slot = (highest DKS keyId in the keymap) + 1, starting at 0x05.
  * The UI order is: `setDynamicKeyStroke(slot)` first, then `setOneKeyAction`.
* **Trigger depths**: the four DKS events are fixed by the firmware. The UI passes `ban-change-point` and displays them as 1.0 / 3.0 / 3.0 / 1.0 mm. Those are display defaults (`{min:10, max:30}` in 0.1 mm units), not device values. Their true values are UNVERIFIED.
* **Events** (UI icons):

  | event | icon | meaning |
  |---|---|---|
  | E0 | `dks-start` | press passes the first point |
  | E1 | `dks-bottom` | bottom-out |
  | E2 | `dks-uplift` | lift from the bottom |
  | E3 | `dks-top` | full release |

* **Bit map** (from the UI editor logic):

  | bit | meaning |
  |---|---|
  | 0 | key event (tap/press) at E0 |
  | 1 | held E0→E1 |
  | 2 | held through E1 |
  | 3 | key event at E1 |
  | 4 | held E1→E2 |
  | 5 | held through E2 |
  | 6 | key event at E2 |
  | 7 | held E2→E3 |
  | 8 | never set by the UI (cleared when the E0 bar is reset) |
  | 9 | key event at E3 |

* **The only status values the UI produces** (one row may combine bars from different columns that don't overlap):

  | pattern | status |
  |---|---|
  | tap@E0 | `0x001` |
  | E0→E1 | `0x003` |
  | E0→E2 | `0x01F` |
  | E0→E3 | `0x0FF` |
  | tap@E1 | `0x008` |
  | E1→E2 | `0x018` |
  | E1→E3 | `0x0F8` |
  | tap@E2 | `0x040` |
  | E2→E3 | `0x0C0` |
  | tap@E3 | `0x200` |

* Firmware interpretation of other combinations: UNVERIFIED.

---

## 6. Keymap and macros

### 6.1 Keycodes **[vendor codes RESOLVED r2]**
* Each keycode is a 16-bit QMK (≥ 0.19 numbering) value, **big-endian** on the wire: `[keyClass, keyId]`.
* Full table: `mad60he-keycodes.json`.
* **Codes the vendor UI offers** (menus C8739–9103):

| group | codes |
|---|---|
| basic | HID usages 0x04–0xE7 (letters, digits, F1–F12, numpad, control/modifier keys, App 0x65), KC_TRNS 0x0001, NUHS 0x32, NUBS 0x64 |
| media/system | 0xA8 mute, 0xA9 vol+, 0xAA vol−, 0xAB next, 0xAC prev, 0xAD stop, 0xAE play/pause, 0xAF media, 0xB1 mail, 0xB2 calc, 0xB3 my computer, 0xB4 search, 0xB5 home, 0xB6 back, 0xB7 fwd, 0xB8 stop, 0xB9 refresh, 0xBA favourites, 0xBD/0xBE screen brightness ±, 0xC1 Mission Control, 0xC2 Launchpad |
| mouse | 0xCD–0xD0 move, 0xD1–0xD5 buttons 1–5, 0xD9–0xDC wheel |
| layers | `0x5221` MO(1) = **Win Fn**, `0x5223` MO(3) = **Mac Fn** |
| lighting | `0x7820` RGB toggle, `0x7821/0x7822` mode ±, `0x7823/0x7824` hue ±, `0x7827/0x7828` brightness ±, `0x7829/0x782A` effect speed ± |
| vendor | `0x7E01` swap WASD, `0x7E02` Win/Mac toggle, `0x7E03` Win-key lock, `0x7E04` 6KRO/NKRO toggle |
| advanced | `0x5700+i` MT (i < 20) / TGL (i ≥ 20); `0x7E05+s` DKS slot s (0..19) |
| macros | `0x7700+n`, M1..M16 (n = 0..15) |

* `0x7E1A–0x7E21` (side-light keys) are offered only for NANO 68, not this board.
* Factory defaults per layer are in `mad60he-layout.json`. Examples:
  * Win Fn: F1–F12 on the number row; Tab/Q/W/E/A/S/D = RGB keys; RShift / M / N / LWin = `0x7E01/02/04/03`.
  * Mac Fn: media keys on R..].

### 6.2 Keymap commands (VIA)
| op | request | response / notes |
|---|---|---|
| get one | `04 layer row col` | `[4..5]` keycode |
| set one | `05 layer row col kcHi kcLo` | **used by the UI** for every remap (C11383) |
| get buffer | `12 offHi offLo 1C` | bytes 4..31 |
| set buffer | `13 offHi offLo 1C data[28]` | |

* Buffer offset = `((layer*5+row)*14+col)*2` bytes; the full buffer is 560 bytes, read in 20 chunks of 28 bytes.

### 6.3 Macros **[delay base RESOLVED r2]** (L14626–14682; UI C12740–12869)
* **Commands**:
  * Count: `0C` → `[1]`.
  * Size: `0D` → `[1..2]` BE.
  * Read: `0E offHi offLo 1C` → bytes 4..31.
  * Write: `0F offHi offLo sz data…`.
* **Write strategy** (web app): re-serialize **all** macros (each followed by `00`; an empty macro is a single `00`), then write from offset 0 in 28-byte chunks. The tail is not cleared.
* **What the UI writes** for every recorded key event:
  * key down `01 02 kk` or key up `01 03 kk` (basic keycode),
  * **always followed by** a delay record `01 04 b2 b3`, even for 0 ms.
  * Mouse buttons become 0xD1–0xD5.
  * The UI never uses the extended types 5–7.
* **Delay encoding the web driver sends** (base 256, +1 offset, little-endian):
  * `b2 = min((ms & 0xFF) + 1, 0xFF)`, `b3 = min((ms >> 8) + 1, 0xFF)`; decode `ms = (b2-1) + 256*(b3-1)`.
  * Recorded delays are 0..65535 ms.
  * A low byte of 0xFF cannot be represented (it clamps to 0xFE).
  * Whether the firmware decodes base 256 or Vial's base 255 is UNVERIFIED. They agree below 255 ms.
* **Parsing**: the UI reads records in pairs (event, delay).
* **Count**: the UI offers M1–M16. The macro count and buffer size come from the device.

---

## 7. Calibration, live travel, switch type

### 7.1 Calibration (L14412; UI C13995 `Calibration`, C14591–14606, C13752)
1. Open the Performance → "Calibration" tab and turn on "Axial alignment". The UI then shows "travel data will be reported in real time".
2. **Start**: `03 96 18`.
3. **Poll**: `getCalibrationAdcTripCompStatusBuffer` (§7.2) **every 150 ms**. The second calibration dialog polls every 200 ms.
   * The user presses each key fully and holds it ~1 s, 2–3 times.
   * A key shows green when its `status` byte ≠ 0.
4. **Finish**: `03 96 19`, wait **500 ms**, then read the init-status buffer (`1F`). Status ≠ 0 = the key holds calibration data.

The legacy `03 96 08` is not used by this UI.

### 7.2 Status buffers (`02 96 <sub> 00 00 offHi offLo size`; data from byte 8)
| sub | size per request | per-key record |
|---|---|---|
| 1C | 4 (18 requests) | `adc` u16 BE (raw), `trip` u16 BE ×0.01 mm (live travel; the UI chart maxes at `limit` 3.5), `status` u8 |
| 1F | 24 (3 requests) | status u8 |

### 7.3 MixAxle (per-key switch type)
* `getMixAxleAll(5,14)`: `02 96 10 00 00 04 offHi offLo 17` → types from byte 9.
* It is read at start-up and kept in profiles, but **never edited** by this board's UI. The write commands are as in rev 1:
  * `START_CFG` `02 96 10 00 00 01`
  * `SET_KEY` `03 … 05`
  * `SET_KEY_ALL` `03 … 07`
  * `SET_KEY_BUFFER` `03 … 06`
  * `SAVE_CFG` `03 96 10 00 00 02`

---

## 8. Lighting **[RESOLVED r2]** (UI `jl`, C12353–12657)

### 8.1 Main backlight effect (L13957)
| | bytes |
|---|---|
| GET request | `08 41` |
| GET response | `[2]` effect, `[3]` unused, `[4]` speed, `[5..7]` R G B, `[8]` brightness |
| SET request | `07 41 effect 00 speed R G B brightness` (no save) |

* **Effect ids** (`Bt`, C8203; shared by all DuckBread boards):

  | id | name | brightness | color | speed |
  |---|---|---|---|---|
  | 0 | Close (off) | — | — | — |
  | 1 | Customization (per-key static, §8.2) | ✓ | ✓ (applies to selected keys) | — |
  | 2 | Monochromatic Constant Brightness | ✓ | ✓ | — |
  | 3 | Focus | ✓ | ✓ | — |
  | 4 | Rainbow static decreasing up and down | ✓ | ✓ | — |
  | 5 | Rainbow static decreasing left and right | ✓ | ✓ | — |
  | 6 | Monochromatic Breathing | ✓ | ✓ | ✓ |
  | 7 | Ribbon fluttering 1 | ✓ | ✓ | ✓ |
  | 8 | Ribbon fluttering 2 | ✓ | ✓ | ✓ |
  | 9 | Rotating Belt 1 | ✓ | ✓ | ✓ |
  | 11 | Rotating Belt 2 | ✓ | ✓ | ✓ |
  | 13 | Spectrum Cycle | ✓ | — | ✓ |
  | 19 | Colorful Swirls 1 | ✓ | — | ✓ |
  | 23 | Colorful Swirls 2 | ✓ | — | ✓ |
  | 25 | Christmas | ✓ | — | — |
  | 26 | Spectrum Breathing | ✓ | ✓ | ✓ |
  | 28 | Spectrum Floating | ✓ | ✓ | ✓ |
  | 30 | Digital Rain | ✓ | — | — |
  | 31 | Monochromatic Lighting Up (reactive) | ✓ | ✓ | ✓ |
  | 36 | Crucifix (reactive cross) | ✓ | ✓ | ✓ |
  | 39 | Fingertip Rainbow (reactive) | ✓ | — | ✓ |

* Ids not listed are not offered by the UI.
* **Brightness**: 1–**200**. The UI overrides the list's 100 with `pl.default` = 200 (C10276), since the board name is not in `pl`. The UI warns at ≥ 60 % (raw ≥ 120): "unstable … disconnection may occur".
* **Speed**: 1–255, step 1. Direction (higher = faster?) is UNVERIFIED.
* **Colour**: 0–255 per channel.

### 8.2 Per-key RGB: supported on this model (UI effect 1)
| | bytes |
|---|---|
| GET request | `08 45 row col num` |
| GET response | RGB triples from byte 5 |
| SET request | `07 42 row col num RGB…` (num ≤ 8) |

* The UI reads 10 requests (each row at col 0 and col 8, 8 keys each) and writes the whole 5×14 map the same way after every colour change.
* `[0,0,0]` = unlit.
* Addressing is by matrix (row, col); `mad60he-layout.json` lists the populated positions.

### 8.3 Persist per-key RGB
`03 96 12`. The UI waits 100 + 500 ms afterwards.

### 8.4 Box / side light: present on this model (L14130; UI `extraLightItems: Pt`, C8133)
The UI shows the tabs "Back lamp" / "Box lamp" for MAD LIGHT 60HE.

* **GET**: `02 96 21` → `[3]` mode, `[4]` colorful (full-colour flag 0/1), `[5]` brightness, `[6]` speed, `[7..9]` R G B.
* **SET**: `03 96 21 mode colorful brightness speed R G B`.

| mode | name | brightness | speed | colour / full colour |
|---|---|---|---|---|
| 0 | Blackout (off) | — | — | — |
| 1 | Wavy | 0–160 | 0–40 | ✓ |
| 2 | Spectrum Cycle | 0–160 | 0–40 | — |
| 3 | Steady light | 0–160 | — | ✓ |
| 4 | Breath | 0–160 | 0–40 | ✓ |

All steps are 1.

---

## 9. Asynchronous data reports **[RESOLVED r2]** (L14396; UI handler C15163–15171)
* An input report starting with `02 96 14` is an event. `type` = byte 3 (enum `Gu`):

  | type | meaning | UI reaction |
  |---|---|---|
  | 1 | default layer changed | re-read §3.3 and §3.2 |
  | 2 | WASD swap toggled | re-read §3.2 |
  | 3 | macOS mode toggled | re-read §3.2 |
  | 4 | Win-lock toggled | re-read §3.2 |
  | 5 | NKRO toggled | re-read §3.2 |

* Other payload bytes are not used.

---

## 10. Physical layout and `axosome`

### 10.1 Layout **[RESOLVED r2]**
Source: `Ft`, C6320–6901 (61 keys). The full geometry (in key units and raw vendor px) and per-layer defaults are in `mad60he-layout.json`.

```
row0: c0 Esc | c1..c10 1..0 | c11 - | c12 = | c13 Backspace(2u)
row1: c0 Tab(1.5u) | c1..c10 Q..P | c11 [ | c12 ] | c13 \(1.5u)
row2: c0 Caps(1.75u) | c1..c9 A..L | c10 ; | c11 ' | c13 Enter(2.25u)          (c12 empty)
row3: c1 LShift(2u) | c3..c9 Z..M | c10 , | c11 . | c12 / | c13 RShift(3u)     (c0, c2 empty)
row4: c0 LCtrl | c1 LWin | c2 LAlt | c6 Space(6.25u) | c9 RAlt | c10 Menu | c11 RCtrl | c12 Fn   (1.25u mods; c3-5, c7-8, c13 empty)
```

* **Empty positions** (9): `2,12`, `3,0`, `3,2`, `4,3`, `4,4`, `4,5`, `4,7`, `4,8`, `4,13`.
* `borrowLayout` = `["2,12", "3,0", "3,2"]`. The UI excludes these from select-all and uses `2,12`'s actuation/RT entries as the "global" slider values.
* **Geometry**: x = round((l − 44)/53.5) to ¼u. The vendor drawing uses 50 px caps and ~53.5 px pitch.
  * The drawing shows a **2u Left Shift and 3u Right Shift**. This is the vendor's artwork; the physical board was not measured.
* **Mac Base (layer 2) differences**: LWin pos (4,1) = LAlt, (4,2) = LGui, RAlt pos (4,9) = RGui, Fn = MO(3).

### 10.2 `axosome` **[RESOLVED r2]**
* `custom.axosome` selects the entry with that `value` in the switch-parameter table `sl.default` (C10138). The table supplies travelMax/Min/Step, the upgrade steps, rtMax/Min/Step and `limit`.
* It is **not** a protocol field and not a unit factor.

| value | switch (vendor label) | travelMax | rtMax | limit |
|---|---|---|---|---|
| 3 | 磁玉 (Magnetic Jade) | 3.4 | 3.41 | 3.4 |
| 4 | 万磁 (Magneto) | 3.5 | 3.51 | 3.5 |
| 5 | 禾金磁珀 (Magnetic Amber) | 4.0 | 4.01 | 4.0 |
| **6** | **禾金磁珀PRO (Magnetic Amber Pro), this board** | **3.5** | **3.51** | **3.5** |
| 7 | TTC 天王电竞 (Uranus Esport) | 3.4 | 3.41 | 3.4 |
| 8 | HUANO 欧米茄 | 3.5 | 3.51 | 3.5 |
| 9 | 凯华磁神 (Kailh Magnetic God) | 3.4 | 3.41 | 3.4 |
| 10 | 禾金矮轴 (low-profile) | 3.3 | 3.31 | 3.3 |
| 11 | 佳达隆矮轴 (Gateron low-profile) | 3.3 | 3.31 | 3.3 |

* All entries share travelMin 0.1, travelStep 0.02 (0.01 in competitive mode), rtMin 0.02 (0.01 in competitive mode) and rtStep 0.01.
* `shaft: "magneto"` only selects the switch artwork (`/images/axis-body/magneto/*`).

---

## 11. Minimal session (Linux hidraw)

```text
open("/dev/hidrawN")    # descriptor begins 06 60 FF 09 61 (VID 373B PID 10F6, interface 1)

# 1. identify
write: 00 | 02 96 00 00 …            read: 02 96 00 .. .. 01 05 …   -> "1.05"
write: 00 | 02 96 13 00 …            read: 02 96 13 01 …            -> Win Base active
# 2. read all actuation points (6 requests; key A = row 2 col 1 = index 29)
write: 00 | 02 96 0D 00 00 00 18 0C  read: 02 96 0D 00 00 00 18 0C [8..31] -> index 24..35
           # index 29 is entry 5 -> bytes 18..19, e.g. 00 C8 = 2.00 mm
# 3. set A to 1.50 mm: patch entry 29 = 0x0096 in the 70-entry table, then send 7 chunks:
write: 00 | 03 96 0D 00 00 00 00 0B 01 <keys 0..10, 22 bytes>      ; wait 200 ms
write: 00 | 03 96 0D 00 00 00 0B 0B 00 <keys 11..21>
…   (off 0x16, 0x21, 0x2C, 0x37 with flashOp 00)
write: 00 | 03 96 0D 00 00 00 42 0B 02 <keys 66..69 + 14 zero bytes>; wait 200 ms
# 4. read back
write: 00 | 02 96 0D 00 00 00 18 0C  read: … bytes 18..19 = 00 96 -> 1.50 mm
# (shortcut, used by no UI and UNVERIFIED to persist:  03 96 03 02 01 00 96)
```

Reference transfer routine (pseudo-Python):

```python
def xfer(fd, payload: bytes, timeout=1.0) -> bytes:
    buf = payload.ljust(32, b"\0")
    os.write(fd, b"\x00" + buf)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not select.select([fd], [], [], max(0, deadline - time.monotonic()))[0]: break
        rep = os.read(fd, 64)[:32]
        if rep[0] == 0x00: continue
        if rep[:3] == b"\x02\x96\x14": handle_event(rep[3]); continue   # §9
        if rep[0] == 0xFF: raise IOError("unhandled")                    # UNVERIFIED
        if rep[0] == buf[0]: return rep
    raise TimeoutError
```

---

## 12. Still open

1. **Hardware behaviour of the unused paths**:
   * persistence of the single-key actuation/RT SETs (`03 96 03`/`07`)
   * reply to unknown commands (`0xFF`?)
   * whether bytes 1–2 are echoed
2. **Macro delay**: the firmware's decoding base (256 as the driver sends it, or Vial's 255) is unknown. Keep each delay < 255 ms, or chain several delay records, to be safe.
3. **DKS**: the actual trigger depths of E0..E3 (the UI shows fixed 1.0/3.0/3.0/1.0 mm), and the meaning of bit 8.
4. `rgbArea` semantics (the UI only reads it), and the direction of the effect `speed` value.
5. **Physical shift-key widths**: the vendor artwork shows 2u / 3u. Factory default actuation/RT values are not in the app.
6. **Optional probes the app never sends for this board**:
   * VIA `01` (protocol version), `11` (layer count)
   * V3 `02 96 24` (TravelInfo), `02 96 23` (profile)
