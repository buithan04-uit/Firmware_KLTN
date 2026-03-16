# Thiết Kế PCB Chuyên Nghiệp Cho Khóa Luận - Hệ Thống ECG

> **Dành cho:** Khóa luận tốt nghiệp - Hệ thống thu thập dữ liệu sinh học
> **Mục đích:** Đạt chuẩn công nghiệp, an toàn, ít nhiễu

---

## ⚠️ VẤN ĐỀ KHI LINH KIỆN HÀN QUÁ SÁT NHAU

### 1. **Nhiễu Crosstalk (Nhiễu xuyên âm)**

- ❌ **Vấn đề:** Tín hiệu từ chân này nhảy sang chân khác
- 📊 **Ảnh hưởng đến ECG:** Tín hiệu ECG (~1mV) rất yếu, dễ bị nhiễu từ tín hiệu số (3.3V)
- ✅ **Giải pháp:** Cách linh kiện analog và digital tối thiểu **5mm**

### 2. **Nhiệt độ tập trung**

- ❌ **Vấn đề:** Linh kiện quá gần → nhiệt tích tụ → giảm tuổi thọ
- 📊 **Ảnh hưởng:** ESP32 nóng → ADS1115 nóng → drift (trôi tín hiệu ADC)
- ✅ **Giải pháp:** Để khoảng trống, thêm via tản nhiệt

### 3. **Khó sửa chữa, debug**

- ❌ **Vấn đề:** Không thể đo điện áp test point, không thể thay linh kiện lỗi
- 📊 **Ảnh hưởng khóa luận:** Giáo viên hướng dẫn khó đánh giá tính khả thi
- ✅ **Giải pháp:** Thêm test points, khoảng cách tối thiểu 2mm giữa các IC

### 4. **Nguy cơ chập mạch**

- ❌ **Vấn đề:** Thiếc hàn chảy sang chân bên cạnh
- 📊 **Ảnh hưởng:** Chập nguồn → hỏng IC → phải làm lại PCB
- ✅ **Giải pháp:** Tuân thủ chuẩn IPC-2221 (khoảng cách tối thiểu)

---

## 📐 CHUẨN KHOẢNG CÁCH LINH KIỆN (IPC Standards)

### Chuẩn IPC-2221 (International PCB Standard)

| Khoảng cách                      | Giá trị tối thiểu | Khuyến nghị cho khóa luận |
| -------------------------------- | ----------------- | ------------------------- |
| **IC to IC**                     | 3mm               | **5-8mm** (dễ hàn, debug) |
| **SMD Resistor to SMD Resistor** | 0.5mm             | **1-2mm**                 |
| **Through-hole to Through-hole** | 2mm               | **3-5mm**                 |
| **Track to Track** (cùng layer)  | 0.2mm (6/6 mil)   | **0.3-0.5mm** (12/12 mil) |
| **Track to Pad**                 | 0.2mm             | **0.3mm**                 |
| **Analog to Digital ground**     | 3mm               | **5-10mm** + guard ring   |

### Khoảng cách cụ thể cho dự án của bạn:

```
┌─────────────────────────────────────────────────────────────┐
│                    PCB LAYOUT ĐỀ XUẤT                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [Analog Zone]          [Digital Zone]                     │
│                                                             │
│  AD8232 ──(5mm)── ADS1115    |    ESP32                   │
│    │                          |     │                       │
│  [Connector]                 |   [WiFi]                    │
│   ECG Pads                   |   [LVGL]                    │
│                              |                             │
│  <── Càng xa càng tốt ───>   |                             │
│                                                             │
│  Guard Ring (vòng bảo vệ) ───┘                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🎯 THIẾT KẾ PCB CHUYÊN NGHIỆP CHO KHÓA LUẬN

### **Mức 1: CƠ BẢN (Đạt 7-8 điểm)**

#### 1. **Phân vùng rõ ràng**

```
┌──────────────────────────────────────┐
│  INPUT (Analog)  │  PROCESSING  │  OUTPUT  │
│                  │              │          │
│  AD8232          │  ESP32       │  LCD     │
│  ADS1115         │  Memory      │  WiFi    │
│  Sensors         │              │          │
└──────────────────────────────────────┘
```

#### 2. **Quy tắc bố trí:**

- ✅ Luồng tín hiệu từ trái → phải (Input → Processing → Output)
- ✅ IC quan trọng ở giữa, connector ở mép
- ✅ Nguồn ở góc, phân phối đều
- ✅ Tụ bypass (0.1uF) sát chân VCC của mỗi IC (<5mm)

#### 3. **Ground plane:**

- ✅ Layer dưới làm GND hoàn toàn (continuous ground plane)
- ✅ Không cắt ground plane dưới IC analog
- ✅ Nối ground analog và digital tại **1 điểm duy nhất** (star point)

---

### **Mức 2: NÂNG CAO (Đạt 8-9 điểm)**

#### 1. **PCB 4 layer** thay vì 2 layer

```
Layer 1 (Top):    Signal + Components
Layer 2:          GND Plane (liền mạch)
Layer 3:          Power Plane (3.3V, 5V)
Layer 4 (Bottom): Signal + GND
```

**Ưu điểm:**

- 🔹 Giảm nhiễu EMI xuống 50-70%
- 🔹 Impedance ổn định hơn
- 🔹 Tản nhiệt tốt hơn
- 🔹 Thể hiện tính chuyên nghiệp

#### 2. **Differential Routing cho I2C**

```
     SDA ═══════════════════════ (đường đôi, cách đều)
     SCL ═══════════════════════

     ✓ Độ dài 2 đường bằng nhau (±5mm)
     ✓ Khoảng cách giữa 2 đường: 0.3mm
```

#### 3. **Guard Ring/Trace** cho tín hiệu ECG

```
   GND Guard Ring
   ┌───────────────────────┐
   │  AD8232    ADS1115    │  ← Bảo vệ khỏi nhiễu digital
   │  OUTPUT ──> A0        │
   │                       │
   └───────────────────────┘

   ✓ Vòng GND bao quanh vùng analog
   ✓ Nhiều via kết nối GND layer 2
```

#### 4. **Shielding cho ECG signal trace**

```
   GND ═════════════════════════ (Layer 1)
       ─────────────────────────  (Layer 2 - Signal)
   GND ═════════════════════════ (Layer 3)

   ✓ Đường tín hiệu ECG được "bọc" bởi GND 2 bên
```

---

### **Mức 3: CHUYÊN NGHIỆP (Đạt 9-10 điểm + Giải thưởng)**

#### 1. **Isolation (Cách ly) giữa phần analog và digital**

```
┌──────────────────┐        ┌────────────────────┐
│  ANALOG ZONE     │        │  DIGITAL ZONE      │
│                  │        │                    │
│  AD8232          │        │  ESP32             │
│  ADS1115         │ <──────│  WiFi              │
│                  │ Opto   │  LCD               │
│  GND_ANALOG      │        │  GND_DIGITAL       │
└──────────────────┘        └────────────────────┘
         │                           │
         └───────────[●]─────────────┘
              Single Point Ground
```

**Techniques:**

- 🔹 Optocoupler giữa ESP32 và ADS1115 (cách ly hoàn toàn)
- 🔹 Isolated DC-DC converter cho nguồn analog
- 🔹 Ferrite bead trên đường nguồn

#### 2. **EMI/EMC Compliance**

- 🔹 Thêm RC filter trên đầu vào AD8232
- 🔹 Ferrite bead trên đường nguồn VCC
- 🔹 TVS diode bảo vệ input ECG
- 🔹 Common mode choke trên I2C

#### 3. **Medical Grade Design**

```
   [ECG Electrode] ──┬── [TVS Diode] ── [RC Filter] ── [AD8232]
                     │
                   [ESD Protection]
                     │
                    GND
```

**Compliance:**

- ✅ IEC 60601-1 (Medical Electrical Equipment)
- ✅ Creepage distance: >3mm (khoảng cách điện môi)
- ✅ ESD protection: ±8kV
- ✅ Isolation: >4000V (nếu dùng optocoupler)

#### 4. **Professional Silkscreen**

```
┌─────────────────────────────────────┐
│  ECG Monitor V1.0                   │
│  ┌─────┐                            │
│  │ U1  │ ESP32-WROOM-32D            │
│  │ESP32│ TP1 TP2 TP3 (Test Points)  │
│  └─────┘                            │
│  + Voltage polarity mark            │
│  [LOGO của trường]                  │
│  [Tên sinh viên - MSSV]             │
│  [Tháng/Năm]                        │
└─────────────────────────────────────┘
```

---

## 🛠️ CHECKLIST THIẾT KẾ PCB CHO KHÓA LUẬN

### **A. Trước khi vẽ mạch (Planning)**

- [ ] Vẽ sơ đồ khối (Block Diagram)
- [ ] Phân chia vùng: Analog, Digital, Power, RF
- [ ] Chọn kích thước PCB phù hợp (khuyến nghị: 100x100mm)
- [ ] Quyết định: 2 layer hay 4 layer?
- [ ] Tính toán dòng điện tiêu thụ → chọn độ rộng track

### **B. Khi vẽ mạch (Layout)**

- [ ] Đặt connector input ở 1 phía, output ở phía đối diện
- [ ] IC quan trọng (ESP32, ADS1115) ở giữa PCB
- [ ] Tụ bypass 0.1uF cách chân VCC của IC < 5mm
- [ ] Đường nguồn rộng hơn đường tín hiệu (Power: 0.5-1mm, Signal: 0.3mm)
- [ ] Track tín hiệu analog ngắn nhất có thể
- [ ] Không chạy track digital song song track analog
- [ ] Thêm test points (TP) cho các điểm quan trọng (VCC, GND, SDA, SCL, ECG)

### **C. Ground và Power**

- [ ] Layer 2 làm GND plane hoàn chỉnh (không cắt)
- [ ] Via ghép GND layer 1 ↔ layer 2 (nhiều via, đặc biệt dưới IC)
- [ ] Ground analog và digital nối tại 1 điểm duy nhất (Star topology)
- [ ] Đường nguồn 3.3V rộng ≥ 0.5mm
- [ ] Tụ lọc nguồn tổng: 100uF + 10uF ceramic ở đầu vào

### **D. Quy tắc khoảng cách**

- [ ] IC to IC: ≥ 5mm
- [ ] Track to Track: ≥ 0.3mm (12/12 mil)
- [ ] Analog zone to Digital zone: ≥ 8mm
- [ ] Edge clearance: ≥ 3mm (từ track đến mép PCB)

### **E. Silkscreen (Lớp tơ)**

- [ ] Label tất cả connector: VCC, GND, SDA, SCL, ECG+, ECG-
- [ ] Đánh dấu cực tính (+/-) cho điện cực ECG
- [ ] Tên dự án + phiên bản (v1.0)
- [ ] Tên sinh viên, MSSV (nhỏ ở góc)
- [ ] Logo trường (nếu có)
- [ ] Ngày tháng thiết kế

### **F. Trước khi gửi sản xuất (DfM - Design for Manufacturing)**

- [ ] Chạy DRC (Design Rule Check) - 0 lỗi
- [ ] Chạy ERC (Electrical Rule Check) - 0 lỗi
- [ ] Kiểm tra lại pin mapping của IC (đúng chân)
- [ ] In ra giấy A4 theo tỷ lệ 1:1, đặt linh kiện thật lên kiểm tra
- [ ] Xuất Gerber files (GTL, GBL, GTO, GBO, GTS, GBS, TXT)
- [ ] Kiểm tra Gerber bằng online viewer

---

## 📏 KÍCH THƯỚC ĐỀ XUẤT CHO DỰ ÁN CỦA BẠN

### **Option 1: PCB 2 layer - 100x80mm** (Tiết kiệm chi phí)

```
Price: ~50,000 - 100,000 VND (5 PCB)
┌────────────────────────────────────────────────┐
│  100mm                                         │
│ ┌────────────────────────────────────────────┐ │
│ │ [Analog Zone]     [Digital Zone]   [Power]│ │ 80mm
│ │  AD8232  ADS1115   ESP32   LCD             │ │
│ │  ECG IN          I2C        WiFi           │ │
│ └────────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
```

### **Option 2: PCB 4 layer - 100x100mm** (Chuyên nghiệp)

```
Price: ~150,000 - 300,000 VND (5 PCB)
Ưu điểm:
✓ Ít nhiễu hơn 60-80%
✓ Compact hơn
✓ Professional appearance
✓ Điểm cộng lớn cho khóa luận
```

---

## 💡 MẸO THIẾT KẾ PCB CHO ĐIỂM CAO

### 1. **Thêm LED chỉ thị**

```cpp
// Thêm vào hardware:
LED1 (Green):  Power ON
LED2 (Blue):   WiFi Connected
LED3 (Red):    ECG Signal Good
LED4 (Yellow): Lead-off Warning
```

### 2. **Thêm jumper/switch cấu hình**

```
JP1: Select I2C Address (0x48 / 0x49)
JP2: Enable/Disable WiFi
SW1: Mode Select (Normal / Debug)
```

### 3. **Test Points quan trọng**

```
TP1: 3.3V
TP2: GND
TP3: ECG Signal (trước ADS1115)
TP4: I2C SDA
TP5: I2C SCL
TP6: ESP32 RX/TX (debug UART)
```

### 4. **Connector chuẩn công nghiệp**

```
❌ KHÔNG dùng: Header 2.54mm thường
✅ NÊN dùng:   JST-XH, JST-PH (chống rút nhầm)
              Screw terminal (cho ECG electrode)
```

### 5. **Mechanical mounting holes**

```
4x M3 mounting holes ở 4 góc
→ Có thể lắp vào vỏ chuyên nghiệp
→ Giáo viên thấy tính ứng dụng thực tế
```

---

## 📊 SO SÁNH THIẾT KẾ

| Tiêu chí           | PCB Thường (6-7đ) | PCB Khóa luận (8-9đ)   | PCB Chuyên nghiệp (9-10đ) |
| ------------------ | ----------------- | ---------------------- | ------------------------- |
| **Số layer**       | 1-2 layer         | 2 layer                | 4 layer                   |
| **Khoảng cách IC** | 2-3mm (chật)      | 5-8mm                  | 8-10mm + shielding        |
| **Ground plane**   | Không             | Có (layer 2)           | Có (layer 2 + 3)          |
| **Phân vùng**      | Không rõ ràng     | Có phân analog/digital | Isolation hoàn toàn       |
| **Test points**    | Không             | Có 5-8 điểm            | Có đầy đủ + socket debug  |
| **Silkscreen**     | Chỉ label IC      | Label đầy đủ + logo    | Professional + UL mark    |
| **ESD Protection** | Không             | Không                  | Có (TVS diode)            |
| **Chi phí**        | 50k               | 100k                   | 200-300k                  |
| **Thời gian làm**  | 1-2 ngày          | 3-5 ngày               | 1-2 tuần                  |

---

## 🎓 TRÌNH BÀY TRONG KHÓA LUẬN

### **Phần thiết kế phần cứng nên có:**

1. **Sơ đồ khối hệ thống** (Block Diagram)
2. **Sơ đồ nguyên lý** (Schematic) - xuất từ KiCAD/Altium
3. **Layout PCB 3D** - ảnh render đẹp
4. **Gerber files** - chứng minh đã thiết kế đến nơi đến chốn
5. **BOM (Bill of Materials)** - bảng linh kiện chi tiết
6. **Giải thích:**
   - Tại sao chọn khoảng cách đó?
   - Tại sao dùng 4 layer thay vì 2 layer?
   - Làm thế nào để giảm nhiễu?
   - Tuân thủ chuẩn nào? (IPC-2221, IEC-60601)

### **Câu trả lời mẫu khi bảo vệ:**

**Câu hỏi: "Tại sao em đặt ADS1115 cách AD8232 5mm?"**

✅ **Trả lời tốt:**

> "Em tuân thủ chuẩn IPC-2221 với khoảng cách IC tối thiểu 3mm, nhưng em tăng lên 5mm vì:
>
> 1. Dễ hàn thủ công hơn
> 2. Tránh nhiễu nhiệt từ IC này sang IC kia
> 3. Để test point giữa 2 IC để debug
> 4. Tham khảo từ reference design của TI (Texas Instruments)"

**Câu hỏi: "Tại sao phải dùng ground plane?"**

✅ **Trả lời tốt:**

> "Ground plane giúp:
>
> 1. Giảm impedance của đường GND → giảm nhiễu
> 2. Tản nhiệt tốt hơn (copper dissipate heat)
> 3. Shielding cho tín hiệu analog ECG (~1mV rất yếu)
> 4. Đây là best practice trong thiết kế PCB y tế theo chuẩn IEC 60601"

---

## 🔧 CÔNG CỤ THIẾT KẾ ĐỀ XUẤT

### **Free & Easy:**

- 🔹 **KiCAD** (Khuyến nghị nhất cho khóa luận)
  - Free, mã nguồn mở
  - Có thư viện đầy đủ
  - Community lớn
  - Export Gerber chuẩn

### **Professional:**

- 🔹 **Altium Designer** (Nếu trường có license)
  - Industry standard
  - Thiết kế chuẩn nhất
  - Điểm cộng lớn nếu dùng

### **Online:**

- 🔹 **EasyEDA**
  - Thiết kế online
  - Link trực tiếp với JLCPCB
  - Dễ order PCB

---

## 💰 CHI PHÍ DỰ TOÁN

### **Sản xuất PCB tại Việt Nam:**

| Specification               | Giá (5 PCB)  |
| --------------------------- | ------------ |
| 2 layer - 100x100mm - Green | 50,000 VND   |
| 2 layer - 100x100mm - Black | 80,000 VND   |
| 4 layer - 100x100mm - Green | 200,000 VND  |
| Assembly (hàn linh kiện)    | +100,000 VND |

**Nhà cung cấp Việt Nam:**

- PCBWay Vietnam
- JLCPCB (ship về VN)
- PCB Việt Nam

---

## ✅ TÓM TẮT - ACTION PLAN

### **Cho điểm 7-8 (Đạt yêu cầu):**

1. PCB 2 layer, 100x80mm
2. Phân vùng analog/digital rõ ràng
3. Khoảng cách IC ≥ 5mm
4. Ground plane layer 2
5. Silkscreen đầy đủ label

### **Cho điểm 8-9 (Khá - Giỏi):**

1. PCB 4 layer
2. Guard ring cho vùng analog
3. Test points đầy đủ
4. Differential routing cho I2C
5. 3D render đẹp cho thuyết trình

### **Cho điểm 9-10 (Xuất sắc):**

1. Medical grade design
2. Isolation analog/digital
3. ESD/EMI protection
4. Tuân thủ IEC 60601
5. Thiết kế vỏ hộp chuyên nghiệp

---

## 📚 TÀI LIỆU THAM KHẢO

1. **IPC-2221**: Generic Standard on Printed Board Design
2. **IEC 60601-1**: Medical Electrical Equipment Standard
3. **Texas Instruments**: "PCB Layout Guidelines for ADS1115"
4. **Analog Devices**: "ECG Front-End Design Guide"
5. **KiCAD Documentation**: https://docs.kicad.org/

---

**Lời khuyên cuối:**

> Đừng cố làm quá phức tạp. Một PCB 4 layer đơn giản nhưng chuyên nghiệp (clean layout, rõ ràng, có test point) vẫn tốt hơn một PCB 6 layer phức tạp nhưng lộn xộn.
>
> **Focus vào:** Clean layout + Professional silkscreen + Good documentation

**Chúc bạn bảo vệ khóa luận thành công! 🎓**
