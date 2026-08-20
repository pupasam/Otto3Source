#include "src/constants.h"
#include "src/LowLevelFns.h"
#include "src/OttoFns.h"
#include "src/RunProtocol.h"   // runAutomation() — shared with OttoPanel

// for testing, insert this line to stop the loop at that point: stopLoop();

char msg[64];  // static buffer, no heap, reads in 63 chars max

void loop() {

    //ASSERT((vacTime/mLPumpTime) >= 0.75); // vacTime should be sufficient to pump 0.75mL into the well

    if (Serial.available() > 0) {

        size_t len = Serial.readBytesUntil('\n', msg, sizeof(msg) - 1);
        msg[len] = '\0';  // ensure null termination

        // trim end
        while (len > 0 && (msg[len - 1] == '\r' || msg[len - 1] == ' ')) {
            msg[--len] = '\0';
        }

        // compare safely
        if (len >= 16 && strncmp(msg, "BEGIN AUTOMATION", 16) == 0) {
            runAutomation();
        }
    }
}
