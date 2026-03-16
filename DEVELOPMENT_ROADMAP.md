# 🛣️ ROADMAP PHÁT TRIỂN - TỪ BREADBOARD ĐẾN PCB CHUYÊN NGHIỆP

> **Chiến lược:** Test kỹ trên breadboard → Code ổn định → Thiết kế PCB
> **Mục tiêu:** Tránh lãng phí tiền bạc, thời gian làm lại PCB

---

## ✅ CHIẾN LƯỢC CỦA BẠN LÀ ĐÚNG!

### Tại sao nên test trên breadboard/perfboard trước?

**Ưu điểm:**

- ✅ **Linh hoạt:** Thay đổi kết nối dễ dàng khi debug
- ✅ **Tiết kiệm:** Không phải order PCB nhiều lần (mỗi lần 100-300k)
- ✅ **Học hỏi:** Hiểu rõ cách hoạt động của từng module
- ✅ **An toàn:** Không sợ hỏng PCB khi code lỗi
- ✅ **Demo sớm:** Có thể demo cho thầy cô khi code chưa hoàn thiện

**Nhược điểm (chấp nhận được):**

- ❌ Nhiễu nhiều hơn (nhưng vẫn test được chức năng)
- ❌ Kích thước lớn (nhưng không quan trọng lúc này)
- ❌ Không đẹp bằng PCB (nhưng đây mới là giai đoạn R&D)

---

## 📅 TIMELINE PHÁT TRIỂN ĐỀ XUẤT

### **Giai đoạn 1: BREADBOARD TESTING (2-4 tuần)**

**Mục tiêu:** Validate tất cả chức năng cơ bản

```
Tuần 1-2: Hardware Integration
├─ Module AD8232 + ADS1115 → Test đọc ECG
├─ Module ESP32 → Test WiFi, LVGL
├─ Module MAX30102 → Test SpO2
├─ Module MLX90614 → Test nhiệt độ
└─ Test riêng lẻ từng module

Tuần 3-4: System Integration
├─ Kết nối tất cả modules với ESP32
├─ Test I2C bus (nhiều cảm biến cùng lúc)
├─ Test power consumption
├─ Test WiFi + gửi data lên cloud
└─ Phát hiện và fix bugs
```

**Deliverable:**

- [ ] Tất cả sensors đọc được data
- [ ] WiFi kết nối ổn định
- [ ] LVGL UI hiển thị được
- [ ] Gửi data lên Google Sheets thành công
- [ ] Code chạy liên tục >1 giờ không crash

---

### **Giai đoạn 2: PERFBOARD PROTOTYPE (1-2 tuần)**

**Mục tiêu:** Gọn gàng hơn, ổn định hơn để demo

```
Tuần 1: Assembly
├─ Hàn modules lên perfboard (PCB đục lỗ)
├─ Bố trí gọn gàng (vùng analog xa vùng digital)
├─ Dây nối ngắn, gọn
└─ Làm vỏ hộp đơn giản (nếu có thời gian)

Tuần 2: Final Testing
├─ Test lại tất cả chức năng
├─ Test độ bền (chạy 24h liên tục)
├─ Đo nhiễu, chất lượng tín hiệu
└─ Chuẩn bị demo cho thầy cô
```

**Deliverable:**

- [ ] Hệ thống compact hơn breadboard
- [ ] Chạy ổn định >24 giờ
- [ ] Sẵn sàng demo cho giáo viên
- [ ] Document các vấn đề gặp phải + cách fix

---

### **Giai đoạn 3: CODE FINALIZATION (2-3 tuần)**

**Mục tiêu:** Code production-ready

```
Tuần 1: Code Refinement
├─ Thêm error handling
├─ Optimize performance
├─ Thêm logging/debug
└─ Comment code đầy đủ

Tuần 2: Features Complete
├─ Tất cả features theo yêu cầu khóa luận
├─ UI/UX hoàn thiện
├─ Battery management (nếu có)
└─ OTA update (nếu cần)

Tuần 3: Testing & Documentation
├─ Test cases đầy đủ
├─ Viết user manual
├─ Chuẩn bị nội dung khóa luận
└─ Record video demo
```

**Deliverable:**

- [ ] Code không còn bugs nghiêm trọng
- [ ] Tất cả features hoạt động
- [ ] Code được comment tốt
- [ ] Tài liệu kỹ thuật hoàn chỉnh

---

### **Giai đoạn 4: PCB DESIGN (1-2 tuần)**

**Mục tiêu:** Thiết kế PCB chuyên nghiệp dựa trên breadboard đã test

```
Tuần 1: Schematic & Layout
├─ Vẽ schematic dựa trên breadboard
├─ Layout PCB (áp dụng kiến thức từ file hướng dẫn)
├─ Review với thầy/bạn
└─ Fix và finalize

Tuần 2: Order & Wait
├─ Export Gerber
├─ Order PCB (JLCPCB hoặc PCBViet)
├─ Order linh kiện
└─ Chờ giao hàng (3-14 ngày)
```

**Deliverable:**

- [ ] Schematic hoàn chỉnh
- [ ] PCB layout đạt chuẩn
- [ ] Gerber files
- [ ] BOM (Bill of Materials)

---

### **Giai đoạn 5: PCB ASSEMBLY & FINAL TEST (1 tuần)**

**Mục tiêu:** Sản phẩm cuối cùng

```
├─ Nhận PCB và linh kiện
├─ Hàn linh kiện (cẩn thận!)
├─ Test từng bước (power → IC → sensors)
├─ Flash code (đã test kỹ rồi nên ít bugs)
├─ Final testing
└─ Lắp vào vỏ hộp
```

**Deliverable:**

- [ ] PCB hoạt động 100%
- [ ] Sản phẩm hoàn chỉnh
- [ ] Sẵn sàng bảo vệ khóa luận

---

## 📋 CHECKLIST - TEST TRÊN BREADBOARD TRƯỚC KHI LÀM PCB

### **A. Hardware Validation**

#### 1. Power System

- [ ] ESP32 nhận đủ 3.3V (đo bằng multimeter)
- [ ] Dòng điện tổng < 500mA (đo bằng ammeter)
- [ ] Không có IC nào nóng bất thường (>50°C)
- [ ] Capacitor đủ cho mỗi IC (0.1uF + 10uF)

#### 2. Communication (I2C)

- [ ] ADS1115 đúng địa chỉ 0x48
- [ ] MLX90614 đúng địa chỉ 0x5A
- [ ] MAX30102 đúng địa chỉ 0x57
- [ ] Chạy I2C scanner → detect hết devices
- [ ] I2C hoạt động với pull-up 4.7kΩ

#### 3. Sensors

- [ ] AD8232 + ADS1115: Đọc được ECG waveform
- [ ] Lead-off detection hoạt động (LO+, LO-)
- [ ] Heart rate detection chính xác ±5 BPM
- [ ] MAX30102: SpO2 và HR đo được
- [ ] MLX90614: Nhiệt độ chính xác ±0.5°C

#### 4. Display & UI

- [ ] TFT LCD hiển thị đúng (không bị lỗi pixel)
- [ ] LVGL render mượt (>15 FPS)
- [ ] Touch (nếu có) responsive
- [ ] UI không bị crash khi thao tác

#### 5. Connectivity

- [ ] WiFi connect được router
- [ ] HTTP request thành công
- [ ] Google Sheets nhận được data
- [ ] Reconnect tự động khi mất WiFi

---

### **B. Software Validation**

#### 1. Stability

- [ ] Chạy liên tục 1 giờ không crash
- [ ] Chạy liên tục 6 giờ không crash
- [ ] Chạy liên tục 24 giờ không crash
- [ ] Memory leak check (heap usage ổn định)

#### 2. Error Handling

- [ ] Xử lý khi sensor không phản hồi
- [ ] Xử lý khi WiFi mất kết nối
- [ ] Xử lý khi Google Sheets lỗi
- [ ] Xử lý khi power loss đột ngột

#### 3. Performance

- [ ] Boot time < 5 giây
- [ ] Sensor update rate ≥ 1Hz
- [ ] UI responsive (không lag)
- [ ] WiFi data send < 2 giây

---

### **C. Documentation (Chuẩn bị cho PCB)**

- [ ] Schematic vẽ lại từ breadboard (bằng KiCAD/Altium)
- [ ] Note tất cả GPIO đã dùng
- [ ] Document tất cả kết nối I2C, SPI, UART
- [ ] Chụp ảnh breadboard (nhiều góc độ)
- [ ] Note những vấn đề gặp phải và cách fix
- [ ] List linh kiện cuối cùng (BOM sơ bộ)

---

## 🛠️ SETUP BREADBOARD TỐI ƯU

### **Bố trí đề xuất:**

```
┌─────────────────────────────────────────────────────┐
│  BREADBOARD LAYOUT                                  │
│                                                     │
│  [Power Section]           [Analog Section]        │
│  ┌──────────┐             ┌──────────┐            │
│  │ 3.3V REG │             │ AD8232   │            │
│  │ AMS1117  │             │ ADS1115  │            │
│  │ + Caps   │             │          │            │
│  └──────────┘             └──────────┘            │
│                                                     │
│  [Digital Section]         [Display]               │
│  ┌──────────────┐        ┌──────────┐            │
│  │ ESP32        │        │  TFT LCD │            │
│  │              │        │  ILI9341 │            │
│  └──────────────┘        └──────────┘            │
│                                                     │
│  [Other Sensors]                                   │
│  ┌────────┐  ┌────────┐                          │
│  │MAX30102│  │MLX90614│                          │
│  └────────┘  └────────┘                          │
│                                                     │
│  [LED Indicators]                                  │
│  ○ Power  ○ WiFi  ○ ECG  ○ Error                 │
└─────────────────────────────────────────────────┘
```

### **Tips cho breadboard:**

1. **Phân vùng rõ ràng**
   - Power ở 1 phía
   - Analog (AD8232, ADS1115) ở xa Digital
   - Display ở riêng 1 vùng

2. **Dây nối**
   - Dùng dây jumper ngắn (<10cm)
   - Analog signals dùng dây xoắn shielded (nếu có)
   - Power dùng dây đỏ, GND dùng dây đen (dễ nhìn)

3. **Decoupling**
   - Mỗi IC có 1 tụ 0.1uF sát chân VCC
   - Power rail có 100uF + 10uF

4. **Ground**
   - Tất cả GND nối chung tại 1 điểm
   - Analog GND và Digital GND cùng nối vào đó

5. **Test Points**
   - Để sẵn header pins ở các điểm quan trọng để đo

---

## 📊 SO SÁNH CÁC GIAI ĐOẠN

| Tiêu chí                | Breadboard | Perfboard | PCB Prototype   | PCB Final     |
| ----------------------- | ---------- | --------- | --------------- | ------------- |
| **Chi phí**             | 100k       | 150k      | 300k            | 500k          |
| **Thời gian làm**       | 2-3 ngày   | 1 tuần    | 2 tuần + ship   | 2 tuần + ship |
| **Độ linh hoạt**        | ⭐⭐⭐⭐⭐ | ⭐⭐⭐    | ⭐              | ⭐            |
| **Chất lượng tín hiệu** | ⭐⭐       | ⭐⭐⭐    | ⭐⭐⭐⭐        | ⭐⭐⭐⭐⭐    |
| **Độ bền**              | ⭐         | ⭐⭐⭐    | ⭐⭐⭐⭐        | ⭐⭐⭐⭐⭐    |
| **Mục đích**            | Test code  | Demo sớm  | Test PCB design | Sản phẩm cuối |

---

## ⚠️ CÁC SAI LẦM CẦN TRÁNH

### ❌ **SAI LẦM 1: Thiết kế PCB quá sớm**

```
Hậu quả:
→ Code chưa ổn → phát hiện cần thêm sensor
→ Phải thiết kế lại PCB
→ Mất tiền, mất thời gian
```

### ❌ **SAI LẦM 2: Không test đủ trên breadboard**

```
Hậu quả:
→ Order PCB xong mới phát hiện IC không hoạt động
→ PCB thành phế liệu
→ Phải order lại
```

### ❌ **SAI LẦM 3: Không document lại breadboard**

```
Hậu quả:
→ Khi vẽ PCB, quên mất đã nối như thế nào
→ Vẽ sai schematic
→ PCB không hoạt động
```

### ✅ **ĐÚNG: Làm theo roadmap trên**

```
1. Test kỹ trên breadboard
2. Document đầy đủ
3. Code ổn định 100%
4. Mới thiết kế PCB
→ PCB hoạt động ngay lần đầu
→ Tiết kiệm thời gian và tiền bạc
```

---

## 🎯 MỐC THỜI GIAN QUAN TRỌNG

**GIẢ SỬ BẠN BẢO VỆ KHÓA LUẬN VÀO THÁNG 6:**

```
┌───────────────────────────────────────────────────┐
│  THÁNG 3 (Hiện tại)                               │
│  ✓ Breadboard testing                             │
│  ✓ Code development                               │
│  ✓ Debug và fix bugs                              │
├───────────────────────────────────────────────────┤
│  THÁNG 4 (1 tháng sau)                            │
│  ✓ Perfboard assembly                             │
│  ✓ Code finalization                              │
│  ✓ Demo cho thầy hướng dẫn (lần 1)                │
├───────────────────────────────────────────────────┤
│  THÁNG 5 TUẦN 1-2 (2 tháng trước bảo vệ)        │
│  ✓ Thiết kế PCB                                   │
│  ✓ Order PCB + linh kiện                          │
│  ✓ Viết khóa luận (phần cứng)                     │
├───────────────────────────────────────────────────┤
│  THÁNG 5 TUẦN 3-4                                 │
│  ✓ Nhận PCB                                       │
│  ✓ Hàn và test                                    │
│  ✓ Demo cho thầy (lần 2)                          │
│  ✓ Fix bugs cuối cùng                             │
├───────────────────────────────────────────────────┤
│  THÁNG 6 TUẦN 1-2 (1 tháng trước bảo vệ)        │
│  ✓ Hoàn thiện khóa luận                           │
│  ✓ Chuẩn bị slides                                │
│  ✓ Luyện thuyết trình                             │
├───────────────────────────────────────────────────┤
│  THÁNG 6 TUẦN 3-4                                 │
│  ✓ BẢO VỆ KHÓA LUẬN 🎓                           │
└───────────────────────────────────────────────────┘
```

**⚠️ Lưu ý:** Nên có PCB final **ít nhất 1 tháng trước** khi bảo vệ để có thời gian fix bugs nếu có.

---

## 📝 DOCUMENT NGAY TỪ BÂY GIỜ

### **File Excel/Google Sheet tracking:**

| STT | Module   | Status     | Tested?    | Issues       | Notes              |
| --- | -------- | ---------- | ---------- | ------------ | ------------------ |
| 1   | ESP32    | ✅ OK      | ✅ Yes     | None         | Boot time 3s       |
| 2   | AD8232   | 🔄 Testing | ❌ No      | Noisy        | Need better filter |
| 3   | ADS1115  | ✅ OK      | ✅ Yes     | None         | I2C addr 0x48      |
| 4   | MAX30102 | ⏸️ Pending | ❌ No      | -            | Chưa nhận hàng     |
| 5   | MLX90614 | ✅ OK      | ✅ Yes     | None         | Works perfect      |
| 6   | TFT LCD  | 🔄 Testing | ⏸️ Partial | Slow refresh | Optimize needed    |
| 7   | WiFi     | ✅ OK      | ✅ Yes     | None         | Reconnect OK       |

### **Log Book (Nhật ký phát triển):**

```
Ngày 02/03/2026:
- Test AD8232 với ADS1115
- Vấn đề: Nhiễu quá nhiều
- Giải pháp: Thêm RC filter
- Kết quả: Cải thiện 70%
- TODO: Test thêm với dây chắn nhiễu

Ngày 03/03/2026:
- Test ESP32 WiFi
- Kết quả: Connect OK
- Test gửi data lên Sheets: OK
- Latency: ~1.5s
```

---

## 💡 LỜI KHUYÊN TỪ KINH NGHIỆM

### **1. Ưu tiên chức năng trước, form sau**

> Đừng lo về PCB đẹp hay không. Lo về code chạy được hay không.

### **2. Test, test, và test**

> Một giờ test trên breadboard = Tiết kiệm 1 tuần khi làm PCB.

### **3. Document mọi thứ**

> Não bạn sẽ quên. Giấy không quên.

### **4. Show progress cho thầy thường xuyên**

> Thầy thấy bạn làm việc → Dễ pass hơn.

### **5. Có Plan B**

> - Plan A: PCB đẹp, professional
> - Plan B: Perfboard vẫn chạy tốt, có thể bảo vệ được

### **6. Đừng cố làm quá hoàn hảo**

> 80% functionality với 100% stability > 100% functionality với 50% stability

---

## ✅ ACTION PLAN CHO TUẦN NÀY

**Tuần này (2-8/3/2026):**

- [ ] Tập trung test AD8232 + ADS1115 trên breadboard
- [ ] Fix code ECG processing
- [ ] Test lead-off detection
- [ ] Test heart rate accuracy (so với máy đo thật)
- [ ] Document lại kết nối hiện tại (vẽ schematic đơn giản)

**Tuần sau (9-15/3/2026):**

- [ ] Tích hợp thêm sensors khác (MAX30102, MLX90614)
- [ ] Test I2C bus với nhiều devices
- [ ] Test LVGL UI với nhiều màn hình
- [ ] Measure power consumption

**Mục tiêu tháng 3:**
✅ Tất cả sensors hoạt động trên breadboard
✅ Code ổn định, không crash
✅ Sẵn sàng chuyển sang perfboard

---

## 🎓 TIPS KHI DEMO CHO THẦY CÔ

**Demo lần 1 (Breadboard):**

> "Em đang test từng module trên breadboard để đảm bảo chức năng trước khi thiết kế PCB chuyên nghiệp. Phương pháp này giúp em tránh lãng phí chi phí và phát hiện sớm các vấn đề kỹ thuật."

**Demo lần 2 (Perfboard):**

> "Em đã assembly lại hệ thống trên perfboard để có sản phẩm ổn định hơn, compact hơn. Đây là bản prototype trước khi em thiết kế PCB cuối cùng."

**Demo cuối (PCB):**

> "Dựa trên kinh nghiệm từ breadboard và perfboard, em đã thiết kế PCB 4-layer tuân thủ chuẩn IPC-2221 và IEC 60601 cho thiết bị y tế."

→ **Thầy thấy quá trình phát triển có hệ thống → Điểm cao!**

---

## 📞 KHI CẦN HỖ TRỢ

**Trong quá trình test breadboard, nếu gặp vấn đề:**

1. Check lại kết nối (90% lỗi do đây)
2. Đo voltage tại các điểm quan trọng
3. Chạy I2C scanner
4. Check datasheet của IC
5. Hỏi trên forum (ESP32.com, Arduino forum)
6. Hỏi thầy hướng dẫn

**Khi sẵn sàng thiết kế PCB:**

- Đọc lại file PCB_DESIGN_PROFESSIONAL.md
- Follow checklist trong PCB_CHECKLIST.md
- Tham khảo references trong PCB_REFERENCES.md

---

## 🎯 TÓM TẮT

✅ **Bạn đang làm đúng!** Gom module breadboard, test code trước là chiến lược thông minh.

✅ **Timeline:**

- Tháng 3: Breadboard testing ← **BẠN ĐANG Ở ĐÂY**
- Tháng 4: Code finalization + Perfboard
- Tháng 5: PCB design + Assembly
- Tháng 6: Hoàn thiện + Bảo vệ

✅ **Ưu tiên:**

1. Code ổn định
2. Tất cả sensors hoạt động
3. Document đầy đủ
4. Mới thiết kế PCB

✅ **Chi phí tiết kiệm:**

- Nếu thiết kế PCB ngay: 500k (có thể fail) + 500k (làm lại) = 1,000k
- Nếu test kỹ trước: 100k (breadboard) + 500k (PCB lần 1 OK) = 600k
- **Tiết kiệm: 400,000 VND!**

---

**Cứ tập trung test và hoàn thiện code trên breadboard đi. PCB để sau, không vội! 🚀**

**Có gì cần hỗ trợ về code hay hardware testing, cứ hỏi nhé!** 💪
