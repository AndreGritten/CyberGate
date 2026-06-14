#include <ESP32Servo.h>

Servo servo;

void setup() {
  servo.attach(13); // fio laranja/amarelo do servo no GPIO 13
}

void loop() {
  servo.write(0);
  delay(1000);

  servo.write(90);
  delay(1000);

  servo.write(180);
  delay(1000);

  servo.write(90);
  delay(1000);
}