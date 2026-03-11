# Proyecto_Biomecatronica
Proyecto académico de biomecatrónica que desarrolla un prototipo educativo de marcapasos con IoT (no implantable). Incluye generación de señales ECG simuladas, uso de ESP32, comunicación MQTT, dashboards en la nube y análisis de riesgos y ciberseguridad en dispositivos médicos conectados

# Marcapasos IoT Educativo — Biomecatronica

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Protocol](https://img.shields.io/badge/protocol-MQTT-orange)
![Framework](https://img.shields.io/badge/framework-PlatformIO-purple)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-Academico-yellow)

Prototipo educativo de marcapasos conectado a IoT desarrollado para la materia de Biomecatronica. Simula senales ECG realistas (ondas P, QRS, T), las transmite via MQTT desde un ESP32 y las visualiza en tiempo real en un dashboard web. No implantable — solo con fines educativos.

---

## Arquitectura del sistema

```
ESP32 (FreeRTOS)
  ├── Task 1: Generacion ECG (250 Hz)  -->  Buffer circular
  └── Task 2: MQTT Publisher           -->  broker.hivemq.com
                                                    |
                                          Dashboard Web (index.html)
                                          Suscripcion MQTT via WebSocket
```

---

## Stack tecnologico

| Capa | Tecnologia |
|---|---|
| Microcontrolador | ESP32 (FreeRTOS, dual-core) |
| Comunicacion | MQTT over WiFi |
| Broker | HiveMQ (publico) |
| Senal ECG | Sintesis matematica PQRST con ruido gaussiano |
| Dashboard | HTML + JavaScript + WebSocket MQTT |
| Framework | PlatformIO + Arduino |

---

## Estructura del repositorio

```
Proyecto_Biomecatronica/
├── firmware/
│   ├── src/
│   │   ├── main.cpp                 # Logica principal ESP32
│   │   └── credentials.example.h   # Plantilla de credenciales WiFi
│   ├── platformio.ini               # Configuracion del proyecto
│   └── .gitignore
├── dashboard/
│   └── index.html                   # Visualizador ECG en tiempo real
└── README.md
```

---

## Como usar

### Requisitos
- ESP32 (cualquier variante)
- [PlatformIO](https://platformio.org/) instalado en VSCode
- Conexion WiFi

### Pasos

1. Clonar el repo
   ```bash
   git clone https://github.com/darwinjaco/Proyecto_Biomecatronica.git
   ```

2. Configurar credenciales WiFi
   ```bash
   cd firmware/src
   cp credentials.example.h credentials.h
   # Editar credentials.h con tu red WiFi
   ```

3. Compilar y flashear
   ```bash
   cd firmware
   pio run --target upload
   ```

4. Abrir el dashboard
   - Abre `dashboard/index.html` en tu navegador
   - El dashboard se conecta automaticamente al broker MQTT

---

## Topicos MQTT

| Topico | Direccion | Descripcion |
|---|---|---|
| `pacemaker/ecg` | ESP32 -> Dashboard | Batch de 50 muestras ECG (250 Hz) |
| `pacemaker/state` | ESP32 -> Dashboard | Estado: `ACTIVO` / `INACTIVO` |
| `pacemaker/cmd` | Dashboard -> ESP32 | Comandos: `ON`, `OFF`, `BPM:XX` |

---

## Generacion de senal ECG

La senal se sintetiza componente por componente usando funciones gaussianas:

- Onda P: despolarizacion auricular
- Complejo QRS: despolarizacion ventricular (Q negativa, R pico, S negativa)
- Onda T: repolarizacion ventricular
- Baseline wander: deriva de linea base senoidal
- Ruido muscular: ruido gaussiano +/- 40 unidades

Frecuencia de muestreo: 250 Hz | Resolucion: 12 bits (0 a 4095)

---

## Aviso

Este proyecto es exclusivamente educativo. No reemplaza ni simula con fidelidad clinica un dispositivo medico real. No debe utilizarse con fines diagnosticos o terapeuticos.

---

## Licencia

MIT - Darwin Jacome
