/*
  Streaming data from Bluetooth to internal DAC of ESP32
  
  Copyright (C) 2020 Phil Schatzmann
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "BluetoothA2DPSink.h"

#define BEAM_PIN_R 2
#define BEAM_PIN_L 15

//maximun volume distance in cm
#define MaxVolDist 30

//входы датчика расстояния
#define TRIGGER_PIN 4
#define ECHO_PIN 17

enum Command {
  DFLT = 0,
  PLAY,
  PAUSE,
  VOL,
  NEXT,
  PREVIOUS
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

Command lastCmd = DFLT;

BluetoothA2DPSink a2dp_sink;
TaskHandle_t Task1;

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
}

void setup() {
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = 44100, // corrected by info from bluetooth
      .bits_per_sample = (i2s_bits_per_sample_t) 16, /* the DAC module will only take the 8bits from MSB */
      .channel_format =  I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false
  };

  a2dp_sink.set_i2s_config(i2s_config);  

  Serial.begin(115200);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);

  //name
  a2dp_sink.start("GCBTSpeaker");  

  pinMode(BEAM_PIN_L, INPUT_PULLUP);
  pinMode(BEAM_PIN_R, INPUT_PULLUP);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // xTaskCreatePinnedToCore(loop2, "loop2", 1000, NULL, 1, &Task1, 0);

  Serial.println("Setup finished");

}

int getBeamL() {
  return digitalRead(BEAM_PIN_L) == HIGH;
}

int getBeamR() {
  return digitalRead(BEAM_PIN_R) == HIGH;
}

//считывание информации датчиками движения: сначала левый, потом правый -> следующая песня; наоборот -> предыдущая
void swiped() {
  if ((getBeamL() == 1) && (getBeamR() == 0)){
    for (int counter = 0; counter < 1000000; counter++) {
      if (getBeamR() == 1) {
        Serial.println("******Next Song*****");
        status = 1;
        break;
      }
    }
  } else if ((getBeamR() == 1) && (getBeamL() == 0)){
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
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
      break;
    case 1:
      S2_value = 1;
      S3_value = 0;
      S4_value = 0;
      break;
    case 2:
      S2_value = 0;
      S3_value = 1;
      S4_value = 0;
      break;
    case 3:
      S2_value = 1;
      S3_value = 1;
      S4_value = 0;
      break;
    case 4:
      S2_value = 0;
      S3_value = 0;
      S4_value = 1;
      break;
    case 5:
      S2_value = 1;
      S3_value = 0;
      S4_value = 1;
      break;
    case 6:
      S2_value = 0;
      S3_value = 1;
      S4_value = 1;
      break;
    case 7:
      S2_value = 1;
      S3_value = 1;
      S4_value = 1;
      break;
  }
}

void loop() {
  //Serial.println("loop");
  swiped();
  float dist = distance();

    //next song
    if (status == 1) {
    S0_value = 0; //0b00100 4
    S1_value = 0;
    S2_value = 1;
    S3_value = 0;
    S4_value = 0;
    loop2();
    delay(1000);
    status = 0;
    S2_value = 0;
    //previous song
  } else if (status == 2) {
    S0_value = 1; //0b00101 5
    S1_value = 0;
    S2_value = 1;
    S3_value = 0;
    S4_value = 0;
    loop2();
    delay(1000);
    status = 0;
    S0_value = 0;
    S2_value = 0;
    //gesture - ?
  } else if (dist <= (MaxVolDist + 4)) {
    if (dist < 4 && playing == 1) {
      Serial.println("PAUSE");
      playing = 0;
      S0_value = 0; //0b00010 2
      S1_value = 1;
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
      loop2();
      delay(1500);
      S1_value = 0;
    } else if (dist < 4 && playing == 0) {
      Serial.println("PLAY");
      playing = 1;
      S0_value = 1; //0b00001 1
      S1_value = 0;
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
      loop2();
      delay(1500);
      S0_value = 0;
    } else if (dist >= 4 && playing == 1) {
      Serial.println("DETECTING VOLUME");
      S0_value = 1; //0bxxx11 3
      S1_value = 1;
      int distScaled = int(((dist - 4)  * 7 / (MaxVolDist - 4)));
 //     while ((MaxVolDist + 4) >= dist && dist >= 4) {    //если произведен жест для регулирования громкости (от 4 до 20 см)
        // float volScaled = ((dist - 4.0) / (MaxVolDist - 4)) + 0.1;
        // if (volScaled > 1.0) {
        //   volScaled = 1.0;
        // }
        // if (volScaled < 0.0) {
        //   volScaled = 0.0;
        // }
        distanceToBits(distScaled);
//        dist = distance();
//        distScaled = int(((dist - 4)  * 7 / (MaxVolDist - 4)));
//      }
      loop2();
      delay(200);
      S0_value = 0;
      S1_value = 0;
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
    }
  } else {
    //Serial.println("HERE AT DEFAULT");
      S0_value = 0;
      S1_value = 0;
      S2_value = 0;
      S3_value = 0;
      S4_value = 0;
  }
  delay(200);

}

Command bitsToCommand() {
  String res = "";//"0b";
  res += S4_value ? "1" : "0";
  res += S3_value ? "1" : "0";
  res += S2_value ? "1" : "0";
  res += S1_value ? "1" : "0";
  res += S0_value ? "1" : "0";

  if (res[3] == '1' && res[4] == '1') {
    return VOL;
  } else {
    return static_cast<Command>(strtol(res.c_str(), 0, 2));
  }
}

int bitsToVolume(String binary) {
  String res = ""; //"0b";
  res += binary[2] == '1' ? "1" : "0";
  res += binary[3] == '1' ? "1" : "0";
  res += binary[4] == '1' ? "1" : "0";

  int scaledVolume = strtol(res.c_str(), 0, 2) * 127/7;//(16 * (res.toInt(2) + 1) - 1) / 2 + 63;

  return scaledVolume;
}

//передача команд на смартфон...
void control(Command cmd, int vol) {
 if (cmd == PLAY) {
    a2dp_sink.play();
    Serial.println("PLAYcmd");
  } else if (cmd == PAUSE) {
    a2dp_sink.pause();
    Serial.println("PAUSEcmd");
  } else if (cmd == NEXT) {
    a2dp_sink.next();
    Serial.println("NEXTcmd");
  } else if (cmd == PREVIOUS) {
    //to begin of son
    a2dp_sink.previous();
    delay(10);
    //and again for previous song
    a2dp_sink.previous();
    Serial.println("PREVIOUScmd");
  } else if (cmd == VOL) {
    a2dp_sink.set_volume(vol);
    Serial.print("Volumecmd: ");
    Serial.println(vol);
  } else if (cmd == DFLT) {
  }

}

void loop2() {
// void loop2(void * parameter) {
  // for(;;){
  // delay(500);
  Command cmd = bitsToCommand();

  if (cmd == VOL) {
    int vol = bitsToVolume("0b" + String(S4_value) + String(S3_value)+ String(S2_value));
    control(cmd, vol);
  } else {
    if (cmd != lastCmd) {
      control(cmd, 0);
    }
  }

  //lastCmd = cmd;
  // }
}

