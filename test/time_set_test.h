#ifndef TIME_SET_TEST_H
#define TIME_SET_TEST_H

// ============================================================================
// TIME SET DEBUG TEST
// ============================================================================
// Tests different CAN IDs to find which one actually sets MVHR time
// 
// USAGE:
// 1. Include this in main.cpp: #include "time_set_test.h"
// 2. Call: TimeSetTest::runTest() from Serial command or setup
// 3. Check MVHR display after each test to see if time changed
// ============================================================================

#include <Arduino.h>
#include "comfoair/twai_wrapper.h"

namespace TimeSetTest {

static CAN_FRAME test_msg;

// Calculate seconds since 2000 for a specific time
uint32_t calculateSeconds(uint8_t hour, uint8_t minute) {
    // Current date: 2025-10-26
    // Days from 2000-01-01 to 2025-10-26
    uint32_t days = 9430;  // From your test result
    uint32_t seconds_today = (hour * 3600) + (minute * 60);
    return (days * 86400) + seconds_today;
}

void testSetTime(uint32_t can_id, const char* description, uint8_t hour, uint8_t minute) {
    Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
    Serial.printf("║ Testing: %s\n", description);
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    
    uint32_t target_seconds = calculateSeconds(hour, minute);
    
    Serial.printf("Target time: %02d:%02d (approx %u seconds since 2000)\n", 
                 hour, minute, target_seconds);
    Serial.printf("CAN ID: 0x%08X\n\n", can_id);
    
    // Build the message
    test_msg.id = can_id;
    test_msg.extended = true;
    test_msg.rtr = false;
    test_msg.length = 4;
    
    // Pack time as little-endian
    test_msg.data.uint8[0] = (target_seconds) & 0xFF;
    test_msg.data.uint8[1] = (target_seconds >> 8) & 0xFF;
    test_msg.data.uint8[2] = (target_seconds >> 16) & 0xFF;
    test_msg.data.uint8[3] = (target_seconds >> 24) & 0xFF;
    
    Serial.printf("Data bytes: [%02X %02X %02X %02X]\n", 
                 test_msg.data.uint8[0], test_msg.data.uint8[1],
                 test_msg.data.uint8[2], test_msg.data.uint8[3]);
    
    Serial.print("Sending... ");
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("✓ SENT");
        
        // Wait a bit for MVHR to process
        delay(500);
        
        Serial.println("\n⚠️  CHECK YOUR MVHR DISPLAY NOW!");
        Serial.printf("   Expected time: %02d:%02d\n", hour, minute);
        Serial.println("   Did the time change? (y/n)");
        Serial.println("\nWaiting 10 seconds for you to check...");
        
        // Wait for user to check
        unsigned long start = millis();
        bool answered = false;
        
        while (millis() - start < 10000) {
            if (Serial.available()) {
                char response = Serial.read();
                if (response == 'y' || response == 'Y') {
                    Serial.println("\n🎉 SUCCESS! This CAN ID works for setting time!");
                    Serial.printf("✅ WORKING: CAN ID 0x%08X\n", can_id);
                    answered = true;
                    break;
                } else if (response == 'n' || response == 'N') {
                    Serial.println("\n❌ No change - trying next option...");
                    answered = true;
                    break;
                }
            }
            delay(100);
        }
        
        if (!answered) {
            Serial.println("\n⏭️  No response - continuing to next test...");
        }
        
    } else {
        Serial.println("✗ FAILED TO SEND");
    }
    
    delay(1000);
}

void runTest() {
    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════════════════════════════╗");
    Serial.println("║                                                                   ║");
    Serial.println("║              TIME SET COMMAND - DEBUG TEST                        ║");
    Serial.println("║                                                                   ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("📋 OBJECTIVE:");
    Serial.println("   Find which CAN ID can SET the MVHR time");
    Serial.println();
    Serial.println("🔍 WHAT WE KNOW:");
    Serial.println("   ✅ Read time: RTR to 0x10080028 → Response on 0x10040001");
    Serial.println("   ❓ Set time: Unknown CAN ID");
    Serial.println();
    Serial.println("🧪 TEST STRATEGY:");
    Serial.println("   We'll try setting time to UNIQUE minutes on each test");
    Serial.println("   Check your MVHR display after each test!");
    Serial.println();
    Serial.println("⚠️  IMPORTANT:");
    Serial.println("   - Keep your MVHR display visible");
    Serial.println("   - After each test, type 'y' if time changed, 'n' if not");
    Serial.println("   - We'll set time to: 14:11, 14:12, 14:13, etc.");
    Serial.println();
    Serial.println("Press any key to start...");
    
    while (!Serial.available()) {
        delay(100);
    }
    while (Serial.available()) Serial.read(); // Clear buffer
    
    Serial.println("\n🚀 Starting tests...\n");
    delay(1000);
    
    // Test 1: Try 0x10080001 (related to read request ID)
    testSetTime(0x10080001, "CAN ID 0x10080001 (Related to request 0x10080028)", 14, 11);
    
    // Test 2: Try 0x10040001 (the response ID where time is read from)
    testSetTime(0x10040001, "CAN ID 0x10040001 (Time response ID)", 14, 12);
    
    // Test 3: Try 0x10080028 with data (request ID but with data instead of RTR)
    testSetTime(0x10080028, "CAN ID 0x10080028 (Request ID with data)", 14, 13);
    
    // Test 4: Try 0x10040028 (seen in CAN dump)
    testSetTime(0x10040028, "CAN ID 0x10040028 (From CAN dump)", 14, 14);
    
    // Test 5: Try 0x10000028 (also in dump)
    testSetTime(0x10000028, "CAN ID 0x10000028 (From CAN dump)", 14, 15);
    
    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════════════════════════════╗");
    Serial.println("║                      TEST COMPLETE                                ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("📊 RESULTS:");
    Serial.println("   If any test showed '🎉 SUCCESS!' above, that CAN ID works!");
    Serial.println();
    Serial.println("🤔 IF NONE WORKED:");
    Serial.println("   We need to capture a CAN dump while manually changing time");
    Serial.println("   on the MVHR to see the actual SET command");
    Serial.println();
    Serial.println("📝 NEXT STEPS:");
    Serial.println("   Report which test (if any) changed the MVHR display time");
    Serial.println();
}

// Alternative: Manual single test
void testSingleTime(uint32_t can_id, uint8_t hour, uint8_t minute) {
    Serial.printf("\n🧪 Testing single time set: CAN ID 0x%08X, Time %02d:%02d\n", 
                 can_id, hour, minute);
    
    uint32_t target_seconds = calculateSeconds(hour, minute);
    
    test_msg.id = can_id;
    test_msg.extended = true;
    test_msg.rtr = false;
    test_msg.length = 4;
    test_msg.data.uint8[0] = (target_seconds) & 0xFF;
    test_msg.data.uint8[1] = (target_seconds >> 8) & 0xFF;
    test_msg.data.uint8[2] = (target_seconds >> 16) & 0xFF;
    test_msg.data.uint8[3] = (target_seconds >> 24) & 0xFF;
    
    if (CAN0.sendFrame(test_msg)) {
        Serial.println("✓ Command sent - check MVHR display!");
    } else {
        Serial.println("✗ Failed to send");
    }
}

} // namespace TimeSetTest

#endif // TIME_SET_TEST_H
