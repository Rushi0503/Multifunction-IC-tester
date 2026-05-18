//display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ACS712.h>

#define CURRENT_PIN 12          
#define VCCc 5.0                 // Supply voltage to ACS712
#define ADC_RESOLUTION 4095     
#define SENSITIVITY 100.0       // Sensitivity in mV/A for 30A module

ACS712 ACS(12, VCCc, ADC_RESOLUTION, SENSITIVITY);

#define CONTINUITY_PIN 15     // GPIO15 for analog input
#define CONTINUITY_THRESHOLD 1.0  // Voltage threshold in volts

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Button Pins (Active High)
#define UP_BUTTON 23
#define DOWN_BUTTON 19
#define NEXT_BUTTON 18

const int total_modes0 = 8;  // 0-6 are measurement modes, 7 is Inductance
int mode_index0 = 0;

const int total_modes1 = 3;
int mode_index1 = 0;

const int total_modes2 = 3;
int mode_index2 = 0;

const int total_modes3 = 34;
int mode_index3 = 0;

// Modes Arrays
const char* master_modes[] = {
    "Voltage", "Current", "Resistance", "Capacitance",
    "Continuity", "IC Testing", "Transistor test", "Inductance"
};

const char* voltage_modes[] = { "5V", "9V", "15V" };

const char* transistor_modes[] = { "NPN / N-MOS", "PNP / P-MOS", "TEST" };

const char* ic_modes[] = {
    // Original 25
    "Find_IC", "741",   "7400",  "7402",  "7404",  "7408",  "7432",  "7486",
    "7401",    "7403",  "7405",  "7406",  "7407",  "7409",  "7410",  "7411",
    "7412",    "7415",  "7420",  "7425",  "7427",  "7451",  "7458",  "74107", "555",
    // New — existing pins only (indices 25-31)
    "7426",  "7428",  "7430",  "7433",  "7437",  "7438",  "74266",
    // New — uses IC pin12(GPIO4) + pin13(GPIO5) (indices 32-33)
    "7421",  "7474"
};

// Pin aliases
const int pins[3] = {2, 25, 26};

// Resistance pins
const int BASE1 = 32;
const int BASE2 = 33;
const int BASE3 = 13;
const int ADC_PIN = 4;

const float R1 = 973.0;
const float R2 = 9760.0;
const float R3 = 91100.0;

const float ADC_REF = 3.3;
const int ADC_RES = 4095;
const float V_IN = 2.7;

// Voltage divider resistors
#define VOLTAGE_PIN 15

const float R01 = 21500.0;
const float R02 = 8840.0;
const float R03 = 39500.0;
const float R04 = 9770.0;

// Inductance tester pins
#define INDUCTOR_DRIVE_PIN  32   // OUTPUT — drives LC circuit via 150Ω resistor
#define INDUCTOR_PULSE_PIN  35   // INPUT  — reads comparator/op-amp output (pulseIn)
#define INDUCTOR_CAPACITANCE 2.0e-6  // Reference cap value in Farads (2µF) — change if different

// IC base pin 12 and 13 — shared with resistance/capacitance, never active simultaneously
#define IC_PIN12  4   // GPIO4  (resistance ADC in other mode)
#define IC_PIN13  5   // GPIO5  (capacitance charge pin in other mode)
const int chargePin = 5;
const int dischargePin = 2;
const int analogPin = 34;
const float Rc = 1500.0;
const int ADC_MAX = 4095;

// Transistor pins
#define L 12
#define M 14
#define R 27

// ─────────────────────────────────────────────
// SHARED DISPLAY HELPERS
// ─────────────────────────────────────────────

// Prepare display for a fresh text frame
void dispBegin() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
}

// Show PASS/FAIL result for an IC or component test
void showResult(const char* label, bool pass) {
    dispBegin();
    if (pass) {
        display.print(label);
        display.println(" WORKING");
        Serial.print(label); Serial.println(" WORKING");
    } else {
        display.print(label);
        display.println(" FAULTY");
        Serial.print(label); Serial.println(" FAULTY");
    }
    display.display();
}

// Show "Executing: <name>" splash before running a mode
void showExecuting(const char* name) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 10);
    display.print("Executing:");
    display.setCursor(10, 20);
    display.print(name);
    display.display();
    Serial.print("Executing Mode: ");
    Serial.println(name);
}

// ─────────────────────────────────────────────
// UNIFIED MENU DISPLAY
// Replaces displayModes0/1/2/3
// ─────────────────────────────────────────────
void displayMenu(const char** modes, int total, int selected) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    int start = selected > 0 ? selected - 1 : 0;
    int end = start + 3;
    if (end > total) end = total;

    for (int i = start; i < end; i++) {
        display.setCursor(10, (i - start) * 10);
        display.print(i == selected ? "> " : "  ");
        display.print(modes[i]);
    }
    display.display();
}

// Convenience wrappers (buttons() and loop() call these by name)
void displayModes0() { displayMenu(master_modes,     total_modes0, mode_index0); }
void displayModes1() { displayMenu(voltage_modes,    total_modes1, mode_index1); }
void displayModes2() { displayMenu(transistor_modes, total_modes2, mode_index2); }
void displayModes3() { displayMenu(ic_modes,         total_modes3, mode_index3); }

// ─────────────────────────────────────────────
// BUTTON HANDLER
// ─────────────────────────────────────────────
void buttons() {
    if (digitalRead(UP_BUTTON) == HIGH) {
        if (mode_index0 > 0) { mode_index0--; displayModes0(); }
        delay(200);
    }
    if (digitalRead(DOWN_BUTTON) == HIGH) {
        if (mode_index0 < total_modes0 - 1) { mode_index0++; displayModes0(); }
        delay(200);
    }
    if (digitalRead(NEXT_BUTTON) == HIGH) {
        executeMode0(mode_index0);
        delay(1000);
        ESP.restart();
    }
}

// ─────────────────────────────────────────────
// UNIFIED EXECUTE MODE
// Replaces executeMode0/1/2/3
// ─────────────────────────────────────────────
void executeMode0(int mode) {
    showExecuting(master_modes[mode]);
    switch (mode) {
        case 0: measureVoltage();    break;
        case 1: measureCurrent();    break;
        case 2: measureResistance(); break;
        case 3: measureCapacitance();break;
        case 4: continuity();        break;
        case 5: testIC();            break;
        case 6: testTransistor();    break;
        case 7: measureInductance(); break;
    }
    delay(2000);
}

void executeMode1(int mode) {
    showExecuting(voltage_modes[mode]);
    float Rtop;
    switch (mode) {
        case 0: Rtop = R04; break;
        case 1: Rtop = R01; break;
        case 2: Rtop = R03; break;
        default: return;
    }
    measureVoltageRange(Rtop, R02);
    delay(2000);
}

void executeMode2(int mode) {
    showExecuting(transistor_modes[mode]);
    switch (mode) {
        case 0: check_npn(); break;
        case 1: check_pnp(); break;
        case 2: TEST();      break;
    }
    delay(2000);
}

void executeMode3(int mode) {
    showExecuting(ic_modes[mode]);
    switch (mode) {
        case 0:  Find_IC();      break;
        case 1:  check_741();    break;
        case 2:  check_7400();   break;
        case 3:  check_7402();   break;
        case 4:  check_7404();   break;
        case 5:  check_7408();   break;
        case 6:  check_7432();   break;
        case 7:  check_7486();   break;
        case 8:  check_7401();   break;
        case 9:  check_7403();   break;
        case 10: check_7405();   break;
        case 11: check_7406();   break;
        case 12: check_7407();   break;
        case 13: check_7409();   break;
        case 14: check_7410();   break;
        case 15: check_7411();   break;
        case 16: check_7412();   break;
        case 17: check_7415();   break;
        case 18: check_7420();   break;
        case 19: check_7425();   break;
        case 20: check_7427();   break;
        case 21: check_7451();   break;
        case 22: check_7458();   break;
        case 23: check_74107();  break;
        case 24: check_555();    break;
        // New ICs — existing pins only
        case 25: check_7426();   break;
        case 26: check_7428();   break;
        case 27: check_7430();   break;
        case 28: check_7433();   break;
        case 29: check_7437();   break;
        case 30: check_7438();   break;
        case 31: check_74266();  break;
        // New ICs — use IC pin12(GPIO4) + pin13(GPIO5)
        case 32: check_7421();   break;
        case 33: check_7474();   break;
    }
    delay(2000);
}

// ─────────────────────────────────────────────
// IC TESTING
// ─────────────────────────────────────────────

// Helper: run 2-input truth table, return pass/fail
bool test2Input(int pinA, int pinB, int pinY,
                bool a0, bool a1, bool a2, bool a3,  // expected outputs for 00,01,10,11
                bool invertedPins = false) {
    if (!invertedPins) {
        pinMode(pinA, OUTPUT); pinMode(pinB, OUTPUT);
    } else {
        pinMode(pinB, OUTPUT); pinMode(pinA, OUTPUT);
    }
    pinMode(pinY, INPUT);

    bool pass = true;
    bool inputs[4][2] = {{LOW,LOW},{LOW,HIGH},{HIGH,LOW},{HIGH,HIGH}};
    bool expected[4] = {a0, a1, a2, a3};

    for (int i = 0; i < 4; i++) {
        digitalWrite(pinA, inputs[i][0]);
        digitalWrite(pinB, inputs[i][1]);
        delay(10);
        if ((bool)digitalRead(pinY) != expected[i]) pass = false;
    }
    return pass;
}

void Find_IC() {
    display.clearDisplay();

    // Each test: { pinA, pinB, pinY, truth table for 00/01/10/11 }
    struct ICTest { int a, b, y; bool t[4]; const char* name; };

    ICTest tests[] = {
        { 2, 25, 26, {HIGH, HIGH, HIGH, LOW},  "7400" },  // NAND
        {26, 25,  2, {HIGH, LOW,  LOW,  LOW},  "7402" },  // NOR
        { 2, 25, 26, {LOW,  LOW,  LOW,  HIGH}, "7408" },  // AND
        { 2, 25, 26, {LOW,  HIGH, HIGH, HIGH}, "7432" },  // OR
        { 2, 25, 26, {LOW,  HIGH, HIGH, LOW},  "7486" },  // XOR
    };

    for (auto& t : tests) {
        bool pass = test2Input(t.a, t.b, t.y, t.t[0], t.t[1], t.t[2], t.t[3]);
        if (pass) {
            dispBegin();
            display.print(t.name); display.println(" FOUND");
            display.display();
            delay(500);
            return;  // Early exit — no need to test further
        }
    }

    // 7404 inverter (single input, different structure)
    pinMode(2, OUTPUT); pinMode(25, INPUT);
    digitalWrite(2, LOW); delay(10);
    bool inv_pass = (digitalRead(25) == HIGH);
    digitalWrite(2, HIGH); delay(10);
    inv_pass &= (digitalRead(25) == LOW);

    dispBegin();
    if (inv_pass) { display.println("7404 FOUND"); }
    else          { display.println("IC Not Working"); }
    display.display();
    delay(500);
}

// LINEAR IC
void check_555() {
    const int TRIG  = 2;
    const int OUT   = 25;
    const int RESET = 26;
    const int CTRL  = 32;
    const int THR   = 33;
    const int DISCH = 13;
    const int GND   = 27;

    pinMode(GND, OUTPUT);   digitalWrite(GND, LOW);
    pinMode(RESET, OUTPUT); digitalWrite(RESET, HIGH);
    pinMode(TRIG, INPUT); pinMode(THR, INPUT);
    pinMode(OUT, INPUT); pinMode(DISCH, INPUT); pinMode(CTRL, INPUT);

    int highCount = 0, lowCount = 0;
    unsigned long start = millis();
    while (millis() - start < 3000) {
        if (digitalRead(OUT) == HIGH) highCount++; else lowCount++;
        delay(10);
    }
    showResult("555", highCount > 10 && lowCount > 10);
    delay(500);
}

void check_741() {
    const int VCC      = 33;
    const int IN_PLUS  = 26;
    const int IN_MINUS = 25;
    const int OUT      = 32;

    pinMode(VCC, OUTPUT);      digitalWrite(VCC, HIGH);
    pinMode(IN_PLUS, OUTPUT);
    pinMode(IN_MINUS, OUTPUT);
    pinMode(OUT, INPUT);

    digitalWrite(IN_PLUS, HIGH); digitalWrite(IN_MINUS, LOW);
    delay(10);
    bool test1 = (digitalRead(OUT) == HIGH);
    showResult("741", test1);
}

// BASIC LOGIC GATES — all use showResult() + test2Input()
void check_7400() { showResult("7400", test2Input(2,25,26, HIGH,HIGH,HIGH,LOW )); }
void check_7401() { showResult("7401", test2Input(26,25,2, HIGH,HIGH,HIGH,LOW )); }
void check_7402() { showResult("7402", test2Input(26,25,2, HIGH,LOW, LOW, LOW )); }
void check_7403() { showResult("7403", test2Input(2,25,26, HIGH,LOW, LOW, LOW )); }

void check_7404() {
    pinMode(2, OUTPUT); pinMode(25, INPUT);
    digitalWrite(2, LOW); delay(10);
    bool out = (digitalRead(25) == HIGH);
    showResult("7404", out);
}
void check_7405() {
    pinMode(2, OUTPUT); pinMode(25, INPUT);
    digitalWrite(2, LOW); delay(10);
    showResult("7405", digitalRead(25) == HIGH);
}
void check_7406() {
    pinMode(2, OUTPUT); pinMode(25, INPUT);
    digitalWrite(2, LOW); delay(10);
    showResult("7406", digitalRead(25) == HIGH);
}
void check_7407() {
    pinMode(2, OUTPUT); pinMode(25, INPUT);
    digitalWrite(2, LOW); delay(10);
    showResult("7407", digitalRead(25) == LOW);
}

void check_7408() { showResult("7408", test2Input(2,25,26, LOW, LOW, LOW, HIGH)); }
void check_7409() { showResult("7409", test2Input(2,25,26, LOW, LOW, LOW, HIGH)); }
void check_7432() { showResult("7432", test2Input(2,25,26, LOW, HIGH,HIGH,HIGH)); }
void check_7486() { showResult("7486", test2Input(2,25,26, LOW, HIGH,HIGH,LOW )); }

// 3-INPUT GATE HELPER
bool test3Input(int pinA, int pinB, int pinC, int pinY,
                bool t000, bool t001, bool t010, bool t011,
                bool t100, bool t101, bool t110, bool t111) {
    pinMode(pinA, OUTPUT); pinMode(pinB, OUTPUT);
    pinMode(pinC, OUTPUT); pinMode(pinY, INPUT);

    bool pass = true;
    bool inputs[8][3] = {
        {0,0,0},{0,0,1},{0,1,0},{0,1,1},
        {1,0,0},{1,0,1},{1,1,0},{1,1,1}
    };
    bool expected[8] = {t000,t001,t010,t011,t100,t101,t110,t111};

    for (int i = 0; i < 8; i++) {
        digitalWrite(pinA, inputs[i][0]);
        digitalWrite(pinB, inputs[i][1]);
        digitalWrite(pinC, inputs[i][2]);
        delay(10);
        if ((bool)digitalRead(pinY) != expected[i]) pass = false;
    }
    return pass;
}

void check_7410() {  // 3-input NAND
    showResult("7410", test3Input(2,25,27,26,
        HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,LOW));
}
void check_7411() {  // 3-input AND
    showResult("7411", test3Input(2,25,27,26,
        LOW,LOW,LOW,LOW,LOW,LOW,LOW,HIGH));
}
void check_7412() {  // 3-input NAND (OC)
    showResult("7412", test3Input(2,25,27,26,
        HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,LOW));
}
void check_7415() {  // 3-input AND (OC)
    showResult("7415", test3Input(2,25,27,26,
        LOW,LOW,LOW,LOW,LOW,LOW,LOW,HIGH));
}
void check_7427() {  // 3-input NOR
    showResult("7427", test3Input(27,2,25,26,
        HIGH,LOW,LOW,LOW,LOW,LOW,LOW,LOW));
}

// 4-INPUT GATE HELPER
bool test4Input(int pinA, int pinB, int pinC, int pinD, int pinY,
                bool t0000, bool t0001, bool t0111, bool t1111) {
    pinMode(pinA, OUTPUT); pinMode(pinB, OUTPUT);
    pinMode(pinC, OUTPUT); pinMode(pinD, OUTPUT);
    pinMode(pinY, INPUT);

    bool pass = true;
    // Spot-check: all-low, one-high, three-high, all-high
    auto check = [&](bool a, bool b, bool c, bool d, bool exp) {
        digitalWrite(pinA, a); digitalWrite(pinB, b);
        digitalWrite(pinC, c); digitalWrite(pinD, d);
        delay(10);
        if ((bool)digitalRead(pinY) != exp) pass = false;
    };
    check(LOW, LOW, LOW, LOW,   t0000);
    check(LOW, LOW, LOW, HIGH,  t0001);
    check(LOW, HIGH,HIGH,HIGH,  t0111);
    check(HIGH,HIGH,HIGH,HIGH,  t1111);
    return pass;
}

void check_7420() {  // 4-input NAND
    showResult("7420", test4Input(12,14,2,25,26, HIGH,HIGH,HIGH,LOW));
}
void check_7425() {  // 4-input NOR
    showResult("7425", test4Input(12,14,2,25,26, HIGH,LOW,LOW,LOW));
}

// AND-OR-INVERT gates (7451 / 7458) — struct-driven truth table
void runAOI(int pinA, int pinB, int pinC, int pinD, int pinY,
            const char* label,
            bool t0000, bool t1100, bool t0011, bool t1111,
            bool t1000, bool t0001) {
    pinMode(pinA, OUTPUT); pinMode(pinB, OUTPUT);
    pinMode(pinC, OUTPUT); pinMode(pinD, OUTPUT);
    pinMode(pinY, INPUT);

    struct TV { bool a,b,c,d,y; };
    TV tests[] = {
        {0,0,0,0,t0000}, {1,1,0,0,t1100},
        {0,0,1,1,t0011}, {1,1,1,1,t1111},
        {1,0,0,0,t1000}, {0,0,0,1,t0001}
    };
    bool pass = true;
    for (auto& t : tests) {
        digitalWrite(pinA, t.a); digitalWrite(pinB, t.b);
        digitalWrite(pinC, t.c); digitalWrite(pinD, t.d);
        delay(10);
        if ((bool)digitalRead(pinY) != t.y) pass = false;
    }
    showResult(label, pass);
}

void check_7451() {
    runAOI(14,27,2,25,26, "7451", 1,0,0,0,1,1);
}
void check_7458() {
    runAOI(14,27,2,25,26, "7458", 0,1,1,1,0,0);
}

void check_74107() {
    const int J   = 12;
    const int K   = 13;
    const int CLK = 32;
    const int RST = 33;
    const int Q   = 25;

    pinMode(J, OUTPUT); pinMode(K, OUTPUT);
    pinMode(CLK, OUTPUT); pinMode(RST, OUTPUT);
    pinMode(Q, INPUT);

    bool pass = true;

    digitalWrite(RST, LOW); delay(10);
    digitalWrite(RST, HIGH); delay(10);

    // Set: J=1 K=0
    digitalWrite(J, HIGH); digitalWrite(K, LOW);
    digitalWrite(CLK, HIGH); delay(5);
    digitalWrite(CLK, LOW); delay(10);
    if (digitalRead(Q) != HIGH) pass = false;

    // Reset: J=0 K=1
    digitalWrite(J, LOW); digitalWrite(K, HIGH);
    digitalWrite(CLK, HIGH); delay(5);
    digitalWrite(CLK, LOW); delay(10);
    if (digitalRead(Q) != LOW) pass = false;

    // Toggle: J=1 K=1 (two clocks must give opposite Q)
    digitalWrite(J, HIGH); digitalWrite(K, HIGH);
    digitalWrite(CLK, HIGH); delay(5);
    digitalWrite(CLK, LOW); delay(10);
    bool q1 = digitalRead(Q);
    digitalWrite(CLK, HIGH); delay(5);
    digitalWrite(CLK, LOW); delay(10);
    bool q2 = digitalRead(Q);
    if (q1 == q2) pass = false;

    showResult("74107", pass);
}

// ─────────────────────────────────────────────
// TRANSISTOR TESTING
// ─────────────────────────────────────────────

// Shared pin-pair test used by check_npn and check_pnp
// Returns true if the transistor conducts as expected
bool transistorPairTest(int collector, int emitter, bool isNPN) {
    pinMode(collector, OUTPUT); digitalWrite(collector, HIGH);
    pinMode(emitter, INPUT);
    pinMode(M, OUTPUT);

    // With base HIGH
    digitalWrite(M, HIGH); delay(10);
    bool withBase = digitalRead(emitter);

    // With base LOW
    digitalWrite(M, LOW); delay(10);
    bool withoutBase = digitalRead(emitter);

    if (isNPN)  return (withBase == true  && withoutBase == false);
    else        return (withBase == false && withoutBase == true);
}

void check_npn() {
    dispBegin();
    display.println("NPN Transistor Test");

    bool detected = false;

    if (transistorPairTest(L, R, true)) {
        display.println("NPN: C=Left E=Right");
        detected = true;
    } else if (transistorPairTest(R, L, true)) {
        display.println("NPN: C=Right E=Left");
        detected = true;
    }

    if (!detected) {
        display.println("No conduction");
        display.println("Not NPN or damaged");
    }
    display.display();
}

void check_pnp() {
    dispBegin();
    display.println("PNP Transistor Test");

    bool detected = false;

    if (transistorPairTest(L, R, false)) {
        display.println("PNP: C=Left E=Right");
        detected = true;
    } else if (transistorPairTest(R, L, false)) {
        display.println("PNP: C=Right E=Left");
        detected = true;
    }

    if (!detected) {
        display.println("No conduction");
        display.println("Not PNP or damaged");
    }
    display.display();
}

// TEST() now just calls check_pnp then check_npn if nothing detected
void TEST() {
    dispBegin();
    display.println("Test");

    bool detected = false;

    // Try PNP first
    if (transistorPairTest(L, R, false)) {
        display.println("PNP: C=Left E=Right"); detected = true;
    } else if (transistorPairTest(R, L, false)) {
        display.println("PNP: C=Right E=Left"); detected = true;
    }

    // Try NPN if PNP not detected
    if (!detected && transistorPairTest(L, R, true)) {
        display.println("NPN: C=Left E=Right"); detected = true;
    } else if (!detected && transistorPairTest(R, L, true)) {
        display.println("NPN: C=Right E=Left"); detected = true;
    }

    if (!detected) {
        display.println("No conduction");
        display.println("Not NPN/PNP or damaged");
    }
    display.display();
}

// ─────────────────────────────────────────────
// MEASUREMENT FUNCTIONS
// ─────────────────────────────────────────────

void measureVoltage() {
    Serial.println("Measuring Voltage...");
    while (true) {
        displayModes1();
        delay(200);
        if (digitalRead(UP_BUTTON) == HIGH) {
            if (mode_index1 > 0) { mode_index1--; delay(200); }
        }
        if (digitalRead(DOWN_BUTTON) == HIGH) {
            if (mode_index1 < total_modes1 - 1) { mode_index1++; delay(200); }
        }
        if (digitalRead(NEXT_BUTTON) == HIGH) {
            executeMode1(mode_index1);
            break;
        }
        delay(50);
    }
}

void measureCurrent() {
    ACS.autoMidPoint();
    while (true) {
        int current_mA = ACS.mA_DC();
        Serial.print("Current: "); Serial.print(current_mA); Serial.println(" mA");

        dispBegin();
        display.print("Current: ");
        display.print(current_mA);
        display.println(" mA");
        display.display();
        delay(200);
    }
}

// Unified voltage range measurement — replaces measureVoltage3/9/15
void measureVoltageRange(float Rtop, float Rbot) {
    while (true) {
        int adcValue = analogRead(VOLTAGE_PIN);
        float vNode = adcValue * (ADC_REF / ADC_RES);
        float vin = vNode * (1.0 + (Rtop / Rbot));

        Serial.print("ADC Value: "); Serial.println(adcValue);
        Serial.print("Vnode: ");     Serial.print(vNode, 3); Serial.println(" V");
        Serial.print("Vin: ");       Serial.print(vin, 3);   Serial.println(" V");

        dispBegin();
        display.print("Vin: ");
        display.print(vin, 2);
        display.println(" V");
        display.display();
        delay(200);
    }
}

// Kept for compatibility — called via executeMode1
void measureVoltage3()  { measureVoltageRange(R04, R02); }
void measureVoltage9()  { measureVoltageRange(R01, R02); }
void measureVoltage15() { measureVoltageRange(R03, R02); }

void measureResistance() {
    while (true) {
        float Vout, R_unknown;
        String label = "Invalid";

        auto activate = [](int pin) {
            pinMode(BASE1, INPUT); pinMode(BASE2, INPUT); pinMode(BASE3, INPUT);
            pinMode(pin, OUTPUT); digitalWrite(pin, HIGH);
        };
        auto readVoltage = []() {
            return analogRead(ADC_PIN) * (ADC_REF / ADC_RES);
        };
        auto calcR = [](float Vout, float R_known) -> float {
            if (Vout <= 0.01 || Vout >= V_IN) return -1.0;
            return (R_known * Vout) / (V_IN - Vout);
        };

        activate(BASE1); delay(100);
        Vout = readVoltage(); R_unknown = calcR(Vout, R1);
        if (R_unknown > 0 && R_unknown < 8000) label = "1k Ref";
        else {
            activate(BASE2); delay(100);
            Vout = readVoltage(); R_unknown = calcR(Vout, R2);
            if (R_unknown >= 8000 && R_unknown < 80000) label = "10k Ref";
            else {
                activate(BASE3); delay(100);
                Vout = readVoltage(); R_unknown = calcR(Vout, R3);
                if (R_unknown >= 80000 && R_unknown < 150000) label = "100k Ref";
                else R_unknown = -1;
            }
        }

        pinMode(BASE1, INPUT); pinMode(BASE2, INPUT); pinMode(BASE3, INPUT);
        Serial.println("------");

        display.clearDisplay();
        if (R_unknown > 0) {
            Serial.print("Ref: "); Serial.println(label);
            Serial.print("Voltage: "); Serial.println(Vout);
            Serial.print("R: "); Serial.print(R_unknown); Serial.println(" ohms");

            display.setCursor(0, 0);  display.println("Resistance Measured:");
            display.setCursor(0, 10); display.print(R_unknown, 0); display.println(" ohms");
            display.setCursor(0, 22); display.print("Using "); display.println(label);
        } else {
            Serial.println("No valid range.");
            display.setCursor(0, 0);  display.println("Invalid Range!");
            display.setCursor(0, 10); display.println("Check connection");
        }
        display.display();
        delay(1000);
    }
}

void measureCapacitance() {
    const int targetADC = (int)(0.63 * ADC_MAX);
    while (true) {
        Serial.println("Measuring Capacitance...");

        pinMode(chargePin, OUTPUT); pinMode(dischargePin, OUTPUT);
        digitalWrite(chargePin, LOW); digitalWrite(dischargePin, LOW);
        delay(500);

        pinMode(dischargePin, INPUT);
        digitalWrite(chargePin, HIGH);

        unsigned long startTime = micros();
        while (analogRead(analogPin) < targetADC) {
            if ((micros() - startTime) > 3000000) break;
        }
        unsigned long elapsedTime = micros() - startTime;

        float capacitance = ((float)elapsedTime / Rc) * 0.7692;

        Serial.print("Elapsed: "); Serial.print(elapsedTime); Serial.println(" us");
        Serial.print("Capacitance: "); Serial.print(capacitance, 3); Serial.println(" uF");

        dispBegin();
        display.println("Capacitance:");
        display.setCursor(0, 10);
        display.print(capacitance, 2);
        display.println(" uF");
        display.display();
        delay(2000);
    }
}

void continuity() {
    analogReadResolution(12);
    const float AREF = 3.3;
    const int   ARES = 4095;

    while (true) {
        int adcValue = analogRead(CONTINUITY_PIN);
        float voltage = adcValue * (AREF / ARES);

        Serial.print("Voltage: "); Serial.print(voltage, 3); Serial.print(" V - ");

        display.clearDisplay();
        display.setCursor(0, 0);
        if (voltage > CONTINUITY_THRESHOLD) {
            Serial.println("Continuity Detected!");
            display.print("Continuity OK");
        } else {
            Serial.println("No Continuity");
            display.print("No Continuity");
        }
        display.display();
        delay(300);
    }
}

// ─────────────────────────────────────────────
// SUB-MENU NAVIGATORS
// ─────────────────────────────────────────────

// Generic sub-menu loop — used by testIC, testTransistor, measureVoltage
void runSubMenu(const char** modes, int total, int& index,
                void (*displayFn)(), void (*executeFn)(int)) {
    while (true) {
        displayFn();
        delay(200);
        if (digitalRead(UP_BUTTON) == HIGH) {
            if (index > 0) { index--; delay(200); }
        }
        if (digitalRead(DOWN_BUTTON) == HIGH) {
            if (index < total - 1) { index++; delay(200); }
        }
        if (digitalRead(NEXT_BUTTON) == HIGH) {
            executeFn(index);
            break;
        }
        delay(50);
    }
}

void testIC() {
    runSubMenu(ic_modes, total_modes3, mode_index3, displayModes3, executeMode3);
}

void testTransistor() {
    runSubMenu(transistor_modes, total_modes2, mode_index2, displayModes2, executeMode2);
}

// ─────────────────────────────────────────────
// NEW IC TESTS — EXISTING PINS ONLY
// ─────────────────────────────────────────────

// 7426 — Quad 2-input NAND (open-collector, high voltage)
// Same pinout and logic as 7400. Test is identical.
void check_7426() { showResult("7426", test2Input(2,25,26, HIGH,HIGH,HIGH,LOW)); }

// 7428 — Quad 2-input NOR buffer
// Same pinout as 7402 (Y,A,B order). NOR logic: same as 7402.
void check_7428() { showResult("7428", test2Input(26,25,2, HIGH,LOW,LOW,LOW)); }

// 7433 — Quad 2-input NOR (open-collector)
// Same pinout as 7402. Same NOR logic.
void check_7433() { showResult("7433", test2Input(26,25,2, HIGH,LOW,LOW,LOW)); }

// 7437 — Quad 2-input NAND buffer
// Same pinout and logic as 7400.
void check_7437() { showResult("7437", test2Input(2,25,26, HIGH,HIGH,HIGH,LOW)); }

// 7438 — Quad 2-input NAND (open-collector)
// Same pinout and logic as 7400.
void check_7438() { showResult("7438", test2Input(2,25,26, HIGH,HIGH,HIGH,LOW)); }

// 74266 — Quad 2-input XNOR (open-collector)
// Same pinout as 7486. XNOR truth table: 00→1, 01→0, 10→0, 11→1
void check_74266() { showResult("74266", test2Input(2,25,26, HIGH,LOW,LOW,HIGH)); }

// 7430 — 8-input NAND gate
// IC pins 1-6 (GPIO 12,14,27,2,25,26) = inputs A-F
// IC pin 9 (GPIO32) = input G,  IC pin 10 (GPIO33) = input H
// IC pin 8 (GPIO35) = output Y
// Y = LOW only when ALL 8 inputs HIGH
void check_7430() {
    const int inPins[] = {12, 14, 27, 2, 25, 26, 32, 33};
    const int outPin = 35;

    for (int i = 0; i < 8; i++) pinMode(inPins[i], OUTPUT);
    pinMode(outPin, INPUT);

    bool pass = true;

    // All LOW → output HIGH
    for (int i = 0; i < 8; i++) digitalWrite(inPins[i], LOW);
    delay(10);
    if (digitalRead(outPin) != HIGH) pass = false;

    // All HIGH → output LOW
    for (int i = 0; i < 8; i++) digitalWrite(inPins[i], HIGH);
    delay(10);
    if (digitalRead(outPin) != LOW) pass = false;

    // One LOW → output HIGH
    digitalWrite(inPins[0], LOW);
    delay(10);
    if (digitalRead(outPin) != HIGH) pass = false;

    showResult("7430", pass);
}

// ─────────────────────────────────────────────
// NEW IC TESTS — USES IC PIN12 (GPIO4) + PIN13 (GPIO5)
// ─────────────────────────────────────────────

// 7421 — Dual 4-input AND gate
// IC pin 1(GPIO12)=A1, 2(GPIO14)=B1, 3(GPIO27)=NC, 4(GPIO2)=C1, 5(GPIO25)=D1, 6(GPIO26)=Y1
// IC pin 8(GPIO35)=Y2, 9(GPIO32)=A2, 10(GPIO33)=B2, 11(GPIO13)=NC, 12(GPIO4)=C2, 13(GPIO5)=D2
void check_7421() {
    const int A1=12, B1=14, C1=2,  D1=25, Y1=26;
    const int Y2=35, A2=32, B2=33, C2=IC_PIN12, D2=IC_PIN13;

    pinMode(A1,OUTPUT); pinMode(B1,OUTPUT); pinMode(C1,OUTPUT); pinMode(D1,OUTPUT);
    pinMode(Y1,INPUT);
    pinMode(A2,OUTPUT); pinMode(B2,OUTPUT); pinMode(C2,OUTPUT); pinMode(D2,OUTPUT);
    pinMode(Y2,INPUT);

    bool pass = true;

    // Gate 1: all LOW → LOW
    digitalWrite(A1,LOW); digitalWrite(B1,LOW); digitalWrite(C1,LOW); digitalWrite(D1,LOW);
    delay(10);
    if (digitalRead(Y1) != LOW)  pass = false;

    // Gate 1: all HIGH → HIGH
    digitalWrite(A1,HIGH); digitalWrite(B1,HIGH); digitalWrite(C1,HIGH); digitalWrite(D1,HIGH);
    delay(10);
    if (digitalRead(Y1) != HIGH) pass = false;

    // Gate 1: one LOW → LOW
    digitalWrite(A1,LOW);
    delay(10);
    if (digitalRead(Y1) != LOW)  pass = false;

    // Gate 2: all HIGH → HIGH
    digitalWrite(A2,HIGH); digitalWrite(B2,HIGH); digitalWrite(C2,HIGH); digitalWrite(D2,HIGH);
    delay(10);
    if (digitalRead(Y2) != HIGH) pass = false;

    // Gate 2: one LOW → LOW
    digitalWrite(A2,LOW);
    delay(10);
    if (digitalRead(Y2) != LOW)  pass = false;

    showResult("7421", pass);
}

// 7474 — Dual D Flip-Flop (positive edge triggered)
// IC pin 1(GPIO12)=1CLR, 2(GPIO14)=1D, 3(GPIO27)=1CLK, 4(GPIO2)=1PR
// IC pin 5(GPIO25)=1Q,   6(GPIO26)=1/Q
// IC pin 8(GPIO35)=2/Q,  9(GPIO32)=2Q, 10(GPIO33)=2PR, 11(GPIO13)=2CLK
// IC pin 12(GPIO4)=2D,   13(GPIO5)=2CLR
void check_7474() {
    const int CLR1=12, D1=14, CLK1=27, PR1=2, Q1=25, NQ1=26;
    const int NQ2=35,  Q2=32, PR2=33,  CLK2=13;
    const int D2=IC_PIN12, CLR2=IC_PIN13;

    pinMode(CLR1,OUTPUT); pinMode(D1,OUTPUT); pinMode(CLK1,OUTPUT); pinMode(PR1,OUTPUT);
    pinMode(Q1,INPUT);    pinMode(NQ1,INPUT);
    pinMode(NQ2,INPUT);   pinMode(Q2,INPUT);
    pinMode(PR2,OUTPUT);  pinMode(CLK2,OUTPUT);
    pinMode(D2,OUTPUT);   pinMode(CLR2,OUTPUT);

    // Release preset and clear (both active low — HIGH = inactive)
    digitalWrite(PR1,HIGH);  digitalWrite(CLR1,HIGH);
    digitalWrite(PR2,HIGH);  digitalWrite(CLR2,HIGH);
    delay(5);

    bool pass = true;

    // ── FF1 tests ──
    // Clock in D=1 on rising edge → Q should go HIGH
    digitalWrite(D1,HIGH);
    digitalWrite(CLK1,LOW);  delay(5);
    digitalWrite(CLK1,HIGH); delay(5);
    if (digitalRead(Q1) != HIGH || digitalRead(NQ1) != LOW) pass = false;

    // Clock in D=0 → Q should go LOW
    digitalWrite(D1,LOW);
    digitalWrite(CLK1,LOW);  delay(5);
    digitalWrite(CLK1,HIGH); delay(5);
    if (digitalRead(Q1) != LOW || digitalRead(NQ1) != HIGH) pass = false;

    // Test CLR1 (active low) — set Q then clear it
    digitalWrite(D1,HIGH);
    digitalWrite(CLK1,LOW);  delay(5);
    digitalWrite(CLK1,HIGH); delay(5);   // Q=1
    digitalWrite(CLR1,LOW);  delay(5);   // Assert clear
    if (digitalRead(Q1) != LOW) pass = false;
    digitalWrite(CLR1,HIGH);

    // ── FF2 tests ──
    // Clock in D=1 → Q2 HIGH
    digitalWrite(D2,HIGH);
    digitalWrite(CLK2,LOW);  delay(5);
    digitalWrite(CLK2,HIGH); delay(5);
    if (digitalRead(Q2) != HIGH || digitalRead(NQ2) != LOW) pass = false;

    // Clock in D=0 → Q2 LOW
    digitalWrite(D2,LOW);
    digitalWrite(CLK2,LOW);  delay(5);
    digitalWrite(CLK2,HIGH); delay(5);
    if (digitalRead(Q2) != LOW || digitalRead(NQ2) != HIGH) pass = false;

    // Test CLR2 (active low)
    digitalWrite(D2,HIGH);
    digitalWrite(CLK2,LOW);  delay(5);
    digitalWrite(CLK2,HIGH); delay(5);   // Q2=1
    digitalWrite(CLR2,LOW);  delay(5);
    if (digitalRead(Q2) != LOW) pass = false;
    digitalWrite(CLR2,HIGH);

    showResult("7474", pass);
}

// ─────────────────────────────────────────────
// INDUCTANCE MEASUREMENT
// GPIO 32 → OUTPUT (drives LC circuit via 150Ω resistor)
// GPIO 35 → INPUT  (reads comparator/op-amp output)
// ─────────────────────────────────────────────
void measureInductance() {
    pinMode(INDUCTOR_DRIVE_PIN, OUTPUT);
    pinMode(INDUCTOR_PULSE_PIN, INPUT);

    while (true) {
        // Charge the inductor then release to let LC circuit resonate
        digitalWrite(INDUCTOR_DRIVE_PIN, HIGH);
        delay(5);                           // Give inductor time to charge
        digitalWrite(INDUCTOR_DRIVE_PIN, LOW);
        delayMicroseconds(100);             // Let resonation settle before measuring

        double pulse = pulseIn(INDUCTOR_PULSE_PIN, HIGH, 5000); // Timeout 5ms

        if (pulse > 0.1) {  // Valid reading — no timeout
            double frequency   = 1.0e6 / (2.0 * pulse);
            double inductance  = 1.0 / (INDUCTOR_CAPACITANCE * frequency * frequency
                                        * 4.0 * 3.14159 * 3.14159);
            double ind_uH      = inductance * 1.0e6;
            double ind_mH      = ind_uH * 0.001;

            Serial.print("Pulse uS: ");    Serial.print(pulse);
            Serial.print("\tFreq Hz: ");   Serial.print(frequency);
            Serial.print("\tInduct uH: "); Serial.println(ind_uH);

            dispBegin();
            display.println("Inductance:");
            display.setCursor(0, 12);
            display.print(ind_uH, 2);
            display.print(" uH  ");
            display.print(ind_mH, 4);
            display.println(" mH");
            display.display();
        } else {
            // Timeout — no valid resonance pulse detected
            Serial.println("No pulse — check LC circuit");
            dispBegin();
            display.println("No pulse.");
            display.setCursor(0, 12);
            display.println("Check LC circuit");
            display.display();
        }

        delay(10);
    }
}

// ─────────────────────────────────────────────
// SETUP & LOOP
// ─────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;);
    }

    pinMode(UP_BUTTON, INPUT);
    pinMode(DOWN_BUTTON, INPUT);
    pinMode(NEXT_BUTTON, INPUT);

    displayModes0();
}

void loop() {
    buttons();
}
