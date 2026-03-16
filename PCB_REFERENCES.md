# 🎯 REFERENCE DESIGNS & EXAMPLES - ECG PCB

## 📚 Tài Liệu Kỹ Thuật Chính Thức

### AD8232 (Analog Devices)

- **Datasheet:** https://www.analog.com/media/en/technical-documentation/data-sheets/ad8232.pdf
- **Evaluation Board:** https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/eval-ad8232.html
- **Layout Guidelines:** Page 18-20 của datasheet

**Key Points từ Reference Design:**

```
✓ Decoupling capacitor: 0.1uF + 10uF ở VCC
✓ Input filter: RC filter (10MΩ + 100nF)
✓ Lead-off resistor: 10MΩ
✓ Guard ring around analog traces
✓ Ground plane continuous
```

### ADS1115 (Texas Instruments)

- **Datasheet:** https://www.ti.com/lit/ds/symlink/ads1115.pdf
- **User Guide:** https://www.ti.com/lit/ug/sbau211/sbau211.pdf
- **Layout Example:** Figure 51, Page 38

**Key Points:**

```
✓ Bypass capacitor: 0.1uF ceramic ở VCC
✓ Input filtering: RC filter trước A0
✓ I2C pull-up: 4.7kΩ
✓ ADDR pin: kéo GND (address 0x48)
```

### ESP32-WROOM-32 (Espressif)

- **Datasheet:** https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf
- **Hardware Design Guidelines:** https://www.espressif.com/sites/default/files/documentation/esp32_hardware_design_guidelines_en.pdf

**Critical PCB Design Rules:**

```
✓ Antenna keepout zone: 15mm x 15mm
✓ Decoupling: 100nF + 10uF ở 3V3 pin
✓ EN pull-up: 10kΩ
✓ Boot mode: GPIO0, GPIO2 pull-up
```

---

## 🔧 KiCAD Libraries Cần Thiết

### Thư viện chính thức:

```
1. ESP32-WROOM-32:
   - Repo: https://github.com/espressif/kicad-libraries
   - Download: espressif-kicad-libraries.zip

2. AD8232:
   - Repo: https://github.com/SnapEDA/KiCad-Libraries
   - Search: "AD8232"

3. ADS1115:
   - Built-in KiCAD (ADC_Texas_Instruments)
```

### Cách import vào KiCAD:

```
Preferences → Manage Symbol Libraries → Add Library
Preferences → Manage Footprint Libraries → Add Library
```

---

## 🎨 Ví Dụ Layout Cụ Thể

### Layout 1: Compact (80x60mm - 2 layer)

```
┌─────────────────────────────────────────────────────┐
│  ┌─────────┐                                        │
│  │ J1      │  ECG Electrode Connector               │
│  │ ECG IN  │  (Screw Terminal 3-pin)                │
│  └─────────┘                                        │
│      │                                              │
│      ↓                                              │
│  ┌──────────┐    ┌──────────┐                      │
│  │ U1       │────│ U2       │                      │
│  │ AD8232   │    │ ADS1115  │                      │
│  │          │    │  16-bit  │                      │
│  └──────────┘    └──────────┘                      │
│      │   I2C         │                             │
│      └───────────────┴──────┐                      │
│                              │                     │
│                      ┌───────┴─────────┐           │
│                      │     U3          │           │
│                      │   ESP32-WROOM   │           │
│                      │                 │           │
│                      └─────────────────┘           │
│                                                     │
│  ┌──────┐  ┌──────┐                               │
│  │ J2   │  │ J3   │                               │
│  │ USB  │  │ UART │                               │
│  └──────┘  └──────┘                               │
└─────────────────────────────────────────────────────┘
```

**Ưu điểm:**

- ✅ Nhỏ gọn, tiết kiệm chi phí
- ✅ Dễ lắp vào vỏ hộp

**Nhược điểm:**

- ❌ Hơi chật chội
- ❌ Khó hàn thủ công

---

### Layout 2: Professional (100x100mm - 4 layer)

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   [ANALOG ZONE]          │      [DIGITAL ZONE]          │
│                          │                              │
│   ┌──────┐  ┌─────┐    │   ┌──────────┐              │
│   │ J1   │──│ U1  │─┐  │   │   U3     │              │
│   │ ECG  │  │AD8232│ │  │   │  ESP32   │              │
│   │ IN   │  └─────┘ │  │   │          │              │
│   └──────┘           │  │   └──────────┘              │
│                      ↓  │        │                     │
│   [RC Filter]    ┌─────┐│        │ I2C                │
│                  │ U2  ││        │                     │
│                  │ADS  ││   ┌────┴────┐               │
│   TP1 TP2 TP3    │1115 ││   │   U4    │               │
│    ○   ○   ○    └─────┘│   │  TFT    │               │
│                         │   │  LCD    │               │
│   GND Guard Ring        │   └─────────┘               │
│   ═══════════════       │                             │
│                         │   LED1 LED2 LED3            │
│   [POWER SECTION]       │    ○   ○   ○               │
│   ┌────────────┐        │                             │
│   │  Vin  3.3V │        │   ┌──────┐                 │
│   │  LDO  GND  │        │   │ J2   │                 │
│   └────────────┘        │   │ USB  │                 │
│                         │   └──────┘                 │
└──────────────────────────────────────────────────────────┘

Legend:
│ = Physical barrier/guard trace
═ = Ground guard ring
○ = Test point / LED
```

**Ưu điểm:**

- ✅ Phân vùng rõ ràng
- ✅ Nhiều test point
- ✅ Professional appearance
- ✅ Dễ debug
- ✅ Điểm cao nhất cho khóa luận

**Nhược điểm:**

- ❌ Chi phí cao hơn (~200k)
- ❌ Mất thời gian thiết kế

---

## 🌐 Open Source Projects Tham Khảo

### 1. **Protocentral HealthyPi**

- GitHub: https://github.com/Protocentral/healthypi-v4
- Features: ECG, SpO2, Temperature
- PCB: 4-layer, medical grade
- **Tham khảo:** Layout analog section, guard ring design

### 2. **OpenECG**

- GitHub: https://github.com/openecg
- Features: Reference ECG design
- **Tham khảo:** Schematic, filtering

### 3. **Upbeat Labs**

- Link: https://upskilltechs.com/product/upbeat-labs-heart-rate-sensor/
- Open source ECG
- **Tham khảo:** Compact layout

---

## 📊 PCB Stackup (4-layer)

### Recommended Stackup cho Medical Device:

```
┌─────────────────────────────────────┐
│  Layer 1 (Top Signal)               │ ← Components, Signal traces
│  ────────────────────────────────   │   Copper: 1oz (35μm)
│  Prepreg (0.2mm)                    │
├─────────────────────────────────────┤
│  Layer 2 (GND Plane)                │ ← Continuous GND
│  ═════════════════════════════════   │   Copper: 1oz
│  Core (1.0mm)                       │
├─────────────────────────────────────┤
│  Layer 3 (Power Plane)              │ ← 3.3V, 5V planes
│  ═════════════════════════════════   │   Copper: 1oz
│  Prepreg (0.2mm)                    │
├─────────────────────────────────────┤
│  Layer 4 (Bottom Signal)            │ ← Ground, Signal
│  ────────────────────────────────   │   Copper: 1oz
└─────────────────────────────────────┘
Total thickness: 1.6mm (standard)
```

### Layer Usage Guidelines:

**Layer 1 (Top):**

- Tất cả components
- Signal traces (tín hiệu ECG càng ngắn càng tốt)
- I2C traces (SDA, SCL)
- Power traces (ngắn)

**Layer 2 (GND):**

- Liền mạch, không cắt
- Via stitching dưới ICs
- Ground guard ring quanh analog zone

**Layer 3 (Power):**

- 3.3V plane cho ESP32
- 3.3V plane cho ADS1115
- Isolated nếu cần

**Layer 4 (Bottom):**

- Ground pour
- Overflow signal traces
- KHÔNG chạy tín hiệu dưới IC WiFi

---

## 💡 Tricks & Tips từ Engineers

### Trick 1: Via Stitching

```
┌─────────────────────────────┐
│  [IC]                       │
│   ○ ○ ○ ○                   │ ← Via fence
│   ○ [ADS1115] ○            │   (0.3mm, every 2mm)
│   ○ ○ ○ ○                   │
└─────────────────────────────┘
```

**Mục đích:** Ghép GND layer 1 ↔ layer 2, giảm impedance

### Trick 2: Differential Pair cho I2C

```
   SDA ═══════════════════
         (0.3mm spacing)
   SCL ═══════════════════

   Track width: 0.3mm
   Length matching: ±5mm
```

### Trick 3: RC Filter cho ECG Input

```
   [ECG] ──[10MΩ]──┬──[100nF]── [AD8232 IN+]
                   │
                  GND
```

**Mục đích:** Chống ESD, lọc nhiễu RFI

### Trick 4: Ferrite Bead cho Nguồn

```
   [3.3V] ──[Ferrite Bead 600Ω@100MHz]─┬─[IC VCC]
                                        │
                                    [0.1uF]
                                        │
                                       GND
```

---

## 🔍 PCB Manufacturers Recommendations

### Cho sinh viên (Budget-friendly):

**1. JLCPCB** (China → Vietnam)

- Website: https://jlcpcb.com
- Giá 2-layer 100x100mm: $2 + ship $10
- Giá 4-layer 100x100mm: $15 + ship $10
- Thời gian: 7-14 ngày
- ✅ Rẻ nhất
- ✅ Chất lượng ổn
- ✅ Assembly service (hàn linh kiện)
- ❌ Ship lâu

**2. PCBWay** (China → Vietnam)

- Website: https://www.pcbway.com
- Giá 2-layer: $5 + ship $12
- Giá 4-layer: $20 + ship $12
- Thời gian: 7-14 ngày
- ✅ Chất lượng tốt hơn JLCPCB
- ✅ Support tốt
- ❌ Đắt hơn JLCPCB

**3. PCBViet** (Việt Nam)

- Website: https://pcbviet.com
- Giá 2-layer: 80,000 VND
- Giá 4-layer: 250,000 VND
- Thời gian: 3-5 ngày
- ✅ Nhanh nhất
- ✅ Không lo customs
- ✅ Support tiếng Việt
- ❌ Đắt hơn order từ TQ

### Khuyến nghị cho khóa luận:

> **Order 2 lần:**
>
> 1. Lần 1: JLCPCB 2-layer → test code
> 2. Lần 2: PCBViet 4-layer → demo chính thức

---

## 📐 Design Rules cho JLCPCB

### Minimum Specs (Free):

```
Track width:       0.127mm (5mil)
Track spacing:     0.127mm (5mil)
Via diameter:      0.3mm
Via drill:         0.2mm
Soldermask:        Green (free), others +$10
Silkscreen:        White (free)
Surface finish:    HASL (free), ENIG +$20
```

### Recommended Specs (Dễ sản xuất):

```
Track width:       0.3mm (12mil)   ← Dùng cái này
Track spacing:     0.3mm (12mil)   ← Dùng cái này
Via diameter:      0.6mm
Via drill:         0.3mm
Min hole size:     0.3mm
```

---

## 🎓 Các Nội Dung Trình Bày Trong Khóa Luận

### Chương: "Thiết kế phần cứng"

#### 3.1 Sơ đồ khối hệ thống

```
[Diagram block]
```

#### 3.2 Sơ đồ nguyên lý

- Schematic đầy đủ
- Giải thích từng khối

#### 3.3 Thiết kế PCB

- **3.3.1 Lựa chọn số layer**
  - So sánh 2-layer vs 4-layer
  - Lý do chọn 4-layer
- **3.3.2 Phân vùng mạch**
  - Analog zone
  - Digital zone
  - Power section
  - Lý do phân vùng

- **3.3.3 Layout guidelines**
  - Tuân thủ IPC-2221
  - Reference design từ TI, ADI
  - Khoảng cách linh kiện
  - Track width calculation

- **3.3.4 Ground plane & Power plane**
  - Tại sao cần ground plane
  - Single point ground
  - Via stitching

- **3.3.5 Signal integrity**
  - Guard ring cho ECG signal
  - Differential routing cho I2C
  - EMI/EMC considerations

### Hình ảnh cần có:

1. ✅ Block diagram
2. ✅ Full schematic (A3 hoặc A2)
3. ✅ PCB layout 2D (top + bottom)
4. ✅ 3D render (nhiều góc độ)
5. ✅ PCB thật sau sản xuất
6. ✅ PCB đã hàn linh kiện
7. ✅ Hệ thống hoàn chỉnh trong vỏ

---

## 🏆 Tiêu Chí Đánh Giá (Theo GS/PGS thường chấm)

### Điểm 7-8: Đạt yêu cầu

- [x] Mạch hoạt động
- [x] Layout gọn gàng
- [x] Có silkscreen cơ bản

### Điểm 8-9: Khá - Giỏi

- [x] 4-layer PCB
- [x] Phân vùng rõ ràng
- [x] Test points
- [x] Professional silkscreen
- [x] Tuân thủ chuẩn IPC

### Điểm 9-10: Xuất sắc

- [x] Medical grade design
- [x] ESD/EMI protection
- [x] Isolation
- [x] Có vỏ hộp chuyên nghiệp
- [x] Tài liệu đầy đủ (BOM, Assembly drawing)
- [x] Đề cập chuẩn IEC 60601

---

## 📞 Support & Community

### KiCAD Forums:

- https://forum.kicad.info/

### ESP32 Hardware:

- https://esp32.com/

### EEVblog (PCB Design):

- https://www.eevblog.com/forum/

### Reddit:

- r/PrintedCircuitBoard
- r/esp32

---

## ✅ Final Checklist

**Trước khi order PCB lần CUỐI:**

1. [ ] Đã test trên breadboard → hoạt động 100%
2. [ ] Đã vẽ schematic đầy đủ → 0 error
3. [ ] Đã layout PCB → tuân thủ design rules
4. [ ] Đã chạy DRC → 0 error
5. [ ] Đã check Gerber viewer → chính xác
6. [ ] Đã in ra A4 1:1 → đặt linh kiện lên check
7. [ ] Đã có người review (bạn/thầy) → OK
8. [ ] Đã chuẩn bị tiền order + linh kiện

**Chi phí tổng:**

- PCB 4-layer: 200,000 VND
- Linh kiện: 500,000 VND
- Vỏ hộp: 100,000 VND
- **TỔNG: ~800,000 VND**

---

**Good luck với khóa luận! 🎓🚀**
