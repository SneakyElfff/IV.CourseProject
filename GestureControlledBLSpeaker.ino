#include <Wire.h>
#include <ESP32_Servo.h>

#define BEAM_PIN_R 26
#define BEAM_PIN_L 25

//входы датчика расстояния
#define TRIGGER_PIN 32
#define ECHO_PIN 33

enum Command {
  DEFAULT,
  PLAY,
  PAUSE,
  NEXT,
  PREVIOUS,
  VOL
};

//используются для кодирования команд
int S0_value = 0;
int S1_value = 0;
int S2_value = 0;
int S3_value = 0;
int S4_value = 0;

//gеременные для хранения состояний плеера
int status = 0;
int playing = 1;

//объявление функций
void swiped();
float distance();
void distance2GPIO(int dist);
void control(Command cmd, int vol);

Command lastCmd = DEFAULT;

void setup() {
  Serial.begin(9600);

  pinMode(BEAM_PIN_L, INPUT_PULLUP);
  pinMode(BEAM_PIN_R, INPUT_PULLUP);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("Setup finished");
}

int getBeamL() {
  return digitalRead(BEAM_PIN_L) == LOW;
}

int getBeamR() {
  return digitalRead(BEAM_PIN_R) == LOW;
}

//считывание информации датчиками движения: сначала левый, потом правый -> следующая песня; наоборот -> предыдущая
void swiped() {
  if (getBeamL() == 1) {
    for (int counter = 0; counter < 1000000; counter++) {
      if (getBeamR() == 1) {
        Serial.println("******Next Song*****");
        status = 1;
        break;
      }
    }
  } else if (getBeamR() == 1) {
    for (int counter = 0; counter < 1000000; counter++) {
      if (getBeamL() == 1) {
        Serial.println("*****Previous Song******");
        status = 2;
        break;
      }
    }
  } else {
    status = 0;
  }
}

float distance() {
  digitalWrite(TRIGGER_PIN, HIGH);    //отправить импульс датчику
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  unsigned long StartTime = micros();
  unsigned long StopTime = micros();

  while (digitalRead(ECHO_PIN) == 0) {
    StartTime = micros();
  }

  while (digitalRead(ECHO_PIN) == 1) {
    StopTime = micros();
  }

  float TimeElapsed = (float)(StopTime - StartTime);
  float distance = TimeElapsed * 0.0343 / 2.0;

  return distance;
}

void distanceToBits(int dist) {
  switch (dist) {
    case 0:
      S4_value = 0;
      S3_value = 0;
      S2_value = 0;
      break;
    case 1:
      S4_value = 1;
      S3_value = 0;
      S2_value = 0;
      break;
    case 2:
      S4_value = 0;
      S3_value = 1;
      S2_value = 0;
      break;
    case 3:
      S4_value = 1;
      S3_value = 1;
      S2_value = 0;
      break;
    case 4:
      S4_value = 0;
      S3_value = 0;
      S2_value = 1;
      break;
    case 5:
      S4_value = 1;
      S3_value = 0;
      S2_value = 1;
      break;
    case 6:
      S4_value = 0;
      S3_value = 1;
      S2_value = 1;
      break;
    case 7:
      S4_value = 1;
      S3_value = 1;
      S2_value = 1;
      break;
  }
}

void loop() {
  swiped();
  float dist = distance();

    //next song
    if (status == 1) {
    S0_value = 1;
    S2_value = 1;
    S1_value = 0;
    S3_value = 0;
    S4_value = 0;
    delay(1000);
    S2_value = 0;
    S0_value = 0;
    //previous song
  } else if (status == 2) {
    S0_value = 1;
    S3_value = 1;
    S2_value = 0;
    S1_value = 0;
    S4_value = 0;
    delay(1000);
    S3_value = 0;
    S0_value = 0;
    //gesture - ?
  } else if (dist < 20) {
    if (dist < 4 && playing == 1) {
      Serial.println("PAUSE");
      playing = 0;
      S1_value = 1;
      S3_value = 1;
      S2_value = 0;
      S4_value = 0;
      S0_value = 0;
      delay(1500);
      S3_value = 0;
      S1_value = 0;
    } else if (dist < 4 && playing == 0) {
      Serial.println("PLAY");
      playing = 1;
      S1_value = 1;
      S2_value = 1;
      S0_value = 0;
      S3_value = 0;
      S4_value = 0;
      delay(1500);
      S2_value = 0;
      S1_value = 0;
    } else if (dist >= 4 && playing == 1) {
      Serial.println("DETECTING VOLUME");
      S0_value = 1;
      S1_value = 1;

      int distScaled = int(((dist - 4) / (20 - 4)) * 7);
      while (20 >= dist && dist >= 4) {    //если произведен жест для регулирования громкости (от 4 до 20 см)
        float volScaled = ((dist - 4.0) / (20 - 4)) + 0.1;
        if (volScaled > 1.0) {
          volScaled = 1.0;
        }
        if (volScaled < 0.0) {
          volScaled = 0.0;
        }

        distanceToBits(distScaled);
        dist = distance();
        distScaled = int(((dist - 4) / (20 - 4)) * 7);
      }

      delay(1000);
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
      S0_value = 0;
      S1_value = 0;
    }
  } else {
    Serial.println("HERE AT DEFAULT");
      S0_value = 0;
      S1_value = 0;
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
  }
  delay(200);
}

Command bitsToCommand() {
  String res = "0b";
  res += S0_value ? "1" : "0";
  res += S1_value ? "1" : "0";
  res += S2_value ? "1" : "0";
  res += S3_value ? "1" : "0";
  res += S4_value ? "1" : "0";

  if (res[2] == '1' && res[3] == '1') {
    return VOL;
  } else {
    return static_cast<Command>(res.toInt(2));
  }
}

int bitsToVolume(String binary) {
  String res = "0b";
  res += binary[4] == '1' ? "1" : "0";
  res += binary[5] == '1' ? "1" : "0";
  res += binary[6] == '1' ? "1" : "0";

  int scaledVolume = (16 * (res.toInt(2) + 1) - 1) / 2 + 63;
  return scaledVolume;
}

//передача команд на смартфон...
void control(Command cmd, int vol) {
 if (cmd == PLAY) {
  Play(player_iface);
  } else if (cmd == PAUSE) {
    Pause(player_iface);
  } else if (cmd == NEXT) {
    Next(player_iface);
  } else if (cmd == PREVIOUS) {
    Previous(player_iface);
  } else if (cmd == VOL) {
  } else if (cmd == DEFAULT) {
  }

  if (cmd != DEFAULT) {
    Serial.print("Command: ");
    Serial.println(cmd);
    if (cmd == VOL) {
      Serial.print("Volume: ");
      Serial.println(vol);
    }
  }
}

void loop2() {
  delay(500);
  Command cmd = bitsToCommand();

  if (cmd == VOL) {
    int vol = bitsToVolume("0b" + String(S4_value) + "000");
    control(cmd, vol);
  } else {
    if (cmd != lastCmd) {
      control(cmd, 0);
    }
  }

  lastCmd = cmd;
}