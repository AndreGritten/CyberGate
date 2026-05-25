// =============================================
// TESTE ISOLADO — Leitor RFID RC522
// =============================================
// Fiação:
//   SDA  → GPIO 21
//   SCK  → GPIO 18
//   MOSI → GPIO 23
//   MISO → GPIO 19
//   RST  → GPIO 22
//   GND  → GND
//   3.3V → 3.3V
//   IRQ  → não conectar
// =============================================
// Abra o Serial Monitor (115200 baud) e aproxime um cartão/tag.
// O UID vai aparecer na tela. ANOTE ESSE UID para usar no projeto!

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN   21
#define RST_PIN  22

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
    Serial.begin(115200);
    delay(500);

    SPI.begin();         // Inicia o barramento SPI
    rfid.PCD_Init();     // Inicia o módulo RC522

    Serial.println("========================================");
    Serial.println("  TESTE RC522 — Leitor RFID");
    Serial.println("========================================");

    // Verifica se o leitor responde
    byte versao = rfid.PCD_ReadRegister(rfid.VersionReg);
    if (versao == 0x00 || versao == 0xFF) {
        Serial.println("❌ ERRO: RC522 não detectado!");
        Serial.println("   Verifique a fiação SPI e alimentação 3.3V.");
    } else {
        Serial.print("✅ RC522 detectado! Firmware versão: 0x");
        Serial.println(versao, HEX);
        // 0x91 = v1.0, 0x92 = v2.0 — ambos normais
    }

    Serial.println();
    Serial.println("Aproxime um cartão ou chaveiro RFID...");
    Serial.println();
}

void loop() {
    // Verifica se há um cartão novo presente
    if (!rfid.PICC_IsNewCardPresent()) {
        return;
    }

    // Tenta ler o serial do cartão
    if (!rfid.PICC_ReadCardSerial()) {
        return;
    }

    // === Cartão lido com sucesso! ===
    Serial.println("🎫 CARTÃO DETECTADO!");

    // Mostra o UID
    Serial.print("   UID: ");
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) {
            Serial.print("0");
            uid += "0";
        }
        Serial.print(rfid.uid.uidByte[i], HEX);
        uid += String(rfid.uid.uidByte[i], HEX);
        if (i < rfid.uid.size - 1) Serial.print(":");
    }
    uid.toUpperCase();
    Serial.println();

    // Mostra o tipo do cartão
    MFRC522::PICC_Type tipo = rfid.PICC_GetType(rfid.uid.sak);
    Serial.print("   Tipo: ");
    Serial.println(rfid.PICC_GetTypeName(tipo));

    // Mostra o UID formatado para copiar direto no código
    Serial.print("   ➡️  Para usar no código, copie: \"");
    Serial.print(uid);
    Serial.println("\"");
    Serial.println("----------------------------------------");
    Serial.println();

    // Libera o cartão
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    delay(1000); // Evita leituras repetidas
}
