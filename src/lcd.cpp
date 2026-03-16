#include <Arduino.h>
#include "ui.h"

bool in_menu = false;

// Hàm in bộ nhớ trống
void print_heap()
{
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" | Min Free Heap: ");
    Serial.println(ESP.getMinFreeHeap());
}

void guiTask(void *pvParameters)
{
    Serial.println("GUI Task Started");
    ui_init();

    Serial.println("System Started. Waiting 3s...");
    print_heap(); // Kiểm tra RAM lúc mới khởi động

    unsigned long startBoot = millis();

    while (1)
    {
        lv_timer_handler();
        delay(5);

        // LOGIC CHUYỂN MÀN HÌNH
        if (!in_menu && (millis() - startBoot > 3000))
        {
            in_menu = true;
            Serial.println("--- BEFORE SWITCHING ---");
            print_heap();

            Serial.println("Switching to Main Menu...");
            ui_switch_screen(SCR_MENU);

            Serial.println("--- AFTER SWITCHING ---");
            print_heap(); // <--- Xem RAM còn bao nhiêu sau khi tạo menu
        }

        // TẠM THỜI TẮT UPDATE SENSOR ĐỂ LOẠI TRỪ LỖI LOGIC
        /* static unsigned long lastUpdate = 0;
        if (in_menu && (millis() - lastUpdate > 50))
        {
            lastUpdate = millis();
            int fakeECG = 50 + random(-20, 20);
            ui_update_sensors(36.5, 75, 98, fakeECG);
        }
        */
    }
}

void setup()
{
    Serial.begin(115200);
    // Tăng Stack lên 32KB để chắc chắn không phải lỗi Stack
    xTaskCreatePinnedToCore(guiTask, "LVGL", 32768, NULL, 1, NULL, 1);
}

void loop() { delay(1000); }