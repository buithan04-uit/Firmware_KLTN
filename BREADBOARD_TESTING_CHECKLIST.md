# ✅ BREADBOARD TESTING CHECKLIST - THEO DÕI HÀNG NGÀY

**Dự án:** ECG Monitor với ESP32  
**Giai đoạn:** Breadboard Testing  
**Bắt đầu:** **_/_**/2026  
**Mục tiêu:** Code ổn định 100% trước khi thiết kế PCB

---

## 📅 TUẦN 1: TEST TỪNG MODULE RIÊNG LẺ

### Ngày 1: ESP32 Basic

- [ ] ESP32 boot OK
- [ ] Serial monitor hoạt động (115200 baud)
- [ ] Blink LED test (GPIO test)
- [ ] WiFi scan được networks
- [ ] WiFi connect được router
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 2: AD8232 + ADS1115 (ECG)

- [ ] AD8232 nhận nguồn 3.3V
- [ ] ADS1115 I2C address 0x48 detected
- [ ] Đọc được giá trị từ ADS1115 A0
- [ ] Lead-off detection hoạt động (LO+ GPIO13, LO- GPIO14)
- [ ] Thử gắn điện cực → có tín hiệu
- [ ] Tín hiệu ECG có dạng waveform (không phải noise)
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 3: Lọc nhiễu ECG

- [ ] Implement Moving Average Filter
- [ ] Implement Median Filter
- [ ] Implement High-Pass Filter
- [ ] So sánh raw vs filtered signal
- [ ] Tín hiệu mượt hơn chưa?
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 4: Heart Rate Detection

- [ ] Detect peaks trong ECG signal
- [ ] Tính được BPM
- [ ] So sánh với máy đo thật (± bao nhiêu BPM?)
- [ ] Test với nhiều người (nếu được)
- [ ] Heart rate ổn định hay nhảy lung tung?
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 5: TFT LCD + LVGL

- [ ] LCD hiển thị được (test màu: red, green, blue)
- [ ] LVGL init OK
- [ ] Tạo 1 label → hiển thị text
- [ ] Tạo 1 chart → vẽ được
- [ ] FPS đạt >15fps
- [ ] Không bị flicker/lag
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 6: WiFi + Google Sheets

- [ ] HTTP request test (httpbin.org)
- [ ] POST request với JSON
- [ ] Gửi data lên Google Sheets thành công
- [ ] Nhận được response từ server
- [ ] Latency < 3 giây
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 7: Review tuần 1

- [ ] Document lại tất cả vấn đề đã gặp
- [ ] Chụp ảnh breadboard setup
- [ ] Vẽ sơ đồ kết nối đơn giản (trên giấy cũng được)
- [ ] Update code lên Git/backup
- [ ] Plan cho tuần sau

**Tiến độ tuần 1:** **_/42 ✓ (_**%)

---

## 📅 TUẦN 2: TÍCH HỢP HỆ THỐNG

### Ngày 8: MAX30102 (SpO2)

- [ ] MAX30102 I2C detected (0x57)
- [ ] Đọc được Red LED value
- [ ] Đọc được IR LED value
- [ ] Detect finger present
- [ ] Tính được SpO2 (gần đúng)
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 9: MLX90614 (Temperature)

- [ ] MLX90614 I2C detected (0x5A)
- [ ] Đọc được nhiệt độ object
- [ ] Đọc được nhiệt độ ambient
- [ ] So với nhiệt kế thật (± bao nhiêu độ?)
- [ ] Temperature stable
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 10: I2C Multi-Device

- [ ] I2C scanner detect hết devices
- [ ] ADS1115 (0x48) ✓
- [ ] MAX30102 (0x57) ✓
- [ ] MLX90614 (0x5A) ✓
- [ ] Không conflict address
- [ ] Đọc tuần tự từng sensor → OK
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 11: Tích hợp UI

- [ ] ECG waveform vẽ trên chart
- [ ] Heart rate hiển thị số
- [ ] SpO2 hiển thị %
- [ ] Temperature hiển thị °C
- [ ] Lead-off warning hiển thị
- [ ] WiFi status icon
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 12: Power & Performance

- [ ] Đo dòng tiêu thụ tổng: **\_** mA
- [ ] ESP32 nhiệt độ: **\_** °C
- [ ] ADS1115 nhiệt độ: **\_** °C
- [ ] LCD nhiệt độ: **\_** °C
- [ ] Không có IC nào nóng bất thường
- [ ] Memory usage (heap free): **\_** KB
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 13: Stability Test 1 giờ

- [ ] Chạy liên tục 1 giờ
- [ ] Không crash
- [ ] Không reboot
- [ ] Memory leak check (heap tăng/giảm?)
- [ ] Sensors vẫn đọc OK
- [ ] WiFi vẫn connected
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 14: Review tuần 2

- [ ] Tất cả sensors hoạt động cùng lúc
- [ ] UI hiển thị đầy đủ
- [ ] Code không crash
- [ ] Document các issues
- [ ] Update Git/backup

**Tiến độ tuần 2:** **_/42 ✓ (_**%)

---

## 📅 TUẦN 3: STABILITY & ERROR HANDLING

### Ngày 15: Error Handling - Sensors

- [ ] Rút điện cực ECG → hiển thị warning
- [ ] Gắn lại → tự động recovery
- [ ] Rút finger từ MAX30102 → detect
- [ ] Che MLX90614 → đọc ambient temp
- [ ] I2C timeout handling
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 16: Error Handling - WiFi

- [ ] WiFi disconnect → auto reconnect
- [ ] Router tắt → hiển thị "No WiFi"
- [ ] Router bật lại → auto connect
- [ ] HTTP request fail → retry
- [ ] Google Sheets lỗi → queue data
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 17: Stability Test 6 giờ

- [ ] Chạy liên tục 6 giờ
- [ ] Log errors nếu có
- [ ] Count số lần reboot (nếu có): **\_**
- [ ] Memory usage ổn định?
- [ ] WiFi reconnect bao nhiêu lần: **\_**
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 18: Stability Test 24 giờ (Setup overnight)

- [ ] Setup chạy qua đêm
- [ ] Sáng hôm sau kiểm tra
- [ ] Hệ thống còn chạy?
- [ ] Có crash không?
- [ ] Log errors
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 19: Code Cleanup

- [ ] Comment đầy đủ code
- [ ] Remove debug prints không cần thiết
- [ ] Organize code thành functions
- [ ] Add error codes
- [ ] Format code (indent, naming)
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 20: Documentation

- [ ] Vẽ lại schematic từ breadboard
- [ ] List tất cả GPIO đã dùng
- [ ] List tất cả I2C addresses
- [ ] Chụp ảnh breadboard (nhiều góc)
- [ ] Video demo hoạt động
- [ ] Ghi chú vấn đề: ****************\_****************

### Ngày 21: Review tuần 3

- [ ] Code stable
- [ ] Error handling đầy đủ
- [ ] Test 24h pass
- [ ] Documentation đầy đủ
- [ ] Sẵn sàng chuyển sang Perfboard

**Tiến độ tuần 3:** **_/36 ✓ (_**%)

---

## 📊 TRACKING TABLE

| Module        | Tested? | Working? | Issues | Solution | Status |
| ------------- | ------- | -------- | ------ | -------- | ------ |
| ESP32 WiFi    | ☐       | ☐        |        |          | ⏸️     |
| AD8232        | ☐       | ☐        |        |          | ⏸️     |
| ADS1115       | ☐       | ☐        |        |          | ⏸️     |
| MAX30102      | ☐       | ☐        |        |          | ⏸️     |
| MLX90614      | ☐       | ☐        |        |          | ⏸️     |
| TFT LCD       | ☐       | ☐        |        |          | ⏸️     |
| LVGL UI       | ☐       | ☐        |        |          | ⏸️     |
| Google Sheets | ☐       | ☐        |        |          | ⏸️     |

**Legend:** ✅ Done | 🔄 In Progress | ⏸️ Pending | ❌ Failed

---

## 🐛 BUG TRACKING

### Bug #1

- **Date:** **_/_**/\_\_\_
- **Module:** ********\_********
- **Description:** ****************\_****************
- **Solution:** ****************\_****************
- **Status:** ☐ Fixed ☐ Workaround ☐ Pending

### Bug #2

- **Date:** **_/_**/\_\_\_
- **Module:** ********\_********
- **Description:** ****************\_****************
- **Solution:** ****************\_****************
- **Status:** ☐ Fixed ☐ Workaround ☐ Pending

### Bug #3

- **Date:** **_/_**/\_\_\_
- **Module:** ********\_********
- **Description:** ****************\_****************
- **Solution:** ****************\_****************
- **Status:** ☐ Fixed ☐ Workaround ☐ Pending

_(Thêm nhiều bugs nếu cần)_

---

## 📸 PHOTO CHECKLIST

- [ ] Breadboard overview (top view)
- [ ] Breadboard side view
- [ ] ESP32 module closeup
- [ ] AD8232 + ADS1115 setup
- [ ] Wiring connections
- [ ] Power section
- [ ] LCD running with UI
- [ ] ECG waveform trên LCD
- [ ] Gắn điện cực thực tế
- [ ] Serial monitor output

**Lưu ảnh vào folder:** `images/breadboard/`

---

## 📝 DAILY LOG TEMPLATE

### Ngày **_/_**/2026

**Thời gian làm việc:** **\_** giờ

**Công việc đã làm:**

- ***
- ***
- ***

**Vấn đề gặp phải:**

- ***
- ***

**Đã giải quyết:**

- ***
- ***

**Chưa giải quyết:**

- ***
- ***

**Plan ngày mai:**

- ***
- ***

**Ghi chú thêm:**

---

---

---

## ✅ MILESTONE CHECKPOINTS

### Milestone 1: Basic Functionality (Tuần 1)

- [ ] ESP32 boot OK
- [ ] WiFi connect OK
- [ ] Ít nhất 1 sensor hoạt động
- [ ] LCD hiển thị được
- **Target date:** **_/_**/2026
- **Actual date:** **_/_**/2026
- **Status:** ☐ Achieved ☐ Delayed

### Milestone 2: Full Integration (Tuần 2)

- [ ] Tất cả sensors hoạt động
- [ ] UI hoàn chỉnh
- [ ] Gửi data lên cloud OK
- **Target date:** **_/_**/2026
- **Actual date:** **_/_**/2026
- **Status:** ☐ Achieved ☐ Delayed

### Milestone 3: Production Ready (Tuần 3)

- [ ] Stability test 24h pass
- [ ] Error handling đầy đủ
- [ ] Code clean & commented
- [ ] Documentation complete
- **Target date:** **_/_**/2026
- **Actual date:** **_/_**/2026
- **Status:** ☐ Achieved ☐ Delayed

---

## 🎯 READINESS CRITERIA (Trước khi thiết kế PCB)

**Code:**

- [ ] Chạy ổn định >24 giờ không crash
- [ ] Memory leak free
- [ ] Error handling đầy đủ
- [ ] Code được comment tốt
- [ ] Đã backup/version control

**Hardware:**

- [ ] Tất cả sensors test OK
- [ ] Power consumption đo được: **\_** mA
- [ ] Không có IC nào nóng >50°C
- [ ] Schematic vẽ lại từ breadboard
- [ ] GPIO map đầy đủ

**Documentation:**

- [ ] Ảnh breadboard đầy đủ
- [ ] Video demo
- [ ] Schematic
- [ ] BOM sơ bộ
- [ ] Issue log
- [ ] Test results

**Demo:**

- [ ] Đã demo cho thầy/bạn ≥1 lần
- [ ] Nhận feedback
- [ ] Fix theo feedback

---

## 🚦 GO/NO-GO DECISION

**SẴN SÀNG THIẾT KẾ PCB?**

☐ **GO** - Tất cả criteria đạt → Bắt đầu thiết kế PCB  
☐ **NO-GO** - Còn issues → Continue testing

**Lý do NO-GO (nếu có):**

---

---

**Date decision:** **_/_**/2026  
**Signed:** ********\_********

---

## 📞 SUPPORT CONTACTS

**Thầy hướng dẫn:**

- Tên: ********\_********
- Email: ********\_********
- Phone: ********\_********

**Bạn cùng nhóm:**

- Tên: ********\_********
- Phone: ********\_********

**Shop linh kiện:**

- Tên: ********\_********
- Address: ********\_********
- Phone: ********\_********

---

## 💾 BACKUP CHECKLIST

- [ ] Code backup lên GitHub/GitLab
- [ ] Document backup lên Google Drive
- [ ] Ảnh/video backup
- [ ] Schematic backup (multiple copies)
- [ ] BOM backup
- [ ] Test data backup

**Backup frequency:** ☐ Daily ☐ Weekly ☐ After major changes

---

**IN FILE NÀY RA VÀ TICK HÀNG NGÀY! 📋✅**
