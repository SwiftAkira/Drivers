# LAMZU INCA (8K) — USB HID configuration protocol

Reverse-engineered by static analysis of LAMZU "Aurora" driver v1.0.37 (Electron + Vue + WebHID).
Nothing here was tested against hardware. Items marked **UNVERIFIED** are inferences not directly
proven by the code path the app uses for INCA.

Source abbreviations (all under `scratchpad/pretty/aurora/`):

| Tag | File | What it contains |
|---|---|---|
| `BY` | `index-BYu7rQ3i.js` | HID helper classes `sm` (71588–72662) and `Tie` (72664–), device discovery (81795–82100), enums, key tables, i18n strings |
| `C0` | `index-C0uPoN_v.js` | Main UI. All `e.cardData.hidHelper.*` calls |
| `CI` | `index-CIZ5Ivwu.js` | Firmware updater (used here only for hints about the device/MCU topology) |
| `ENV` | `dl/aurora/asar/web/Config/env-models.json` | Model DB (INCA entry) |

---

## 1. Device identification and interface selection

### 1.1 USB IDs (ENV, INCA entry)

| Device | VID | PID | Notes |
|---|---|---|---|
| INCA wired (mouse on USB cable) | 0x37B0 | 0x0009 | `VIDWired/PIDWired` |
| 1K receiver | 0x37B0 | 0x000F | `PIDWireless`, `_1KDongle`. Alternate PID 0x0003 (`_1KDongleIDVD`) |
| 8K receiver | 0x37B0 | 0x0010 | `PIDWireless4K8K`, `_8KDongle`. Alternate PID 0x0001 (`_8KDongleIDVD`). The updater also accepts 0x0001, 0x000C, 0x0010, 0x000E, 0x001B, 0x0015 as "8K receivers" (`_8KDongleExtra`), shared by several LAMZU models |
| Mouse bootloader | 0x37B0 | 0x000A | do not touch |
| 8K receiver bootloader | 0x37B0 | 0x0002 | do not touch |

The app also overrides the type by USB product string: if `productName` contains `"1K"` the device is
treated as a 1K receiver; contains `"8K"` → 8K receiver (BY 82018, 82027).

### 1.2 Which helper class

`BY 82013`: `f == 13652 ? Dr = new Tie : Dr = new sm`. 13652 = 0x3554 (a different OEM, the
"Compx" protocol). For VID 0x37B0 the app **always uses class `sm`** (BY 71588). `IsCompx` is never read
by the code. After opening, `Dr.isNewProtocol = (IsNewProtocol == "1")` (BY 82021) → **true for INCA**,
which means every per-profile command carries a profile byte (see §3).

The `get_Crc` checksum (`85 - sum & 0xFF`, BY 72789) and the 16-byte output-report framing belong to
`Tie` only. **The INCA (`sm`) protocol has no checksum.**

The UI page for INCA is driven by `v2()` (C0 17662); the Tie variant `A2()` is only for VID 0x3554
(C0 16886, 18076, 18129: `vid != 13652 ? v2() : A2()`).

### 1.3 Which HID interface (`findReadFileDeviceAndSetReportDevice`, BY 71641)

The app enumerates all WebHID devices with the VID/PID and picks two of them:

* **`setReportDevice`** (config channel): the first HID device whose collection has a feature report
  whose first item has `reportCount == 64`. On the 8K receiver this is **interface 2**
  (usage page 0xFFFF, usage 0x00, one 64-byte feature report, no report ID; descriptor
  `06 ff ff 09 00 a1 01 09 00 15 00 25 01 75 08 95 40 b1 02 c0`). All commands below go here via
  `sendFeatureReport(0, buf64)` / `receiveFeatureReport(0)`.
* **`readFileDevice`** (notification channel): the first HID device whose collection has an
  **input report with report ID 4**. On the receiver this is **interface 1** (the multi-report
  keyboard/consumer/vendor interface). The app subscribes to `oninputreport` on it (§6).

The hot-plug filter (BY 81810–81812) additionally only reacts to arrivals whose
`collections[0].featureReports[0].items[0].reportCount == 64`.

For the wired mouse (PID 0x0009) the same selection rule is applied; its interface numbering is
**UNVERIFIED** — select by descriptor, not by interface number.

**Linux mapping:** iterate `/sys/class/hidraw/hidraw*/device/`, match `HID_ID` VID/PID from `uevent`,
and parse `report_descriptor`:
* config node = descriptor containing a Feature main item (`b1 xx`) with Report Count 64 and no
  Report ID (on the 8K receiver it is exactly the 20-byte descriptor above);
* notify node = descriptor containing Report ID 4 (`85 04`) on an Input item.

### 1.4 Wired vs wireless; which model is behind the receiver

* Wired/wireless is decided purely from VID/PID (BY 81951–81990): PID 0x0009 → wired ("Mouse"),
  PID 0x000F/0x0003 → "1K", PID 0x0010/0x0001 (or product string containing "8K") → "8K".
* For a receiver the app asks receiver MCU #1 for the paired mouse's IDs (`GetDevicePID`, §4.1.4) and,
  if the returned PID is non-zero and differs from both the receiver PID and the VID, re-resolves the model by
  looking up an entry whose `PIDWired` equals it (BY 82018 → helper `o()` BY 81850). For an INCA receiver
  paired with an INCA it should return wired PID 0x0009 (**UNVERIFIED**, depends on firmware).
  Special case in the code: VID 0x37B0 + PID 0x001C is rewritten to VID 0x373E (irrelevant for INCA).

### 1.5 "Mouse online / asleep behind receiver"

There is no dedicated "link status" query. The app uses:

1. **Firmware-version probe of the mouse** (target 2, §4.1.1). If the reply decodes as `"0.0.0.0"`
   (no valid reply) the mouse is considered unreachable:
   * device list: `isMouseCanSet = (fw != "0.0.0.0" && fw != "00.00.00.00")` (BY 82014);
   * device card polls every 1500 ms (`C0 1167`, `se()` C0 1175–1188): if mouse FW is `"0.0.0.0"` it shows
     the receiver picture instead of the mouse (= mouse offline/asleep).
2. **Notification `04 06 xx`** on the input-report-4 channel (C0 18110): `04 06 00` = mouse
   disconnected from receiver (UI greys out), `04 06 01` = mouse (re)connected.

The exact reply a receiver gives for target 2 while the mouse sleeps (status byte value, timeout) is
**UNVERIFIED** — implement a bounded retry (§2.5) and treat failure as "offline".

---

## 2. Transport and framing

### 2.1 Buffer and transfer

* One 64-byte **feature report, report ID 0 (unnumbered)**, both directions.
* Write: `sendFeatureReport(0, req[64])` (BY 71694). Read: `receiveFeatureReport(0)` (BY 71701).
* No checksum, no sequence number. Request and reply share the same feature report: the reply is
  whatever the firmware has placed there by the time you issue GET_REPORT.

**Linux hidraw:**

```
uint8_t buf[65];
buf[0] = 0x00;                 // report number 0 (stripped by kernel, not sent on the wire)
memcpy(buf + 1, req, 64);
ioctl(fd, HIDIOCSFEATURE(65), buf);     // SET_REPORT(Feature, id 0), 64 data bytes on the wire

usleep(15000);                 // see §2.4

memset(buf, 0, 65);
buf[0] = 0x00;
ioctl(fd, HIDIOCGFEATURE(65), buf);     // returns 65; buf[0] = 0, buf[1..64] = reply payload
reply = buf + 1;
```

### 2.2 Request layout (64 bytes; "payload offset" = index into the 64-byte report)

| Offset | Name | Meaning |
|---|---|---|
| 0 | — | always 0x00 |
| 1 | — | always 0x00 |
| 2 | **target** | 0x02 = mouse (all normal config, wired *and* wireless). 0x00 = receiver MCU #0 (receiver FW version, receiver factory reset). 0x01 = receiver MCU #1 (paired-device VID/PID). See §2.6 |
| 3 | **len** | data length. For SET = number of meaningful bytes from offset 6. For GET = expected reply data length (app sends fixed values; copy them) |
| 4 | **class** | command class: 0x00 system/power, 0x01 sensor/DPI/report-rate, 0x02 LEDs/DPI colours, 0x03 buttons, 0x04 macros, 0xB0 bootloader |
| 5 | **cmd** | command; **bit 7 set = GET**, clear = SET (e.g. 0x08 set LOD / 0x88 get LOD) |
| 6.. | data | for per-profile commands data[0] (offset 6) = **profile number 1..3** |
| rest | | zero |

### 2.3 Reply layout

| Offset | Meaning |
|---|---|
| 0 | **status**: 0xA1 = OK/complete. The app also accepts 0x02 (see §2.5) |
| 1, 2 | not checked by the app (**UNVERIFIED**; probably 0 / target echo) |
| 3 | len echo (checked for battery: `== 0x02`) |
| 4 | class echo (checked only in bootloader paths: `reply[4] == 0xB0`) |
| 5 | cmd echo (checked for FW version 0x81, battery 0x83, profile 0x85, DPI table 0x81) |
| 6.. | data. For per-profile GETs offset 6 = profile echo and the value starts at offset 7 |

#### About `hidIndex` (why the JS indexes look off by one)

The JS reads replies as `a[k - hidIndex]`. `hidIndex` is auto-detected from the FW-version reply
(BY 71718–71760): if `a[6] == 0x81` the platform returned the report-ID byte in front (`hidIndex = 0`),
if `a[5] == 0x81` it did not (`hidIndex = 1`). **Every offset in this document is the payload offset,
i.e. the code's `k - 1`.** With Linux `HIDIOCGFEATURE` and a 65-byte buffer, the app's `a[k]` with
`hidIndex = 0` is exactly `buf[k]`, i.e. payload offset `k-1` is `buf[k]`.

### 2.4 Timing (BY 72589: `commonDelay = 15`)

| Situation | Delay between SET_REPORT and first GET_REPORT |
|---|---|
| Most commands | 15 ms |
| FW version (non-retry variants), EID, battery (`getBatPer`), DPI table GET (`getDPIStageInfo`) | 100 ms (DPI table GET also waits 15 ms after the retry loop) |
| Macro allocate / write / read | 30 ms (allocate also sleeps 30 ms *before* sending) |
| Macro size GET | 10 ms |
| Macro delete | 2 × 15 ms |
| First transfer after opening the device | 15 ms after `open()` |

UI-level pacing (not protocol-required but safe to copy): ~200 ms after most setters, 500 ms after a
profile switch (C0 16886), 1000 ms between mouse and receiver factory reset (C0 16741).

### 2.5 Retry / reply validation (`retrySetGet`, BY 72484)

```
reply = GET()
if reply[0] in (0xA1, 0x02): done
repeat up to 5 times:
    if reply[0] > 0xA1:                       # error/busy → resend the request
        sleep(d); SET(req); sleep(d); reply = GET(); if reply[0]==0xA1: done
    elif reply[0] < 0xA1:                     # not ready yet → keep polling
        repeat 30 times: sleep(d); reply = GET(); if reply[0]==0xA1: done
        if reply[0] in (0xA1, 0x02): break
        sleep(d); SET(req); sleep(d); reply = GET(); if reply[0]==0xA1: done
return reply   # the app does NOT treat exhaustion as an error
```

`d` = 15 ms (`retrySetGet`), or a caller-supplied delay (`retrySetGetWithDelay`, BY 72463; used with
20 ms for FW version, 30 ms for battery/DPI colours/macros). `retrySetGetForMacro` (BY 72477) is the
simplified form: up to 5 × (sleep 15, resend, sleep 15, GET) while `reply[0] != 0xA1`.

Recommended for a native implementation: after SET, poll GET every 15 ms until `reply[0] == 0xA1`
**and** `reply[4] == req[4]` **and** `reply[5] == req[5]` (guards against stale replies), resend after
~30 polls, give up after ~5 resends (≈2.5 s). Status 0x02 is accepted by the app but its meaning is
**UNVERIFIED** (it appears in bootloader checks `getBLFWVer`, BY 72503); status values > 0xA1 are
handled as "resend".

### 2.6 Target byte topology (UNVERIFIED interpretation)

The updater reads FW versions with targets 0 and 1 on the 8K receiver and compares them with the two
receiver hex files `..._8K_Dongle_305_APP_...` and `..._8K_Dongle_820_APP_...` (CI 3203, ENV
`Receiver4K8KFWFile_00/_01`); the mouse file is `INCA_Mouse_840_APP_...`. So most likely:
target 0 = receiver USB MCU ("305"), target 1 = receiver RF MCU ("820", likely nRF52820),
target 2 = the mouse (nRF52840) — forwarded over RF when on a receiver, local when wired. The app
sends target 2 for all mouse settings in both wired and wireless mode.

---

## 3. Command summary (INCA path)

All per-profile commands put the profile (1..3) in byte 6. "Used" = called by the INCA UI.

| Feature | Class | SET cmd | GET cmd | len (SET/GET) | Used for INCA | §   |
|---|---|---|---|---|---|---|
| System control (factory reset; enter bootloader) | 00 | 00 | — | 02 | yes (reset) | 4.10 |
| FW version | 00 | — | 81 | —/10 | yes | 4.1.1 |
| EID (edition / shell colour) | 00 | — | 82 | —/01 | yes | 4.1.2 |
| Battery | 00 | — | 83 | —/02 | yes | 4.1.3 |
| Active profile | 00 | 05 | 85 | 01/01 | yes | 4.2 |
| Sleep time | 00 | 07 | 87 | 03/03 | yes | 4.5 |
| Debounce | 00 | 08 | 88 | 02/02 | yes | 4.4 |
| Paired device VID/PID (target 1) | 00 | — | 8B | —/06 | yes (detection) | 4.1.4 |
| Reset profile | 00 | 0D | — | 01 | no | 4.10 |
| Scroll-wheel "HP" (bhop) | 00 | 19 | 99 | 04/04 | no (flag 0) | 4.11 |
| Rapid trigger (bhop) | 00 | 1A | 9A | 03/03 | no (flag 0) | 4.11 |
| Polling rate | 01 | 00 | 80 | 02/02 | yes | 4.3 |
| DPI stage table | 01 | 01 | 81 | 1A/0A | yes | 4.7.1 |
| Active DPI stage | 01 | 02 | 82 | 02/02 | yes | 4.7.2 |
| Angle snap | 01 | 04 | 84 | 02/02 | yes | 4.8 |
| LOD | 01 | 08 | 88 | 02/02 | yes | 4.6 |
| Motion sync | 01 | 09 | 89 | 02/02 | yes | 4.8 |
| Ripple control | 01 | 0A | 8A | 02/02 | yes | 4.8 |
| Hyper ("Competition") mode | 01 | 0B | 8B | 02/02 | yes | 4.8 |
| DPI max | 01 | — | 8C | —/02 | no | 4.1.6 |
| DPI X/Y split | 01 | 0D | 8D | 02/02 | yes | 4.7.5 |
| Sensor model | 01 | — | 8F | —/01 | yes | 4.1.5 |
| Tracking mode ("20K FPS") | 01 | 13 | 93 | 02/02 | yes (FW ≥ 0.0.0.16) | 4.8 |
| Angle tune | 01 | 14 | 94 | 02/02 | yes | 4.8 |
| Light effect | 02 | 00 | 80 | 5+n | no for INCA | 4.11 |
| DPI stage colours | 02 | 01 | 81 | 13/13 | yes | 4.7.6 |
| Light brightness | 02 | 02 | 82 | 03/03 | no for INCA | 4.11 |
| DPI indicator | 02 | 04 | 84 | 02/02 | no (flag 0) | 4.11 |
| Button mapping | 03 | 00 | 80 | 5+n | yes | 4.9 |
| Button combine | 03 | 01 | 81 | 02/02 | no (flag 0) | 4.11 |
| Macro allocate / size | 04 | 01 | 81 | 06/06 | yes | 4.9.4 |
| Macro delete | 04 | 02 | — | 02 | yes | 4.9.4 |
| Macro data write / read | 04 | 03 | 83 | 7+n | yes | 4.9.4 |
| Bootloader | B0 | … | … | | never | 4.12 |

---

## 4. Command details

Notation: `REQ` lists payload bytes from offset 0; omitted trailing bytes are 0x00. `RSP` gives payload
offsets of the reply. `PP` = profile (1..3).

### 4.1 Identification / status

#### 4.1.1 Firmware version — class 00 cmd 81 (BY 71718, 71740, 71751, 71762)

```
REQ  00 00 TT 10 00 81            TT = 02 mouse, 00 receiver MCU#0, 01 receiver MCU#1
RSP  [0]=A1 [5]=81 [6]=v0 [7]=v1 [8]=v2 [9]=v3     → string "v0.v1.v2.v3" (decimal)
```
* Delay 100 ms (`getFirmwareVersion`, `getDongleFirmwareVersion`, `getFirmwareVersionDeviceID`) or
  15 ms + `retrySetGetWithDelay(…,20)` (`getFirmwareVersionRetry(true)`, used when opening the
  config page, C0 18075).
* Invalid/no reply → `"0.0.0.0"` (= mouse unreachable, §1.5).
* Shipped versions (ENV): mouse 0.0.0.15, 8K receiver 0.0.0.15 (both MCUs).
* `GetUSBPID` (BY 72435, unused) sends `00 00 01 10 00 81` and reads a PID at RSP[10..11] (big-endian),
  suggesting the 16-byte FW-version reply also carries a PID at [10..11] — **UNVERIFIED**.
* The UI computes `fwNum = v0*10000 + v1*1000 + v2*100 + v3` for feature gating (C0 17743).

#### 4.1.2 EID (edition ID) — class 00 cmd 82 (BY 71773 `getEIDRetry`, 71784 `getEID`)

```
REQ  00 00 02 01 00 82
RSP  [0]=A1 [5]=82 [6]=eid
```
Selects artwork only. 0xFF is treated as 0 (C0 17670). `web/Config/INCA/` has a note file naming
`1 black, 2 grey, 0xFF orange, 3 white, 4 black-purple gradient, 5 pink`. No configuration effect.

#### 4.1.3 Battery — class 00 cmd 83 (BY 71823 `getBatPer`, 71834 `getBatPerRetry`)

```
REQ  00 00 02 02 00 83
RSP  [0]=A1 [3]=02 [5]=83 [6]=charging (1 = charging) [7]=percent (0..100)
```
* Valid only if `[0]==A1 && [3]==02 && [5]==83`, else app uses `[0,0]`.
* `getBatPerRetry` = 15 ms, `retrySetGetForMacro`, optionally `retrySetGetWithDelay(…,30)`.
* Wired: the app re-reads up to 3× at 500 ms intervals until charging==1 (BY 82016–82017).
* UI shows 99 % when charging and 100 (C0 17734).
* Also pushed asynchronously as notification `04 03 pct chg` (§6).

#### 4.1.4 Paired device VID/PID — target 1, class 00 cmd 8B (BY 72424 `GetDevicePID`, 72453 `GetDeviceVID`)

```
REQ  00 00 01 06 00 8B 02
RSP  [0]=A1 [8..9]=VID (big-endian) [10..11]=PID (big-endian)
```
Only meaningful on a receiver. Used to resolve the model (§1.4). Meaning of REQ[6]=02 (probably
"device slot 2 = mouse") and of RSP[6..7] **UNVERIFIED**. Note target byte is **01**.

#### 4.1.5 Sensor model — class 01 cmd 8F (BY 71912; UI C0 17685–17688)

```
REQ  00 00 02 01 01 8F
RSP  [0]=A1 [6]=model
```

| model | Sensor | LOD options | DPI max | DPI step |
|---|---|---|---|---|
| 0 | keep model-DB values (INCA: LOD 0.7/1/2, 30000) | | | 50 |
| 1 | PAW3395 | 1, 2 mm | 26000 | 50 |
| 2 | PAW3950 (expected for INCA) | 0.7, 1, 2 mm | 30000 | 50 |
| 4 | PAW3955 | 0.7, 1, 1.3, 1.5, 1.7 mm | 40000 (50000 for one ORCUS variant) | 1 |

The app only calls it because `IsSensorModelCmd=1`. Model 4 also switches LOD decoding (§4.6).

#### 4.1.6 DPI max — class 01 cmd 8C (BY 72182, **not used by the UI**)

```
REQ  00 00 02 02 01 8C
RSP  [6..7] = DPI max, big-endian      (UNVERIFIED — no profile byte, never called)
```

### 4.2 Active profile — class 00 cmd 05 / 85 (BY 71845, 71853; UI C0 16884, 17668)

```
SET  00 00 02 01 00 05 PP              PP = 1..3
GET  00 00 02 01 00 85
RSP  [0]=A1 [5]=85 [6]=profile (1-based)
```
* The INCA page offers **3 profiles** (`profileSelect` "1","2","3", C0 14902; the Tie path builds 4).
  Whether the firmware supports more is **UNVERIFIED**.
* The app loops `getProfileID` until it returns ≥ 1 (C0 17668–17669).
* After switching, the app waits 500 ms and re-reads everything.
* Profile changes made on the mouse are announced by notification `04 02` (§6).

### 4.3 Polling rate — class 01 cmd 00 / 80 (BY 71861, 71872; UI map `oM` C0 15696, setter `lM` C0 15759)

```
SET  00 00 02 02 01 00 PP RR
GET  00 00 02 02 01 80 PP
RSP  [6]=PP [7]=RR
```

| Hz | RR |
|---|---|
| 125 | 0x08 |
| 250 | 0x04 |
| 500 | 0x02 |
| 1000 | 0x01 |
| 2000 | 0x20 |
| 4000 | 0x40 |
| 8000 | 0x80 |

* On read the app rewrites RR 0x10 → 0x01 (1000 Hz) (BY 71878). Treat 0x10 as 1000 Hz.
* Offered values: wired `125;250;500;1000`; 8K receiver `500;1000;2000;4000;8000`;
  1K receiver `125;250;500;1000` (ENV, chosen in BY 82018).
* (The Tie map `nM`, C0 15642, uses 2000=0x10/4000=0x20/8000=0x40 — **not** for INCA.)
* Mouse-side changes are notified as `04 08 RR` (§6).
* UI tip text: "High Speed mode cannot be enabled simultaneously with 2K/4K/8K polling rates"
  (i18n `HyperModeTips`); the app does not enforce it.

### 4.4 Debounce time — class 00 cmd 08 / 88 (BY 71880, 71888; UI `cM` C0 15786)

```
SET  00 00 02 02 00 08 PP MS
GET  00 00 02 02 00 88 PP
RSP  [7]=MS
```
MS = milliseconds, UI range **0..15**, step 1 (`i < 0 || i > 15` rejected).
Note class 00 (not 01; class 01 cmd 08 is LOD).

### 4.5 Sleep time — class 00 cmd 07 / 87 (BY 71896, 71904; UI `pM` C0 15788, options `y2` C0 18042)

```
SET  00 00 02 03 00 07 PP SH SL        seconds, big-endian u16
GET  00 00 02 03 00 87 PP
RSP  [7..8] = seconds (big-endian)
```
* INCA options (`SleepTimeGrade`): 10 s, 30 s, 1 min, 5 min, 10 min, 30 min → 10, 30, 60, 300, 600, 1800.
* Read value 0x0000, 0xFF00 or 0xFFFF → UI shows sleep "disabled/unknown" (C0 17694).
  Whether writing 0 disables sleep is **UNVERIFIED**.

### 4.6 Lift-off distance — class 01 cmd 08 / 88 (BY 71920, 71928; UI `gM` C0 15795, decode C0 17715–17727)

```
SET  00 00 02 02 01 08 PP LL
GET  00 00 02 02 01 88 PP
RSP  [7]=LL
```
Encoding (`setLOD`):
* mm < 1 → `LL = (mm*10) | 0x80` → 0.7 mm = **0x87**
* 1 < mm < 2 → `LL = (floor(mm) << 4) | (mm*10 % 10)` → e.g. 1.3 = 0x13 (only 3955 sensors)
* otherwise → `LL = mm` → 1 mm = **0x01**, 2 mm = **0x02**

Decoding for INCA (sensor ≠ 4): `LL < 0x80 ? LL : (LL - 0x80)/10`.
Decoding for sensor model 4 only: `LL ≥ 0x80 ? (LL-0x80)/10 : (LL==1||LL==2) ? LL : ((LL>>4)*10 + (LL&0x0F))/10`.

INCA values: **0x87 (0.7 mm), 0x01 (1 mm), 0x02 (2 mm)**. Mouse-side LOD change → notification `04 07 LL`.

### 4.7 DPI

DPI values are raw **big-endian u16 counts-per-inch** (helper `Cr`, C0 17000) — no scaling.
UI range for INCA: min 50 (typed values < 50 are clamped to 50; sliders start at 100), max 30000
(`DPIMax`), step 50 (`DPIStep`, values are floored to a multiple of 50) (C0 15951–15982, 21572).
Stage count 1..5 (`DPIMaxStageNum=5`; the 6th option is removed, C0 18067; guard in `HM` C0 16003).
The wire format has room for 6 stages.

#### 4.7.1 DPI stage table — class 01 cmd 01 / 81 (BY 72140 `getDPIStageInfo`, 72158 `setDPIStageInfo`)

```
GET  00 00 02 0A 01 81 PP 06                        (app always asks with [7]=06; delay 100 ms)
RSP  [0]=A1 [5]=81 [6]=PP [7]=N (configured stage count)
     stage k (k = 0..N-1): [8+4k..9+4k] = X dpi (BE), [10+4k..11+4k] = Y dpi (BE)

SET  00 00 02 1A 01 01 PP N  X1h X1l Y1h Y1l  X2h X2l Y2h Y2l ...   (len byte fixed 0x1A = 26)
```
* Writing the table also **sets the number of stages** (N). There is no separate "stage count" command.
* When X/Y split is off the app writes Y = X for every stage (C0 15926–15934, 15983–16001).
* Invalid reply → app returns count 0 and empty lists.

#### 4.7.2 Active (current) stage — class 01 cmd 02 / 82 (BY 72168 `getActiveDPI`, 72176 `setActiveDPI`)

```
SET  00 00 02 02 01 02 PP SS            SS = 1-based stage index
GET  00 00 02 02 01 82 PP
RSP  [7]=SS
```
`setActiveDPI` does a single GET without the retry loop (BY 72176). The app always follows a stage
change with a full table write (C0 15983–16001). Mouse-side stage changes arrive as `04 01 …` (§6).

#### 4.7.3 Set one stage's DPI (`setActiveDPIValue`, BY 72190; UI `dr` C0 15941)

Read-modify-write:
1. GET table (`… 0A 01 81 PP 06`).
2. Copy `N*4` bytes from RSP[8..] into the SET buffer at offset 8.
3. Overwrite stage `S` (1-based) at offset `8 + (S-1)*4` with Xh Xl Yh Yl.
4. SET table with `[3]=1A [4]=01 [5]=01 [6]=PP [7]=N`.
5. Re-read table, active stage and colours (UI `Yr`, C0 16802).

#### 4.7.4 Change the stage count (`setDPIStageNum`, BY 72205; UI `HM` C0 16003)

1. SET active stage = `min(currentActive, newN)` (class 01 cmd 02).
2. GET table (`[7]=06`).
3. Copy `newN*4` bytes from RSP[8..]. If old N < newN, every stage whose X is 0 is filled with the
   default for its index: **400, 800, 1600, 3200, 6400, DPIMax(30000)** (X = Y).
4. SET table with N = newN.

#### 4.7.5 DPI X/Y split — class 01 cmd 0D / 8D (BY 71968, 71976; UI `k2` C0 15926)

```
SET  00 00 02 02 01 0D PP BB          BB = 1 independent X/Y, 0 linked
GET  00 00 02 02 01 8D PP
RSP  [7] == 1 → on
```
After toggling, the app rewrites the table (with Y = X when turning it off).

#### 4.7.6 DPI stage colours — class 02 cmd 01 / 81 (BY 72253, 72261; UI `E2` C0 16006, read `Yr` C0 16802)

```
SET  00 00 02 13 02 01 PP  R1 G1 B1  R2 G2 B2 ...     (len byte fixed 0x13 = 19 = 1 + 6*3)
GET  00 00 02 13 02 81 PP                              (retrySetGetWithDelay 30 ms)
RSP  stage k colour = [7+3k], [8+3k], [9+3k]  (R, G, B)
```
The app sends one RGB triple per currently configured stage. UI swatches: (208,0,103), (0,165,209),
(15,176,0), (255,255,255), (202,199,0) (C0 21415). Model-DB `DefaultDPIColors`
`ffff0000;ff0000ff;ff00ff00;ffff00ff;ffffff00;ffffffff` (ARGB: red, blue, green, magenta, yellow,
white) is **never read by the code** — likely the factory defaults, **UNVERIFIED**.

### 4.8 Sensor / performance toggles (all class 01, per profile)

Generic layout (BY 71984–72111):
```
SET  00 00 02 02 01 CMD PP VV
GET  00 00 02 02 01 CMD|80 PP
RSP  [7]=VV
```

| Feature | SET | GET | VV | UI label (EN) | Source |
|---|---|---|---|---|---|
| Angle snap | 04 | 84 | 0/1 | "Angle Snap" | BY 72000/72008, C0 15804 |
| Motion sync | 09 | 89 | 0/1 | "Motion Sync" | BY 72016/72024, C0 15920 |
| Ripple control | 0A | 8A | 0/1 | "Ripple Control" | BY 72096/72104, C0 15923 |
| Hyper mode | 0B | 8B | 0/1 | UI labels: 1 = "Competition Mode", 0 = "High-Speed Mode" (which state is faster/hungrier is ambiguous in the i18n text — UNVERIFIED) | BY 72080/72088, C0 15937 |
| Tracking mode | 13 | 93 | 0/1 | "20K FPS" | BY 72032/72040, C0 15802 |
| Angle tune | 14 | 94 | int8 | "Angle Tune" | BY 71984/71992, C0 15945 |

Details:
* Booleans are written as 1/0 and read as `VV == 1`.
* **Angle tune**: range −30..+30 degrees, step 1 (slider C0 22048). Encoding = two's-complement int8
  (`r > 0 ? r : 256 - |r|`, 0 → 0x00). Decode: `VV <= 30 ? VV : VV - 256`. Example −5 → 0xFB.
* **Hyper / tracking dependency** (C0 15937, 17732, 18131): tracking mode ("20K FPS") is only enabled
  in the UI while hyper mode is on; turning hyper mode off makes the app also SET tracking = 0.
  Enabling hyper mode shows a power-consumption confirmation. Notification `04 0D H T` reports both.
* **Tracking mode is only shown if mouse FW ≥ 0.0.0.16** (`fwNum >= 16 || _20KFPS`, C0 17744); INCA
  has `_20KFPS=0` and ships 0.0.0.15, so on 0.0.0.15 the app never reads or writes it.
  Behaviour of cmd 0x13/0x93 on older FW is **UNVERIFIED**.
* There is **no hyper-mode "time" parameter** in this protocol. (`Dt.PerformanceState/Performance`
  at BY 71570 are EEPROM offsets of the Tie protocol.)

### 4.9 Buttons

#### 4.9.1 Button IDs (`VM`, C0 16023; enum `due`, BY 81796)

| Button | ID |
|---|---|
| Left | 0x01 |
| Right | 0x02 |
| Middle | 0x03 |
| Back ("Backward") | 0x04 |
| Forward | 0x05 |
| DPI button | 0x14 (20) |

`KeyBindingSeq "01;02;03;04;05"` from the model DB is never used by the code.

#### 4.9.2 Get / set mapping — class 03 cmd 00 / 80 (BY 72374 `SetButton`, 72384 `GetButton`)

```
SET  00 00 02 LEN 03 00 PP BTN 00 TYPE PLEN P0 P1 ...      LEN = 5 + PLEN
GET  00 00 02 0F  03 80 PP BTN 00 FF   0A                   (app's read form: TYPE=FF, PLEN=10)
RSP  [9]=TYPE [10]=PLEN (UNVERIFIED, not read by app) [11..]=params
```
(The UI decoder `Pr`, C0 16483, reads RSP[9] type and RSP[11..14] params.)
Byte 8 is always 0x00 in both directions (meaning UNVERIFIED).

#### 4.9.3 Action types and parameters (encoder `SM`, C0 16198; type enum `pue`, BY 81801)

| Action | TYPE | PLEN | Params | Notes |
|---|---|---|---|---|
| Disabled | 0x00 | 0 | — | |
| Mouse button / button code | 0x01 | 1 | code | code: 1 L, 2 R, 3 M, 4 Back, 5 Fwd, 0x10 wheel up, 0x11 wheel down. Enum `due` also has 6/7/8 = Button6-8, 0x12 DPI up, 0x13 DPI down, 0x14 DPI loop up, 0x15 DPI loop down (decoder labels 0x15 "DPI button") — those extra codes are **UNVERIFIED** as firmware button codes |
| X-Click (fire / multi-click) | 0x02 | 5 | `01, button, count, intervalH, intervalL` | first byte always 01 (meaning UNVERIFIED); button = 1 (left) in UI; count u8 (UI default 3); interval ms BE16, UI rejects < 10 (default 10). Double-click preset = `01 01 02 00 64` |
| Keyboard key | 0x04 | 2 | `modifiers, usage` | modifiers bitmask (§4.9.5), usage = HID keyboard usage (page 0x07) |
| Consumer (media) key | 0x05 | 2 | `usageH, usageL` | HID consumer usage (page 0x0C), **big-endian** |
| Power keys | 0x06 | ? | ? | enum only, **UNVERIFIED** |
| DPI function | 0x07 | 1 or 5 | `01` stage up, `02` stage down, `06` cycle up (`07` cycle down, enum only); DPI lock: `05, dpiH, dpiL, dpiH, dpiL` (PLEN 5) | DPI lock value 50..30000, multiple of 50 (C0 16602) |
| Profile cycle | 0x08 | 1 | `03` (loop up) | not offered in INCA menus |
| FN / Tilt / BatButton / Pairing / Lighting | 0x09 / 0x0A / 0x0C / 0x0F / 0x13 | ? | ? | enum only, **UNVERIFIED** |
| Polling-rate cycle | 0x0D | 1 | `03` | DPI-button menu |
| LOD cycle | 0x0E | 1 | `03` | DPI-button menu |
| Macro, run N times | 0x10 | 3 | `idH, idL, N` | N = repeat count (u8) |
| Macro, repeat while held | 0x11 | 2 | `idH, idL` | |
| Macro, toggle (repeat until pressed again) | 0x12 | 2 | `idH, idL` | |
| Hyper/"game" mode cycle | 0x14 | 1 | `00` | DPI-button menu |

Menus the INCA UI offers (C0 15442–15570):
* Left/Right/Middle/Back/Forward (`KeyOptions`): left, right, middle, forward, back, wheel up,
  wheel down, X-click, media (player 0x0183, play/pause 0x00CD, next 0x00B5, previous 0x00B6,
  vol+ 0x00E9, vol− 0x00EA, mute 0x00E2, stop 0x00B7), shortcuts (calculator 0x0192,
  my computer 0x0194, WWW home 0x0223, e-mail 0x018A, brightness up 0x006F, brightness down 0x0070),
  DPI lock, DPI cycle, DPI up, DPI down, disable, keyboard key+modifiers, macro.
* DPI button (`KeyOptions2`): DPI cycle, LOD cycle, polling-rate cycle, hyper-mode cycle.

Other consumer codes in the enum `gue` (BY 81802): eject 0x00B8, explorer 0x0196, WWW refresh 0x0227,
select app 0x01A3, copy 0x021B, cut 0x021C, paste 0x021D.

App safety rule worth copying (C0 16342 `Gr`): it refuses a change that would leave **no button
mapped to left click** (it counts buttons whose TYPE=01 code=01).

#### 4.9.4 Macros (class 04)

Storage: up to **30 macros, IDs 1..30** (`macroMaxCount`, C0 15268), addressed by a big-endian u16 ID;
each macro is a byte stream of variable size (u32). Total flash capacity is **UNVERIFIED**.

| Op | Request | Notes / source |
|---|---|---|
| Allocate | `00 00 02 06 04 01 IDh IDl S3 S2 S1 S0` | size = total stream length (BE32). Sleep 30 ms before and after; success = RSP[0]==A1; app retries up to 5×. BY 72307 |
| Write chunk | `00 00 02 LEN 04 03 IDh IDl O3 O2 O1 O0 CL D0 … D(CL-1)` | LEN = 7 + CL; offset BE32; app uses CL ≤ **51** (fills the 64-byte report). Sleep 30 ms, `retrySetGetWithDelay(30)`. BY 72319, UI `Gv` C0 17158 |
| Get size | `00 00 02 06 04 81 IDh IDl` | RSP[8..11] = size BE32; 0 = empty slot. 10 ms delay. BY 72333 |
| Read chunk | `00 00 02 LEN 04 83 IDh IDl O3 O2 O1 O0 CL` | LEN = 7 + CL; app uses CL = 50; data at RSP[13 .. 13+CL-1]. BY 72343, UI `g2` C0 17190 |
| Delete | `00 00 02 02 04 02 IDh IDl` | BY 72414, UI `Ev` C0 17072 |

Write sequence: allocate(ID, len) → for each 51-byte chunk write(ID, offset, chunk) → read back.
After deleting a macro the app rebinds Right/Middle/Back/Forward buttons that pointed to it to their
default functions (C0 17085–17099) — the firmware evidently does not do this itself (**UNVERIFIED**).

**Event encoding** (encoder `Sv` C0 17014, decoder `Bv` C0 17212). No header, no terminator:

| Event | Bytes | Meaning |
|---|---|---|
| Key down | `02 usage` | HID keyboard usage |
| Key up | `03 usage` | |
| Modifier down | `09 mask` | modifier bitmask (§4.9.5) |
| Modifier up | `0A mask` | |
| Mouse buttons | `01 state` | **state** = bitmask of buttons held *after* the event: 0x01 L, 0x02 R, 0x04 M, 0x08 Back, 0x10 Fwd. Press = old \| bit, release = old − bit |
| Wheel | `10 dir` | 0x01 up, 0xFF down |
| Delay | `20 d` | 0–255 ms |
| Delay | `21 dH dL` | 256–65535 ms |
| Delay | `22 d2 d1 d0` | up to 2^24−1 ms |
| Delay | `23 d3 d2 d1 d0` | up to 2^32−1 ms |

Any other leading byte makes the app's decoder reject the macro. The UI records modifier keys
(Ctrl/Shift/Alt/Win, left or right) as 09/0A and all other keys as 02/03; delays can be "recorded",
"default" (UI default 50 ms) or "none".

Binding a macro to a button: TYPE 0x10/0x11/0x12 (§4.9.3). Strings: "Run the ID macro 0xNN times",
"Repeat the ID macro until button is released", "Repeat the ID macro until button is pressed again"
(BY 84140–84142).

#### 4.9.5 Keyboard tables

Modifier bitmask (`mue`, BY 81803): 0x01 LCtrl, 0x02 LShift, 0x04 LAlt, 0x08 LGUI, 0x10 RCtrl,
0x20 RShift, 0x40 RAlt, 0x80 RGUI. The button-binding UI only offers the left four
(`modifierSettings`, C0 15394).

Key usages: the app's name→code table (`ffe`, BY 82558–82866) is exactly the standard USB HID
Keyboard/Keypad page (0x07) usages: A=0x04 … Z=0x1D, 1=0x1E … 0=0x27, Enter 0x28, Esc 0x29,
Backspace 0x2A, Tab 0x2B, Space 0x2C, F1 0x3A … F12 0x45, PrintScreen 0x46, ScrollLock 0x47, Pause 0x48,
Insert 0x49 … arrows 0x4F–0x52, NumLock 0x53, keypad 0x54–0x63, LCtrl 0xE0, LShift 0xE1, LAlt 0xE2,
RCtrl 0xE4, RShift 0xE5, RAlt 0xE6. (101 entries; no F13+ or GUI as a plain key.)

### 4.10 Reset

#### Factory reset — class 00 cmd 00, data `C0 xx` (BY 72120–72139; UI `Kv` C0 16741)

```
mouse:      00 00 02 02 00 00 C0 01
receiver:   00 00 00 02 00 00 C0 00     (target 0)
```
* Wired: send the mouse packet only (no reply is read), then leave the config page.
* Wireless (INCA path): send the mouse packet, wait **1000 ms**, send the receiver packet
  (`defaultDevice4_7_02_c0(0)`); no replies read. (LM20 models use `… C0 01` to target 0 instead.)
* Meaning of the second data byte (01 / 00 / 02 variants exist) is **UNVERIFIED**.
* UI text: "After clicking OK, the mouse will be restored to its factory default settings."

#### Reset one profile — class 00 cmd 0D (BY 72112, **not used by the UI**)

```
00 00 02 01 00 0D PP          (UNVERIFIED)
```

### 4.11 Present in the protocol but disabled / not exposed for INCA

| Feature | Why off for INCA | Layout |
|---|---|---|
| Button combine | `CombineKeyEnable=0` | `00 00 02 02 03 01 PP BB` / GET `… 03 81 PP` → RSP[7]==1 (BY 71936/71944) |
| DPI indicator | `DPIIndicatorEnable=0` | `00 00 02 02 02 04 PP BB` / GET `… 02 84 PP` → RSP[7]==1 (BY 71952/71960) |
| Rapid trigger ("SpeedMode") | `bhopWheelEnable=0` | `00 00 02 03 00 1A PP L R` (L/R = 1 enable left/right) / GET `00 00 02 03 00 9A PP` → RSP[7]=L, RSP[8]=R (BY 72064/72072) |
| Scroll-wheel mode + response time | `bhopWheelEnable=0` | `00 00 02 04 00 19 PP MODE TH TL` MODE 0 off / 2 scroll-up / 3 scroll-down, T = 200/400/600/800/1000 ms BE16 / GET `… 04 00 99 PP` → RSP[7]=MODE, RSP[8..9]=T (BY 72048/72056) |
| Sensor offset, dynamic sensitivity ("AdaDPI") | `SensorOffsetEnable=0`, `DynamicSensivtyEnable=0` | UI-only in this build — **no HID commands exist** |
| Lighting (mouse / receiver LED) | `LightEnablePID=""` → `isLightEnable=false`; the receiver-LED panel is only shown for "THORN V2(54H20)" or PID 0x48 | see below |

Lighting layout, for completeness only (**UNVERIFIED on INCA**; BY 72271–72306, UI C0 17443–17490,
15766, 15911, 16822). Target = 2 when wired, `LightDeviceID` (= 1, receiver MCU #1) when wireless:
```
SET effect   00 00 TT LEN 02 00 PP ZONE EFFECT 00 SPEED R G B ...   LEN = 5 + colour bytes
GET effect   00 00 TT LEN 02 80 PP ZONE                              RSP [7]=zone [8]=effect [9]=? [10]=speed [11..]=colours
SET bright   00 00 TT 03 02 02 PP Z BRIGHT       BRIGHT 0..255 (UI % * 255/100); Z = 1 wired / 0 wireless
GET bright   00 00 TT 03 02 82 PP Z              RSP [8]=brightness
```
ZONE 0xFF = all; receiver status-LED zones 5/3/4 ("Light1/2/3") take EFFECT 6 battery, 10 polling rate,
11 DPI, 12 signal. Effect codes in the generic list: 0 off, 2 rainbow wave, 3 rainbow breath,
4 static, 5 breathing, 6 battery, 7 rainbow, 9 marquee, 10 polling rate, 11 DPI, 12 signal,
14 click feedback, 15 move feedback. Model-DB `LightEffects "1,2,4,5"` (Spectrum, Wave, Static,
Breathing) is only consumed by the Tie (`A2`) page, not by INCA's page.

### 4.12 Never send (bricking / identity risk)

* Class 00 cmd 00 data `B0` (`enterBL`, BY 72498) — reboots into bootloader (PID 0x000A / 0x0002).
* Class 0xB0 commands (BL version 0x80, erase 0x01, program, verify 0x83, exit 0x04) — firmware
  update path, payload XOR 0x55 (BY 71511–71550, 72503–72590).
* Target 0 class 00 cmd 10 (`SetUSB1KIDVDPID`, BY 72445) — rewrites the receiver's USB VID/PID.

---

## 5. What the app reads when opening the INCA page (`fA` C0 18064 → `v2` C0 17662)

1. Open notify node, subscribe to input reports.
2. Mouse FW version (target 2, retry).
3. GET profile (repeat until ≥ 1) → PP.
4. GET EID (retry).
5. GET sensor model → adjust DPI max / LOD list.
6. GET polling rate, debounce, sleep time, LOD, angle tune, angle snap (all with PP).
7. GET DPI X/Y split, motion sync, ripple control, hyper mode (with PP).
8. GET battery.
9. If FW ≥ 0.0.0.16: GET tracking mode.
10. GET button mapping for Left, Right, Middle, Back, Forward, DPI (`o0`, C0 16850).
11. For macro IDs 1..30: GET size, then read chunks (`hv`/`g2`, C0 16873, 17190).
12. GET DPI table, active stage, DPI colours (`Yr`, C0 16802).

Wireless and wired use the same sequence; only the polling-rate option list differs.

---

## 6. Notifications — input report ID 4 on the notify node (`v0`, C0 18099)

hidraw `read()` on the notify node returns `04 b1 b2 …` (report ID first). The app's handler is
attached to the whole interface and prepends "04" to *every* input report regardless of its ID, so
filter on report ID 4 yourself. Byte numbering below includes the ID at [0].

| Bytes | Meaning | App reaction |
|---|---|---|
| `04 01 SS XH XL YH YL` | DPI stage changed on the mouse: stage SS (1-based), X/Y DPI BE | update display |
| `04 02 …` | profile changed on the mouse | GET profile and re-read everything |
| `04 03 PCT CHG` | battery % and charging flag (1 = charging) | ignored if PCT == 0 |
| `04 06 00` / `04 06 01` | mouse disconnected / reconnected (receiver) | disable / enable UI |
| `04 07 LL` | LOD changed (encoding §4.6) | |
| `04 08 RR` | polling rate changed (map §4.3) | |
| `04 0D H T` | hyper mode H (0/1) and tracking mode T (0/1) changed | label: H ? (T ? "20K FPS" : "Competition") : "High-Speed" |
| `04 A5 01 01` | disconnect (only handled for "Orcus V2 (LM20)") | — |

Report ID 5 (usage page 0xFFFF) is not interpreted by the app (**UNVERIFIED** purpose).
Whether `04 02` carries the new profile number in byte 2 is **UNVERIFIED** (the app re-queries).

---

## 7. Defaults

The app never writes a full default configuration; factory reset is done by firmware (§4.10).
Values the app itself supplies:

| Item | Value | Source |
|---|---|---|
| DPI stages filled when increasing the stage count | 400, 800, 1600, 3200, 6400, DPIMax (X=Y) | BY 72222–72249 |
| UI placeholder DPI stages (5-stage model) before first read | 400, 800, 1600, 3200, 30000 | C0 17744–17778 |
| DPI lock default | 400 | C0 15388 |
| X-click defaults | count 3, interval 10 ms | C0 15389–15390 |
| Double-click preset | `02 05 01 01 02 00 64` (2 clicks, 100 ms) | C0 16210 |
| Macro repeat count default | 1 | C0 15414 |
| Macro default delay | 50 ms | C0 14878 (`delayDefaultValue`) |
| Model-DB `DefaultDPIColors` | red, blue, green, magenta, yellow, white | ENV (unused by code; **UNVERIFIED** as FW defaults) |
| Model-DB `DefaultSleepTime` | "1" | ENV (unused by code) |

Pairing is purely physical for INCA (no HID command): unplug receiver, hold left+middle+right for
3 s until the pairing LED flashes, plug receiver in near the mouse (i18n `PairStep1–3`, BY 84188–84190;
the Tie-only `enterPairMode` is not used).

---

## 8. Minimal session example (8K receiver, Linux hidraw)

All requests are 64 bytes (listed bytes then zero-padded), sent as `HIDIOCSFEATURE(65)` with a leading
0x00; replies read with `HIDIOCGFEATURE(65)` and taken from `buf[1..64]`.

```
# 0. open
cfg = open("/dev/hidrawN")   # node with 64-byte feature report, usage page 0xFFFF
ntf = open("/dev/hidrawM")   # node with input report ID 4 (optional, for 04 xx events)
sleep 15 ms

# 1. identify
SET 00 00 00 10 00 81                         # receiver MCU#0 FW version
     sleep 100 ms; GET → A1 .. .. 10 00 81 00 00 00 0F ...   => "0.0.0.15"
SET 00 00 01 06 00 8B 02                      # paired device VID/PID via receiver MCU#1
     sleep 15 ms;  GET → A1 .. .. .. .. 8B .. .. 37 B0 00 09 => paired INCA (wired PID 0x0009)  [UNVERIFIED values]
SET 00 00 02 10 00 81                         # mouse FW version; "0.0.0.0"/no A1 → mouse asleep/offline
     sleep 100 ms; GET → A1 .. .. 10 00 81 00 00 00 0F
SET 00 00 02 01 01 8F                         # sensor model → [6] = 02 (PAW3950) expected

# 2. status
SET 00 00 02 02 00 83                         # battery
     GET → A1 .. .. 02 00 83 00 4B            => not charging, 75 %
SET 00 00 02 01 00 85                         # active profile
     GET → A1 .. .. 01 00 85 01               => PP = 1

# 3. read DPI
SET 00 00 02 0A 01 81 01 06                   # DPI table, profile 1
     sleep 100 ms; GET → A1 .. .. .. 01 81 01 05 01 90 01 90 03 20 03 20 06 40 06 40 0C 80 0C 80 75 30 75 30
                       => 5 stages: 400, 800, 1600, 3200, 30000 (X=Y)
SET 00 00 02 02 01 82 01                      # active stage
     GET → A1 .. .. .. 01 82 01 02            => stage 2

# 4. set stage 3 to 1200 DPI (read-modify-write the whole table)
SET 00 00 02 1A 01 01 01 05 01 90 01 90 03 20 03 20 04 B0 04 B0 0C 80 0C 80 75 30 75 30
     sleep 15 ms; GET until [0]==A1 (and [4]==01, [5]==01)

# 5. read back
SET 00 00 02 0A 01 81 01 06
     sleep 100 ms; GET → … [7]=05, stage 3 bytes [16..19] = 04 B0 04 B0   ✓
```

More ready-made requests (profile 1):

| Action | Request bytes |
|---|---|
| Switch to profile 2 | `00 00 02 01 00 05 02` |
| Make stage 3 active | `00 00 02 02 01 02 01 03` |
| Polling 8000 Hz | `00 00 02 02 01 00 01 80` |
| LOD 0.7 mm | `00 00 02 02 01 08 01 87` |
| Sleep 60 s | `00 00 02 03 00 07 01 00 3C` |
| Debounce 2 ms | `00 00 02 02 00 08 01 02` |
| Motion sync on | `00 00 02 02 01 09 01 01` |
| Angle tune −5° | `00 00 02 02 01 14 01 FB` |
| DPI colours (5 stages R,B,G,M,Y) | `00 00 02 13 02 01 01 FF 00 00 00 00 FF 00 FF 00 FF 00 FF FF FF 00` |
| Read Forward-button mapping | `00 00 02 0F 03 80 01 05 00 FF 0A` |
| Forward → Ctrl+C | `00 00 02 07 03 00 01 05 00 04 02 01 06` |
| Forward → Volume up | `00 00 02 07 03 00 01 05 00 05 02 00 E9` |
| DPI button → DPI cycle | `00 00 02 06 03 00 01 14 00 07 01 06` |
| Back → DPI lock 800 | `00 00 02 0A 03 00 01 04 00 07 05 05 03 20 03 20` |
| Factory reset (mouse, then 1 s later receiver) | `00 00 02 02 00 00 C0 01`, then `00 00 00 02 00 00 C0 00` |

---

## 9. Open questions / UNVERIFIED summary

1. Reply bytes [1], [2] and the meaning of status 0x02 and of statuses > 0xA1.
2. What the receiver returns for target-2 requests while the mouse is asleep (status/timeout), and
   whether target 2 vs target 0 both work when wired (the app uses 2 for settings and 0 only in the updater).
3. Target-0/1/2 → MCU mapping (305 / 820 / 840) is inferred from firmware file names.
4. Profile count: the UI exposes 3; firmware maximum unknown.
5. `GetDevicePID` request byte [6]=02 and reply bytes [6..7]; the exact VID/PID an INCA receiver reports.
6. The `len` byte for GETs: the app sends fixed values (e.g. DPI table GET uses 0x0A although the
   reply carries up to 26 data bytes) — copy the app's values verbatim rather than computing them.
7. Tracking mode ("20K FPS") on FW < 0.0.0.16; whether INCA supports it at all (`_20KFPS=0`).
8. Sleep-time value 0 / 0xFFFF semantics; factory-default values (DPI colours, sleep, DPI stages).
9. Button codes 6–8 and 0x12–0x15 for TYPE 0x01, and TYPEs 0x06/0x09/0x0A/0x0C/0x0F/0x13 parameters.
10. Macro flash capacity; maximum stream length per macro; whether firmware unbinds buttons when a
    macro is deleted.
11. Wired-mode (PID 0x0009) interface layout — select nodes by descriptor as in §1.3.
12. Whether notifications other than those listed exist (e.g. report ID 5), and whether `04 02`
    carries the profile number.
13. Lighting commands on INCA (no LED is exposed by the app for this model).
