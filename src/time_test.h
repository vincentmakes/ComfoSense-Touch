#ifndef TIME_TEST_SIMPLE_H
#define TIME_TEST_SIMPLE_H

// ============================================================================
// SIMPLIFIED TIME TEST SUITE (No external dependencies)
// ============================================================================
// This version only tests direct CAN commands without needing ComfoAir class
// Based on CAN dump showing time response on 0x10040001
//
// USAGE IN main.cpp:
//   #include "time_test_simple.h"
//   
//   void setup() {
//       // ... your setup ...
//       comfo->setup();  // Make sure CAN is initialized
//       delay(2000);
//       
//       TimeTest::runAllTests();
//   }
// ============================================================================

#include <Arduino.h>
#include "comfoair/twai_wrapper.h"

namespace TimeTest {

static CAN_FRAME test_msg;
static const uint32_t TEST_TIMEOUT = 3000; // 3 seconds

// Wait for time response on CAN ID 0x10040001
bool waitForTimeResponse(uint32_t timeout_ms) {
    unsigned long start = millis();
    CAN_FRAME incoming;
    
    Serial.println("  ⏳ Waiting for response...");
    
    while (millis() - start < timeout_ms) {
        if (CAN0.read(incoming)) {
            // Check for time response
            if (incoming.id == 0x10040001 && incoming.length >= 4) {
                Serial.println("  ✅ TIME RESPONSE RECEIVED!");
                
                // Decode time (little-endian)
                uint32_t device_seconds = incoming.data.uint8[0] | 
                                         (incoming.data.uint8[1] << 8) |
                                         (incoming.data.uint8[2] << 16) |
                                         (incoming.data.uint8[3] << 24);
                
                Serial.printf("  → Raw: 0x%08X (%u seconds since 2000-01-01)\n", 
                             device_seconds, device_seconds);
                
                // Rough time calculation
                uint32_t days = device_seconds / 86400;
                uint32_t hours = (device_seconds % 86400) / 3600;
                uint32_t minutes = (device_seconds % 3600) / 60;
                uint32_t secs = device_seconds % 60;
                
                Serial.printf("  → Time: ~%u days + %02u:%02u:%02u since 2000-01-01\n", 
                             days, hours, minutes, secs);
                Serial.printf("  → Approx date: ~year %u\n", 2000 + days / 365);
                
                return true;
            }
            
            // Show other responses (helpful for debugging)
            if (incoming.id != 0x10040001) {
                Serial.printf("  [Other CAN ID: 0x%08X, len=%d]\n", 
                             incoming.id, incoming.length);
            }
        }
        delay(10);
    }
    
    Serial.println("  ❌ TIMEOUT - No time response");
    return false;
}

void test1_RTR_0x10040028() {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║ TEST 1: RTR to 0x10040028 ⭐⭐⭐⭐⭐ (MOST LIKELY)          ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println("Evidence: RTR frame 10040028#R seen in CAN dump before time response");
    Serial.println("Method: Remote Transmission Request (empty frame)");
    
    test_msg.id = 0x10040028;
    test_msg.extended = true;
    test_msg.rtr = true;  // RTR frame
    test_msg.length = 0;
    
    Serial.print("\n📤 Sending RTR to 0x10040028... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("SENT ✓");
        waitForTimeResponse(TEST_TIMEOUT);
    } else {
        Serial.println("FAILED ✗");
    }
    
    delay(500);
}

void test2_CMD_0x10000028() {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║ TEST 2: Command 0x10000028 ⭐⭐⭐⭐                          ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println("Evidence: Frame 10000028#02010000 seen in dump before response");
    Serial.println("Method: Direct command with data [02 01 00 00]");
    
    test_msg.id = 0x10000028;
    test_msg.extended = true;
    test_msg.rtr = false;
    test_msg.length = 4;
    test_msg.data.uint8[0] = 0x02;
    test_msg.data.uint8[1] = 0x01;
    test_msg.data.uint8[2] = 0x00;
    test_msg.data.uint8[3] = 0x00;
    
    Serial.print("\n📤 Sending to 0x10000028 [02 01 00 00]... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("SENT ✓");
        waitForTimeResponse(TEST_TIMEOUT);
    } else {
        Serial.println("FAILED ✗");
    }
    
    delay(500);
}

void test3_RTR_0x10080028() {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║ TEST 3: RTR to 0x10080028 ⭐⭐⭐                             ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println("Evidence: ID 10080028 also appears in CAN dump");
    Serial.println("Method: Remote Transmission Request (alternative ID)");
    
    test_msg.id = 0x10080028;
    test_msg.extended = true;
    test_msg.rtr = true;
    test_msg.length = 0;
    
    Serial.print("\n📤 Sending RTR to 0x10080028... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("SENT ✓");
        waitForTimeResponse(TEST_TIMEOUT);
    } else {
        Serial.println("FAILED ✗");
    }
    
    delay(500);
}

void test4_Direct_0x10040001() {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║ TEST 4: RTR to 0x10040001 ⭐⭐                               ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println("Theory: Direct request to the response CAN ID");
    Serial.println("Method: RTR to response ID");
    
    test_msg.id = 0x10040001;
    test_msg.extended = true;
    test_msg.rtr = true;
    test_msg.length = 0;
    
    Serial.print("\n📤 Sending RTR to 0x10040001... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("SENT ✓");
        waitForTimeResponse(TEST_TIMEOUT);
    } else {
        Serial.println("FAILED ✗");
    }
    
    delay(500);
}

void test5_Combined() {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║ TEST 5: Combined sequence ⭐⭐⭐                              ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println("Method: Send both RTR and command as observed in dump sequence");
    
    // First: RTR
    test_msg.id = 0x10040028;
    test_msg.extended = true;
    test_msg.rtr = true;
    test_msg.length = 0;
    
    Serial.print("\n📤 Step 1: RTR to 0x10040028... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("SENT ✓");
    } else {
        Serial.println("FAILED ✗");
    }
    
    delay(100);
    
    // Second: Command
    test_msg.id = 0x10000028;
    test_msg.extended = true;
    test_msg.rtr = false;
    test_msg.length = 4;
    test_msg.data.uint8[0] = 0x02;
    test_msg.data.uint8[1] = 0x01;
    test_msg.data.uint8[2] = 0x00;
    test_msg.data.uint8[3] = 0x00;
    
    Serial.print("📤 Step 2: CMD to 0x10000028 [02 01 00 00]... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("SENT ✓");
        waitForTimeResponse(TEST_TIMEOUT);
    } else {
        Serial.println("FAILED ✗");
    }
    
    delay(500);
}

void runAllTests() {
    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════════════════════════════╗");
    Serial.println("║                                                                   ║");
    Serial.println("║        MVHR TIME COMMAND - AUTOMATED TEST SUITE                   ║");
    Serial.println("║                                                                   ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("📋 OBJECTIVE:");
    Serial.println("   Find which CAN command triggers time response on ID 0x10040001");
    Serial.println();
    Serial.println("🔍 CAN DUMP ANALYSIS CONFIRMED:");
    Serial.println("   ✅ Time response: CAN ID 0x10040001 (4 bytes, little-endian)");
    Serial.println("   ✅ Format: Seconds since 2000-01-01 00:00:00");
    Serial.println("   ✅ Verified: Your time change 15:45→15:47 decoded correctly!");
    Serial.println("   ⭐ Best candidate: RTR frame to 0x10040028");
    Serial.println();
    Serial.println("⚡ Running 5 test formats (3 seconds timeout each)...");
    Serial.println();
    
    delay(2000);
    
    // Run all tests
    test1_RTR_0x10040028();      // ⭐⭐⭐⭐⭐ Most likely
    test2_CMD_0x10000028();       // ⭐⭐⭐⭐ Also in dump
    test3_RTR_0x10080028();       // ⭐⭐⭐ Alternative
    test4_Direct_0x10040001();    // ⭐⭐ Worth trying
    test5_Combined();             // ⭐⭐⭐ Exact sequence
    
    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════════════════════════════╗");
    Serial.println("║                      TEST SUITE COMPLETE                          ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("📊 RESULTS:");
    Serial.println("   Look above for tests with '✅ TIME RESPONSE RECEIVED!'");
    Serial.println();
    Serial.println("💡 NEXT STEPS:");
    Serial.println("   1. Note which test succeeded");
    Serial.println("   2. Report the test number and decoded time");
    Serial.println("   3. We'll update message.cpp with the working format");
    Serial.println();
}

} // namespace TimeTest

#endif // TIME_TEST_SIMPLE_H