# ✅ CHECKLIST THIẾT KẾ PCB - IN RA KIỂM TRA

## 📋 TRƯỚC KHI VẼ MẠCH

- [ ] Đã vẽ sơ đồ khối (block diagram)
- [ ] Đã phân vùng: Analog, Digital, Power
- [ ] Đã chọn kích thước PCB: **\_\_\_** x **\_\_\_** mm
- [ ] Đã quyết định số layer: ☐ 2 layer ☐ 4 layer
- [ ] Đã tính dòng điện và chọn độ rộng track

---

## 🛠️ KHI LAYOUT PCB

### Vị trí linh kiện:

- [ ] Connector input ở bên trái, output ở bên phải
- [ ] ESP32 ở giữa PCB
- [ ] ADS1115 cách AD8232 ≥ 5mm
- [ ] ADS1115 cách ESP32 ≥ 5mm
- [ ] Tụ bypass 0.1uF sát chân VCC (<5mm) của:
  - [ ] ESP32
  - [ ] ADS1115
  - [ ] AD8232

### Khoảng cách:

- [ ] IC to IC: ≥ 5mm
- [ ] Track to Track: ≥ 0.3mm
- [ ] Vùng Analog cách Digital: ≥ 8mm
- [ ] Track to edge: ≥ 3mm

### Track (đường mạch):

- [ ] Track nguồn 3.3V: ≥ 0.5mm
- [ ] Track nguồn GND: ≥ 0.5mm
- [ ] Track tín hiệu ECG: càng ngắn càng tốt, ≥ 0.3mm
- [ ] Track I2C (SDA, SCL): song song, cùng độ dài

### Ground:

- [ ] Layer 2 làm GND plane hoàn chỉnh
- [ ] Via kết nối GND ở dưới mỗi IC (≥4 via/IC)
- [ ] Ground analog và digital nối tại 1 điểm duy nhất
- [ ] Không cắt ground plane dưới IC analog

### Power:

- [ ] Tụ lọc đầu vào: 100uF + 10uF
- [ ] Tụ bypass mỗi IC: 0.1uF + 10uF
- [ ] Đường nguồn không chạy song song đường tín hiệu

---

## 🎨 SILKSCREEN (Lớp tơ)

- [ ] Đã label tất cả connector:
  - [ ] VCC, GND
  - [ ] SDA, SCL
  - [ ] ECG+, ECG-, ECG_REF
  - [ ] LO+, LO-
- [ ] Đã đánh dấu cực tính (+/-) cho:
  - [ ] Capacitor phân cực
  - [ ] Điện cực ECG
- [ ] Đã ghi tên dự án + version (VD: "ECG Monitor v1.0")
- [ ] Đã ghi tên sinh viên + MSSV (nhỏ ở góc)
- [ ] Đã thêm logo trường (nếu có)
- [ ] Đã ghi ngày tháng: **_/_**/2025

---

## 🔬 TEST POINTS

- [ ] TP1: 3.3V
- [ ] TP2: GND
- [ ] TP3: ECG Signal (OUTPUT của AD8232)
- [ ] TP4: I2C SDA
- [ ] TP5: I2C SCL
- [ ] TP6: ESP32 TX/RX (debug UART)

---

## ✨ TÍNH NĂNG NÂNG CAO (Điểm cộng)

- [ ] LED chỉ thị:
  - [ ] Power ON (Green)
  - [ ] WiFi Connected (Blue)
  - [ ] ECG Signal (Red)
- [ ] Mounting holes: 4x M3 ở 4 góc
- [ ] Guard ring/trace bao quanh vùng analog
- [ ] Jumper/Switch cấu hình
- [ ] Connector chuẩn JST-XH (không dùng header thường)

---

## 🔍 TRƯỚC KHI GỬI SẢN XUẤT

- [ ] Chạy DRC (Design Rule Check) → 0 lỗi
- [ ] Chạy ERC (Electrical Rule Check) → 0 lỗi
- [ ] Kiểm tra lại chân IC (đúng pin mapping)
- [ ] In ra A4 tỷ lệ 1:1, đặt linh kiện thật lên check
- [ ] Kiểm tra Gerber bằng online viewer
- [ ] Đã xuất files:
  - [ ] GTL (Top Copper)
  - [ ] GBL (Bottom Copper)
  - [ ] GTO (Top Silkscreen)
  - [ ] GBO (Bottom Silkscreen)
  - [ ] GTS (Top Soldermask)
  - [ ] GBS (Bottom Soldermask)
  - [ ] TXT (Drill file)
  - [ ] G2L, G3L (nếu 4 layer)

---

## 📐 THÔNG SỐ KỸ THUẬT

| Thông số             | Giá trị đã chọn              |
| -------------------- | ---------------------------- |
| Kích thước PCB       | **\_** x **\_** mm           |
| Số layer             | ☐ 2L ☐ 4L                    |
| Track width (Signal) | **\_** mm                    |
| Track width (Power)  | **\_** mm                    |
| Clearance            | **\_** mm                    |
| Via size             | **\_** mm                    |
| Via drill            | **\_** mm                    |
| Soldermask color     | ☐ Green ☐ Blue ☐ Black ☐ Red |
| Silkscreen color     | ☐ White ☐ Black              |
| Surface finish       | ☐ HASL ☐ ENIG                |

---

## 💰 CHI PHÍ DỰ TOÁN

| Hạng mục         | Số lượng | Đơn giá | Thành tiền |
| ---------------- | -------- | ------- | ---------- |
| PCB 2L 100x100mm | 5        |         |            |
| Linh kiện        | 1 bộ     |         |            |
| Hàn linh kiện    |          |         |            |
| Vỏ hộp           |          |         |            |
| **TỔNG**         |          |         |            |

---

## 📸 CHECKLIST CHỤP ẢNH CHO KHÓA LUẬN

- [ ] Ảnh sơ đồ khối (block diagram) - độ phân giải cao
- [ ] Ảnh schematic (sơ đồ nguyên lý) - rõ nét
- [ ] Ảnh layout PCB 2D (top view)
- [ ] Ảnh layout PCB 2D (bottom view)
- [ ] Ảnh 3D render (góc 45°, có bóng đổ)
- [ ] Ảnh PCB thật sau khi sản xuất
- [ ] Ảnh PCB đã hàn linh kiện
- [ ] Ảnh hệ thống hoàn chỉnh

---

## 📝 GHI CHÚ

```
Ghi chú thêm trong quá trình thiết kế:
_________________________________________________________
_________________________________________________________
_________________________________________________________
_________________________________________________________
_________________________________________________________
_________________________________________________________
```

---

## ⚠️ LƯU Ý QUAN TRỌNG

**TRƯỚC KHI ORDER PCB:**

1. ✅ Kiểm tra lại pin mapping của ESP32
2. ✅ Kiểm tra địa chỉ I2C của ADS1115 (0x48)
3. ✅ Kiểm tra chân LO+ = GPIO 13, LO- = GPIO 14
4. ✅ Kiểm tra I2C: SDA = GPIO 21, SCL = GPIO 22
5. ✅ Đảm bảo có tụ bypass cho TẤT CẢ IC

**SAU KHI NHẬN PCB:**

1. ⚡ Kiểm tra chập mạch bằng multimeter (GND ↔ VCC)
2. ⚡ Hàn nguồn và test LED power trước
3. ⚡ Hàn từng IC một, test từng bước
4. ⚡ Không cấp nguồn quá 3.3V cho ESP32

---

**Signature:** **********\_\_\_\_********** **Date:** **_/_**/2025
