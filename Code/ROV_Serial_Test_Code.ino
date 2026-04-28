/*
 * ============================================================
 *  ROV PCB TEST UTILITY — Teensy 4.1 (USB Serial)
 * ============================================================
 *  Connects via USB Serial at 115200 baud.
 *  Use the Python client (test_client.py) or Arduino Serial
 *  Monitor to control.
 *
 *  Thrusters 1-6  → pins 2,3,4,5,6,7   (Servo PWM 1100-1900 µs)
 *  Motor Drivers 1-8 → pins 26,27,30,31,32,33,34,35  (PWM 1100-1900 µs)
 *
 *  All outputs use the Servo library (software-timed pulses)
 *  so every pin works regardless of hardware-PWM capability.
 * ============================================================
 */

#include <Servo.h>

// ======================== PIN MAP ========================
const uint8_t THRUSTER_PINS[]      = { 2,  3,  4,  5,  6,  7 };
const uint8_t NUM_THRUSTERS        = 6;
#include <Servo.h>

Servo myESC;
int pwmValue = 1500;

void setup() {
  Serial.begin(9600);
  myESC.attach(9);

  Serial.println("Arming ESC...");
  myESC.writeMicroseconds(1500);
  delay(3000);
  Serial.println("Armed! Type a PWM value (e.g. 1100 to 1900) and press Enter:");
}

void loop() {
  if (Serial.available() > 0) {
    pwmValue = Serial.parseInt();  // reads the number you type

    if (pwmValue >= 1000 && pwmValue <= 2000) {
      myESC.writeMicroseconds(pwmValue);
      Serial.print("Sending: ");
      Serial.print(pwmValue);
      Serial.println(" us");
    } else {
      Serial.println("Out of range! Use 1000–2000.");
    }
  }
}
const uint8_t MOTOR_DRIVER_PINS[]  = { 26, 27, 30, 31, 32, 33, 34, 35 };
const uint8_t NUM_MOTOR_DRIVERS    = 8;

// ======================== OBJECTS ========================
Servo thrusterServo[NUM_THRUSTERS];
Servo mdServo[NUM_MOTOR_DRIVERS];

int thrusterPWM[NUM_THRUSTERS];
int mdPWM[NUM_MOTOR_DRIVERS];

// ===================== STATE MACHINE =====================
enum State {
    ST_MAIN_MENU,
    ST_SELECT_THRUSTER,
    ST_THRUSTER_PWM,
    ST_SELECT_MD,
    ST_MD_PWM,
    ST_SWEEP_THRUSTER_SELECT,
    ST_SWEEP_MD_SELECT,
    ST_SWEEP_RUNNING,
};

State   state         = ST_MAIN_MENU;
int     selectedIndex = -1;
String  inputBuffer   = "";

// Sweep state
int     sweepIndex    = 0;
int     sweepPWM      = 1500;
int     sweepDir      = 1;
int     sweepPhase    = 0;        // 0=up to 1900, 1=down to 1100, 2=back to 1500
unsigned long sweepLastMs = 0;
const int SWEEP_STEP  = 10;       // µs per tick
const int SWEEP_DELAY = 50;       // ms between ticks
bool    sweepIsThruster = true;

// ======================== HELPERS ========================

void printDivider() {
    Serial.println("================================================");
}

void printHeader() {
    Serial.println();
    printDivider();
    Serial.println("       TEENSY 4.1  ROV PCB TEST UTILITY");
    printDivider();
}

// ====================== MENUS ============================

void showMainMenu() {
    state = ST_MAIN_MENU;
    printHeader();
    Serial.println();
    Serial.println("  [1]  Test Individual Thruster");
    Serial.println("  [2]  Test Individual Motor Driver");
    Serial.println("  [3]  Sweep Single Thruster (auto ramp)");
    Serial.println("  [4]  Sweep Single Motor Driver (auto ramp)");
    Serial.println("  [5]  Sweep ALL Outputs (sequential)");
    Serial.println("  [6]  Set ALL Outputs to Neutral (1500)");
    Serial.println("  [7]  Show Current Status");
    Serial.println();
    Serial.print("  Enter choice [1-7]: ");
}

void showThrusterSelect() {
    state = ST_SELECT_THRUSTER;
    Serial.println();
    printDivider();
    Serial.println("  SELECT THRUSTER");
    printDivider();
    for (int i = 0; i < NUM_THRUSTERS; i++) {
        Serial.println("  [" + String(i + 1) + "]  Thruster " + String(i + 1)
                    + "  (pin " + String(THRUSTER_PINS[i]) + ")"
                    + "  -  current: " + String(thrusterPWM[i]) + " us");
    }
    Serial.println("  [0]  Back to Main Menu");
    Serial.println();
    Serial.print("  Enter thruster number: ");
}

void showMDSelect() {
    state = ST_SELECT_MD;
    Serial.println();
    printDivider();
    Serial.println("  SELECT MOTOR DRIVER");
    printDivider();
    for (int i = 0; i < NUM_MOTOR_DRIVERS; i++) {
        Serial.println("  [" + String(i + 1) + "]  MD" + String(i + 1)
                    + "  (pin " + String(MOTOR_DRIVER_PINS[i]) + ")"
                    + "  -  current: " + String(mdPWM[i]) + " us");
    }
    Serial.println("  [0]  Back to Main Menu");
    Serial.println();
    Serial.print("  Enter motor driver number: ");
}

void promptPWM(const char* label, int index, int currentVal) {
    Serial.println();
    Serial.println(String("  ") + label + " " + String(index + 1)
               + " selected  (current: " + String(currentVal) + " us)");
    Serial.println("  Valid range: 1100 - 1900  (1500 = neutral/stop)");
    Serial.print("  Enter PWM value: ");
}

void showStatus() {
    Serial.println();
    printDivider();
    Serial.println("  CURRENT STATUS");
    printDivider();
    Serial.println();
    Serial.println("  THRUSTERS:");
    for (int i = 0; i < NUM_THRUSTERS; i++) {
        Serial.println("    T" + String(i + 1)
                    + "  pin " + String(THRUSTER_PINS[i])
                    + "  =  " + String(thrusterPWM[i]) + " us");
    }
    Serial.println();
    Serial.println("  MOTOR DRIVERS:");
    for (int i = 0; i < NUM_MOTOR_DRIVERS; i++) {
        Serial.println("    MD" + String(i + 1)
                    + "  pin " + String(MOTOR_DRIVER_PINS[i])
                    + "  =  " + String(mdPWM[i]) + " us");
    }
    Serial.println();
    showMainMenu();
}

void setAllNeutral() {
    for (int i = 0; i < NUM_THRUSTERS; i++) {
        thrusterPWM[i] = 1500;
        thrusterServo[i].writeMicroseconds(1500);
    }
    for (int i = 0; i < NUM_MOTOR_DRIVERS; i++) {
        mdPWM[i] = 1500;
        mdServo[i].writeMicroseconds(1500);
    }
    Serial.println();
    Serial.println("  >> All outputs set to 1500 (neutral).");
    showMainMenu();
}

// ==================== INPUT HANDLING =====================

void processInput(String input) {
    input.trim();
    if (input.length() == 0) return;

    switch (state) {

        case ST_MAIN_MENU: {
            int choice = input.toInt();
            switch (choice) {
                case 1: showThrusterSelect(); break;
                case 2: showMDSelect();       break;
                case 3: {
                    Serial.println();
                    printDivider();
                    Serial.println("  SWEEP THRUSTER");
                    printDivider();
                    for (int i = 0; i < NUM_THRUSTERS; i++) {
                        Serial.println("  [" + String(i + 1) + "]  Thruster " + String(i + 1)
                                    + "  (pin " + String(THRUSTER_PINS[i]) + ")");
                    }
                    Serial.println("  [0]  Back");
                    Serial.print("  Select thruster to sweep: ");
                    state = ST_SWEEP_THRUSTER_SELECT;
                    break;
                }
                case 4: {
                    Serial.println();
                    printDivider();
                    Serial.println("  SWEEP MOTOR DRIVER");
                    printDivider();
                    for (int i = 0; i < NUM_MOTOR_DRIVERS; i++) {
                        Serial.println("  [" + String(i + 1) + "]  MD" + String(i + 1)
                                    + "  (pin " + String(MOTOR_DRIVER_PINS[i]) + ")");
                    }
                    Serial.println("  [0]  Back");
                    Serial.print("  Select motor driver to sweep: ");
                    state = ST_SWEEP_MD_SELECT;
                    break;
                }
                case 5: startSweepAll(); break;
                case 6: setAllNeutral();  break;
                case 7: showStatus();     break;
                default:
                    Serial.println("  !! Invalid choice. Try again.");
                    Serial.print("  Enter choice [1-7]: ");
                    break;
            }
            break;
        }

        case ST_SELECT_THRUSTER: {
            int sel = input.toInt();
            if (sel == 0) { showMainMenu(); break; }
            if (sel < 1 || sel > NUM_THRUSTERS) {
                Serial.println("  !! Invalid. Enter 1-" + String(NUM_THRUSTERS) + " or 0 to go back.");
                Serial.print("  Enter thruster number: ");
                break;
            }
            selectedIndex = sel - 1;
            state = ST_THRUSTER_PWM;
            promptPWM("Thruster", selectedIndex, thrusterPWM[selectedIndex]);
            break;
        }

        case ST_THRUSTER_PWM: {
            if (input.equalsIgnoreCase("b") || input == "0") {
                showThrusterSelect();
                break;
            }
            int pwm = input.toInt();
            if (pwm < 1100 || pwm > 1900) {
                Serial.println("  !! Out of range. Enter 1100-1900, or 'b' to go back.");
                Serial.print("  Enter PWM value: ");
                break;
            }
            thrusterPWM[selectedIndex] = pwm;
            thrusterServo[selectedIndex].writeMicroseconds(pwm);
            Serial.println();
            Serial.println("  >> Thruster " + String(selectedIndex + 1) + " set to " + String(pwm) + " us");
            Serial.println();
            Serial.print("  Enter new PWM value (or 'b' to go back): ");
            break;
        }

        case ST_SELECT_MD: {
            int sel = input.toInt();
            if (sel == 0) { showMainMenu(); break; }
            if (sel < 1 || sel > NUM_MOTOR_DRIVERS) {
                Serial.println("  !! Invalid. Enter 1-" + String(NUM_MOTOR_DRIVERS) + " or 0 to go back.");
                Serial.print("  Enter motor driver number: ");
                break;
            }
            selectedIndex = sel - 1;
            state = ST_MD_PWM;
            promptPWM("Motor Driver", selectedIndex, mdPWM[selectedIndex]);
            break;
        }

        case ST_MD_PWM: {
            if (input.equalsIgnoreCase("b") || input == "0") {
                showMDSelect();
                break;
            }
            int pwm = input.toInt();
            if (pwm < 1100 || pwm > 1900) {
                Serial.println("  !! Out of range. Enter 1100-1900, or 'b' to go back.");
                Serial.print("  Enter PWM value: ");
                break;
            }
            mdPWM[selectedIndex] = pwm;
            mdServo[selectedIndex].writeMicroseconds(pwm);
            Serial.println();
            Serial.println("  >> Motor Driver " + String(selectedIndex + 1) + " set to " + String(pwm) + " us");
            Serial.println();
            Serial.print("  Enter new PWM value (or 'b' to go back): ");
            break;
        }

        case ST_SWEEP_THRUSTER_SELECT: {
            int sel = input.toInt();
            if (sel == 0) { showMainMenu(); break; }
            if (sel < 1 || sel > NUM_THRUSTERS) {
                Serial.println("  !! Invalid.");
                Serial.print("  Select thruster to sweep: ");
                break;
            }
            selectedIndex = sel - 1;
            sweepIsThruster = true;
            startSweepSingle();
            break;
        }

        case ST_SWEEP_MD_SELECT: {
            int sel = input.toInt();
            if (sel == 0) { showMainMenu(); break; }
            if (sel < 1 || sel > NUM_MOTOR_DRIVERS) {
                Serial.println("  !! Invalid.");
                Serial.print("  Select motor driver to sweep: ");
                break;
            }
            selectedIndex = sel - 1;
            sweepIsThruster = false;
            startSweepSingle();
            break;
        }

        case ST_SWEEP_RUNNING: {
            if (input.equalsIgnoreCase("s")) {
                stopSweep();
            }
            break;
        }

        default:
            showMainMenu();
            break;
    }
}

// ====================== SWEEP LOGIC ======================

void startSweepSingle() {
    sweepPWM    = 1500;
    sweepDir    = 1;
    sweepPhase  = 0;
    sweepLastMs = millis();

    String label = sweepIsThruster
        ? ("Thruster " + String(selectedIndex + 1) + " (pin " + String(THRUSTER_PINS[selectedIndex]) + ")")
        : ("MD" + String(selectedIndex + 1) + " (pin " + String(MOTOR_DRIVER_PINS[selectedIndex]) + ")");

    Serial.println();
    Serial.println("  >> Sweeping " + label);
    Serial.println("     1500 -> 1900 -> 1100 -> 1500");
    Serial.println("     Send 's' to stop anytime.");
    Serial.println();

    state = ST_SWEEP_RUNNING;
}

void startSweepAll() {
    sweepIndex  = 0;
    sweepPWM    = 1500;
    sweepDir    = 1;
    sweepPhase  = 0;
    sweepLastMs = millis();
    sweepIsThruster = true;
    selectedIndex = -1;

    Serial.println();
    printDivider();
    Serial.println("  SWEEP ALL OUTPUTS (Sequential)");
    printDivider();
    Serial.println("  Will sweep each output: 1500 -> 1900 -> 1100 -> 1500");
    Serial.println("  Send 's' to stop anytime.");
    Serial.println();

    printCurrentSweepTarget();
    state = ST_SWEEP_RUNNING;
}

void printCurrentSweepTarget() {
    if (sweepIsThruster) {
        Serial.println("  >> Now sweeping: Thruster " + String(sweepIndex + 1)
               + " (pin " + String(THRUSTER_PINS[sweepIndex]) + ")");
    } else {
        Serial.println("  >> Now sweeping: MD" + String(sweepIndex + 1)
               + " (pin " + String(MOTOR_DRIVER_PINS[sweepIndex]) + ")");
    }
}

void stopSweep() {
    if (sweepIsThruster) {
        if (selectedIndex >= 0 && selectedIndex < NUM_THRUSTERS) {
            thrusterServo[selectedIndex].writeMicroseconds(1500);
            thrusterPWM[selectedIndex] = 1500;
        }
        if (sweepIndex < NUM_THRUSTERS) {
            thrusterServo[sweepIndex].writeMicroseconds(1500);
            thrusterPWM[sweepIndex] = 1500;
        }
    } else {
        if (selectedIndex >= 0 && selectedIndex < NUM_MOTOR_DRIVERS) {
            mdServo[selectedIndex].writeMicroseconds(1500);
            mdPWM[selectedIndex] = 1500;
        }
        if (sweepIndex < NUM_MOTOR_DRIVERS) {
            mdServo[sweepIndex].writeMicroseconds(1500);
            mdPWM[sweepIndex] = 1500;
        }
    }

    Serial.println();
    Serial.println("  >> Sweep stopped. Output returned to 1500 (neutral).");
    showMainMenu();
}

void updateSweep() {
    if (state != ST_SWEEP_RUNNING) return;
    if (millis() - sweepLastMs < SWEEP_DELAY) return;
    sweepLastMs = millis();

    sweepPWM += (SWEEP_STEP * sweepDir);

    bool isSingleSweep = (selectedIndex >= 0);

    if (sweepPhase == 0 && sweepPWM >= 1900) {
        sweepPWM = 1900;
        sweepDir = -1;
        sweepPhase = 1;
    } else if (sweepPhase == 1 && sweepPWM <= 1100) {
        sweepPWM = 1100;
        sweepDir = 1;
        sweepPhase = 2;
    } else if (sweepPhase == 2 && sweepPWM >= 1500) {
        sweepPWM = 1500;
        sweepFinishCurrent(isSingleSweep);
        return;
    }

    // Apply PWM
    if (isSingleSweep) {
        if (sweepIsThruster) {
            thrusterServo[selectedIndex].writeMicroseconds(sweepPWM);
            thrusterPWM[selectedIndex] = sweepPWM;
        } else {
            mdServo[selectedIndex].writeMicroseconds(sweepPWM);
            mdPWM[selectedIndex] = sweepPWM;
        }
    } else {
        if (sweepIsThruster) {
            thrusterServo[sweepIndex].writeMicroseconds(sweepPWM);
            thrusterPWM[sweepIndex] = sweepPWM;
        } else {
            mdServo[sweepIndex].writeMicroseconds(sweepPWM);
            mdPWM[sweepIndex] = sweepPWM;
        }
    }

    // Print progress every 100µs change
    static int lastPrinted = 1500;
    if (abs(sweepPWM - lastPrinted) >= 100) {
        Serial.println("     PWM = " + String(sweepPWM) + " us");
        lastPrinted = sweepPWM;
    }
}

void sweepFinishCurrent(bool isSingle) {
    if (isSingle) {
        if (sweepIsThruster) {
            thrusterServo[selectedIndex].writeMicroseconds(1500);
            thrusterPWM[selectedIndex] = 1500;
        } else {
            mdServo[selectedIndex].writeMicroseconds(1500);
            mdPWM[selectedIndex] = 1500;
        }
        Serial.println("  >> Sweep complete. Output at 1500 (neutral).");
        showMainMenu();
        return;
    }

    // Sweep-all mode: advance to next output
    if (sweepIsThruster) {
        thrusterServo[sweepIndex].writeMicroseconds(1500);
        thrusterPWM[sweepIndex] = 1500;
        sweepIndex++;
        if (sweepIndex >= NUM_THRUSTERS) {
            sweepIsThruster = false;
            sweepIndex = 0;
        }
    } else {
        mdServo[sweepIndex].writeMicroseconds(1500);
        mdPWM[sweepIndex] = 1500;
        sweepIndex++;
        if (sweepIndex >= NUM_MOTOR_DRIVERS) {
            Serial.println();
            Serial.println("  >> ALL outputs swept successfully!");
            showMainMenu();
            return;
        }
    }

    sweepPWM   = 1500;
    sweepDir   = 1;
    sweepPhase = 0;
    Serial.println();
    printCurrentSweepTarget();
}

// ======================== SETUP ==========================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {} // wait up to 3s for USB serial
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  ROV PCB Test Utility - Teensy 4.1");
    Serial.println("  USB Serial @ 115200 baud");
    Serial.println("========================================");
    Serial.println();

    // Attach all servo outputs and set neutral
    Serial.print("Attaching thrusters... ");
    for (int i = 0; i < NUM_THRUSTERS; i++) {
        thrusterServo[i].attach(THRUSTER_PINS[i]);
        thrusterServo[i].writeMicroseconds(1500);
        thrusterPWM[i] = 1500;
    }
    Serial.println("OK");

    Serial.print("Attaching motor drivers... ");
    for (int i = 0; i < NUM_MOTOR_DRIVERS; i++) {
        mdServo[i].attach(MOTOR_DRIVER_PINS[i]);
        mdServo[i].writeMicroseconds(1500);
        mdPWM[i] = 1500;
    }
    Serial.println("OK");

    Serial.println("All outputs set to 1500 us (neutral).");
    Serial.println();
    Serial.println("Arming ESCs / Motor Drivers...");
    delay(3000);
    Serial.println("Armed and ready!");
    Serial.println();

    showMainMenu();
}

// ========================= LOOP ==========================
void loop() {
    // Read incoming serial data
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            if (inputBuffer.length() > 0) {
                processInput(inputBuffer);
                inputBuffer = "";
            }
        } else if (c != '\r') {
            inputBuffer += c;
        }
    }

    // Drive sweep if active
    updateSweep();
}
