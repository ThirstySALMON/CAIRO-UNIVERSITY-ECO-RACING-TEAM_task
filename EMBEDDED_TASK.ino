#include <Arduino.h>


typedef struct {
    char     type;
    int16_t  value;
    uint32_t timestamp_ms;
} Command_t;

 QueueHandle_t commandQueue;


volatile bool     braking        = false;   // owned by actuate
volatile uint32_t latestCommand_ms = 0;
volatile char     latestCommand;
// CHANGE TO 600 BFR SUBMIT
volatile int16_t  linkLost_time = 5000; // in ms

volatile uint8_t  throttleTarget = 0;
volatile int16_t  latestSteer    = 0;
volatile bool     linkLost       = false;   // set by WATCHDOG, read by actuate


void Actuator_SetOutput() {

    static uint8_t currentThrottle = 0; // keep a single value on the stack between calls

    if (braking) { // immediately brake
        currentThrottle = 0;
    }else {
        if      (currentThrottle < throttleTarget) currentThrottle++;
        else if (currentThrottle > throttleTarget) currentThrottle--;
    }


    int pwm = map(currentThrottle, 0, 100, 0, 1023);
    ledcWrite(2, pwm);

}
void FailSafe_Blink() {
    static bool ledOn = false;
    static uint32_t lastToggle = 0;
    const uint32_t BLINK_MS = 200;

    uint32_t now = millis();
    if (now - lastToggle >= BLINK_MS) {
        lastToggle = now;
        ledOn = !ledOn;
        ledcWrite(2, ledOn ? 512 : 0);   // half brightness so can be different from throttle pwm
    }
}
void commandRxTask(void *pv) {
    char lineBuf[32];
    uint8_t idx = 0;

    for (;;) {
        while (Serial.available()) {
            char c = Serial.read();

            if (c == '\n' || c == '\r') {
                if (idx == 0) continue;
                lineBuf[idx] = '\0';
               // Serial.print("PARSED: "); Serial.println(lineBuf);
                idx = 0;


                char keyword[16];
                if (sscanf(lineBuf, "%15s", keyword) != 1) continue;


                Command_t cmd;
                cmd.value = 0;

                if      (strcmp(keyword, "THROTTLE") == 0) cmd.type = 'T';
                else if (strcmp(keyword, "STEER")    == 0) cmd.type = 'S';
                else if (strcmp(keyword, "BRAKE")    == 0) cmd.type = 'B';
                else if (strcmp(keyword, "PING")     == 0) cmd.type = 'P';
                else continue;


                if (cmd.type != 'P') {
                    int value;
                    if (sscanf(lineBuf, "%*s %d", &value) != 1) continue;   // missing/garbage number

                    switch (cmd.type) {
                        case 'T':
                            if (value > 100 || value < 0) continue;
                            break;
                        case 'S':
                            if (value > 100 || value < -100) continue;
                            break;
                        case 'B':
                            if (value > 100 || value < 0) continue;
                            break;

                        default:
                            break;
                    }



                    cmd.value = (int16_t)value;
                }


                cmd.timestamp_ms = millis();
                latestCommand    = cmd.type;
                latestCommand_ms = cmd.timestamp_ms;




                bool ok;
                if (cmd.type == 'B') ok = xQueueSendToFront(commandQueue, &cmd, 0);
                else                 ok = xQueueSend(commandQueue, &cmd, 0);


                    if (!ok && cmd.type == 'B') {
                        Command_t discard;
                        xQueueReceive(commandQueue, &discard, 0);      // make room, don't touch cmd
                        xQueueSendToFront(commandQueue, &cmd, 0);      // now the brake goes in
                    }
             //   Serial.print("SENT: "); Serial.println(cmd.type);    // check what entered queue

            } else if (idx < sizeof(lineBuf) - 1) {
                lineBuf[idx++] = c;
            }

        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void actuateTask(void *pv) {
   const TickType_t timeout = pdMS_TO_TICKS(5);
    Command_t cmd;

    for (;;) {
        while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case 'B':
                    braking = (cmd.value > 0);
                    throttleTarget = 0;
                    break;
                case 'T':
                    if (!braking) {
                        throttleTarget = cmd.value;
                    }
                    break;
                case 'S':
                   latestSteer = cmd.value;
                    break;
                case 'P':
                   Serial.println("Ping Acknowledged");
                    break;
                default:
                    break;
            }

        }



        if (linkLost) {
            FailSafe_Blink();
        }
        else {
          Actuator_SetOutput();
        }


        if (xQueuePeek(commandQueue, &cmd, timeout) == pdTRUE) {
            continue;               // something arrived — loop, drain handles it
        }
    }
}

void watchdogTask(void* pv) {
    bool wasLost = false; // to fire once
    for (;;) {
        linkLost = (millis() - latestCommand_ms > linkLost_time); // PUT IN VAR TO TEST 
        if (linkLost && !wasLost) {
            Serial.println("LINK LOST, failing safe");
        }
        wasLost = linkLost;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void statusTask(void* pv) {
    for (;;) {
        uint32_t lastWaketime = millis();
        if (!linkLost) {
            Serial.printf("Uptime: %d\n"
                          "Last Command Received: %c\n"
                          "Last Command Received Time: %d\n"
                          "Steer: %d Throttle: %d\n", lastWaketime ,latestCommand ,latestCommand_ms, latestSteer ,throttleTarget );
        }


        vTaskDelayUntil(&lastWaketime , pdMS_TO_TICKS(1000));
    }
}
void setup() {
    Serial.begin(115200);
    commandQueue = xQueueCreate(20, sizeof(Command_t));
    ledcAttach(2, 5000, 10);  // 5 kHz, 10-bit

    xTaskCreatePinnedToCore(commandRxTask, "RX", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(actuateTask, "ACTUATE", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(watchdogTask, "WATCHDOG", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(statusTask, "STATUS", 4096, NULL, 1, NULL, 1);



}

void loop() {}

