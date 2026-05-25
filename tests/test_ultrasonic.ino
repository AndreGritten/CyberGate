// =============================================
// TESTE FINAL — Validação do ESP32 + HC-SR04
// =============================================
// FIAÇÃO:
//   TRIG → GPIO 26
//   ECHO → GPIO 27 (SEM divisor)
//   VCC  → 3.3V  ← TENTE AQUI AGORA
//   GND  → GND (mesmo trilho do ESP32!)
// =============================================

#define TRIG_PIN  26
#define ECHO_PIN  27
#define LED_PIN   2   // LED embutido do ESP32

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    pinMode(ECHO_PIN, INPUT);

    Serial.println("==========================================");
    Serial.println("  TESTE FINAL HC-SR04");
    Serial.println("==========================================");

    // Pisca o LED pra confirmar que o ESP32 está vivo
    Serial.println("[0] Piscando LED embutido (GPIO 2)...");
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }
    Serial.println("    LED piscou? Se sim, ESP32 OK ✅");

    // Teste: ler o ECHO manualmente após dar um pulso no TRIG
    Serial.println();
    Serial.println("[1] Teste manual do ECHO...");
    Serial.print("    ECHO antes do pulso: ");
    Serial.println(digitalRead(ECHO_PIN));

    // Pulso
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Espera até 20ms lendo o ECHO em loop rápido
    unsigned long inicio = micros();
    bool subiu = false;
    while (micros() - inicio < 20000) {
        if (digitalRead(ECHO_PIN) == HIGH) {
            subiu = true;
            break;
        }
    }
    Serial.print("    ECHO subiu após pulso? ");
    Serial.println(subiu ? "SIM ✅ → Sensor funciona!" : "NÃO ❌ → Sensor com defeito ou sem energia");

    if (!subiu) {
        Serial.println();
        Serial.println("==========================================");
        Serial.println("  CONCLUSÃO:");
        Serial.println("  O sensor NÃO respondeu mesmo com");
        Serial.println("  pinos limpos e leitura manual.");
        Serial.println();
        Serial.println("  Possíveis causas:");
        Serial.println("  1. Sensor com defeito");
        Serial.println("  2. VCC sem energia (teste 3.3V e 5V)");
        Serial.println("  3. GND não compartilhado com o ESP32");
        Serial.println("  4. Jumper solto na protoboard");
        Serial.println("==========================================");
    }

    Serial.println();
    Serial.println("Leituras contínuas:");
}

void loop() {
    // Pisca LED a cada leitura pra confirmar que está rodando
    digitalWrite(LED_PIN, HIGH);

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long dur = pulseIn(ECHO_PIN, HIGH, 50000);

    if (dur == 0) {
        Serial.println("❌ Sem resposta");
    } else {
        float dist = dur * 0.034 / 2.0;
        Serial.print("✅ ");
        Serial.print(dist, 1);
        Serial.println(" cm");
    }

    digitalWrite(LED_PIN, LOW);
    delay(600);
}
