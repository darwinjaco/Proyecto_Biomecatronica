#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "credentials.h"  // No incluido en el repo - ver credentials.example.h

// ========== MQTT ==========
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;
const char* TOPIC_ECG   = "pacemaker/ecg";
const char* TOPIC_STATE = "pacemaker/state";
const char* TOPIC_CMD   = "pacemaker/cmd";

// ========== CONFIGURACION MUESTREO ==========
const int SAMPLE_RATE        = 250;
const int SAMPLE_INTERVAL_US = 4000;
const int BUFFER_SIZE        = 500;
const int BATCH_SIZE         = 50;

// ========== HARDWARE ==========
const int LED_PIN   = 2;
const int PULSE_PIN = 25;

// ========== ESTRUCTURA DATOS ==========
struct ECGSample {
    uint32_t timestamp;
    uint16_t value;
};

// ========== VARIABLES GLOBALES ==========
WiFiClient    espClient;
PubSubClient  client(espClient);

ECGSample       ecgBuffer[BUFFER_SIZE];
volatile int    bufferHead  = 0;
volatile int    bufferTail  = 0;
volatile int    bufferCount = 0;

int          bpm              = 60;
bool         pacemaker_active = true;
uint32_t     sampleCount      = 0;

TaskHandle_t samplingTaskHandle = NULL;
TaskHandle_t mqttTaskHandle     = NULL;

unsigned long lastDebugPrint = 0;

// ========== FUNCIONES ECG ==========

static inline float gaussf(float x, float mu, float sigma) {
    float z = (x - mu) / sigma;
    return expf(-0.5f * z * z);
}

int generateWaveP(float t, float cycle_duration_ms) {
    float t_ms    = t * cycle_duration_ms;
    float p_mu    = 0.18f * cycle_duration_ms;
    float p_sigma = fmaxf(16.0f, 0.022f * cycle_duration_ms);
    return (int)(70.0f * gaussf(t_ms, p_mu, p_sigma));
}

int generateWaveQRS(float t, float cycle_duration_ms) {
    float t_ms    = t * cycle_duration_ms;
    float q_mu    = 0.340f * cycle_duration_ms;
    float r_mu    = 0.360f * cycle_duration_ms;
    float s_mu    = 0.388f * cycle_duration_ms;
    float q_sigma = fmaxf(5.5f,  0.010f * cycle_duration_ms);
    float r_sigma = fmaxf(4.5f,  0.008f * cycle_duration_ms);
    float s_sigma = fmaxf(7.5f,  0.012f * cycle_duration_ms);
    float st_mu   = 0.44f  * cycle_duration_ms;
    float st_sigma = fmaxf(22.0f, 0.030f * cycle_duration_ms);

    return (int)(
        -90.0f  * gaussf(t_ms, q_mu,  q_sigma)  +
         880.0f * gaussf(t_ms, r_mu,  r_sigma)  +
        -420.0f * gaussf(t_ms, s_mu,  s_sigma)  +
         25.0f  * gaussf(t_ms, st_mu, st_sigma)
    );
}

int generateWaveT(float t, float cycle_duration_ms) {
    float t_ms    = t * cycle_duration_ms;
    float t_mu    = 0.62f * cycle_duration_ms;
    float t_sigma = fmaxf(32.0f, 0.060f * cycle_duration_ms);
    float left    = 180.0f * gaussf(t_ms, t_mu - 10.0f, t_sigma * 0.85f);
    float right   = 180.0f * gaussf(t_ms, t_mu + 10.0f, t_sigma * 1.15f);
    return (int)(0.55f * left + 0.45f * right);
}

int generateBaselineWander(uint32_t wander_phase) {
    return (int)(100 * sinf(2.0f * M_PI * wander_phase / 50000.0f));
}

int generateMuscularNoise() {
    return random(-40, 40);
}

int generateRealisticECG(uint32_t ms_in_cycle, float hrv_factor = 1.0) {
    int   baseline       = 2050;
    float cycle_duration = (60000.0f / bpm) * hrv_factor;
    float t              = fmodf((float)ms_in_cycle, cycle_duration) / cycle_duration;

    static uint32_t wander_phase = 0;
    int ecg = baseline
        + generateWaveP(t, cycle_duration)
        + generateWaveQRS(t, cycle_duration)
        + generateWaveT(t, cycle_duration)
        + generateBaselineWander(wander_phase++)
        + generateMuscularNoise();

    return constrain(ecg, 0, 4095);
}

// ========== BUFFER ==========
void addToBuffer(uint32_t timestamp, uint16_t value) {
    ecgBuffer[bufferHead] = {timestamp, value};
    bufferHead = (bufferHead + 1) % BUFFER_SIZE;
    if (bufferCount < BUFFER_SIZE) bufferCount++;
    else bufferTail = (bufferTail + 1) % BUFFER_SIZE;
}

int readFromBuffer(ECGSample* batch, int count) {
    int read = 0;
    while (read < count && bufferCount > 0) {
        batch[read++] = ecgBuffer[bufferTail];
        bufferTail = (bufferTail + 1) % BUFFER_SIZE;
        bufferCount--;
    }
    return read;
}

// ========== CALLBACK MQTT ==========
void callback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    if (String(topic) == TOPIC_CMD) {
        if (msg == "ON") {
            pacemaker_active = true;
            client.publish(TOPIC_STATE, "ACTIVO");
        } else if (msg == "OFF") {
            pacemaker_active = false;
            client.publish(TOPIC_STATE, "INACTIVO");
        } else if (msg.startsWith("BPM:")) {
            bpm = msg.substring(4).toInt();
        }
        Serial.printf("[CMD] %s\n", msg.c_str());
    }
}

// ==========
