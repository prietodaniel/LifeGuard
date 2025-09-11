# LifeGuard

1. Objetivo y Contexto

Objetivo del proyecto:
Este proyecto implementa una **pulsera de emergencia SOS** basada en un **microcontrolador RP2350 (Raspberry Pi Pico 2)**.  
Al presionar el botón de pánico, la pulsera obtiene las coordenadas GPS y envía una alerta mediante un **módulo GSM** (SIM800L / A7670E).  
El sistema está pensado como aplicación embebida de **tiempo real** en una plataforma de recursos restringidos. Puede detectar automáticamente situaciones de emergencia (como caídas bruscas o posibles infartos) y envíar una alerta a contactos predefinidos a través de comunicación inalámbrica. En forma paralela podrá emitir una señal de socorro a satélites del sistema COSPAS-SARSAT. Los satélites retransmiten esta señal a centros de emergencia que, a través de la información de la señal codificada y la frecuencia de localización de 121.5 MHz, determinan la ubicación de la personas y coordinan los esfuerzos de búsqueda y rescate. La detección debe realizarse en tiempo real para una intervención rápida.

Contexto de uso:
Actividades de riesgo como deportes extremos, trekking, ski, snowboard, ciclismo de alta montaña, descenso, etc.
Adultos mayores o personas con condiciones cardíacas crónicas.
Uso doméstico, en hogares, hospitales o instituciones de cuidado.

2. Modos de Implementación y Requerimientos
2.1 Plataforma de desarrollo (recursos restringidos)

Microcontrolador: microcontrolador RP2350 Raspberry Pi Pico 2, (alternativa: Arduino Nano 33 BLE Sense)

Sensores:
- **RP2350** (Raspberry Pi Pico 2 o similar)  
- **Módulo GPS** (u-blox NEO-6M / NEO-M8N)  
- **Módulo GSM** (SIM800L o A7670E)  
- **Botón de pánico** (GPIO)  
- **LED/vibrador** para feedback al usuario  
- **Acelerómetro/MEMS**: MPU6050
- **Sensor de ritmo cardíaco**: MAX30100 / MAX30102

Comunicación:

Bluetooth (BLE) o Wi-Fi para envío de alertas

Opción de buzzer/vibrador para señal local

Alimentación: batería Li-Po recargable

2.2 Requerimientos funcionales

- Detección de caídas en tiempo real usando datos de aceleración y orientación
- Monitoreo de frecuencia cardíaca continua
- Generación de alertas automáticas por Bluetooth o Wi-Fi
- Botón de emergencia manual
- Registro de eventos críticos (timestamp + tipo)
- Detectar la pulsación del botón SOS.  
- Obtener coordenadas GPS en tiempo real.  
- Enviar alerta mediante SMS con la ubicación.  
- Confirmar al usuario que la señal fue enviada (LED o vibración).  

2.3 Requerimientos no funcionales

- Bajo consumo energético
- Tiempo de respuesta ante evento < 2 segundos
- Tolerancia a falsos positivos
- Interfaz sencilla de interacción (mínima)
- Bajo consumo de energía.  
- Tiempo de respuesta < 200 ms desde la pulsación.  
- Autonomía mínima de X horas/días.  
- Tamaño reducido y portable.
  
3. Implementación en Plataforma Embebida
Componentes y arquitectura básica:
┌────────────┐
│  Usuario   │
└────┬───────┘
     │ Pulsera
┌────▼─────┐   ┌────────────┐
│ MPU6050  │   │ MAX30102   │
└────┬─────┘   └────┬───────┘
     │              │
 ┌───▼──────────────▼─────────┐
 │        MCU (ESP32)         │
 │  - RTOS (FreeRTOS opcional)│
 │  - Análisis en tiempo real │
 └───┬────────────┬───────────┘
     │            │
     ▼            ▼
 Buzzer/Vibra  Comunicación BLE/WiFi

Software:

Algoritmo de detección de caídas (derivado del vector de aceleración total)

Filtro de señal cardíaca + umbral de alarma

Sistema de tareas (RTOS o implementación manual por timers)

Lógica de envío de alertas (HTTP, MQTT, BLE)

4. Verificación y Validación
Validación funcional:

Simulación de caídas con movimientos bruscos (pruebas de campo)

Generación intencional de frecuencias cardíacas anormales (mediante simulador o movimientos físicos)

Test de botón de pánico manual

Pruebas de comunicación y recepción de alertas

Verificación temporal (en tiempo real):

Medición del tiempo desde evento → alerta

Pruebas de carga del sistema con múltiples eventos

Validación del consumo energético estimado

Métricas de validación:
Parámetro	Valor objetivo	Resultado esperado
Tiempo detección caída	< 1 segundo	✅
Tiempo respuesta alerta	< 2 segundos	✅
Autonomía	> 24 hs	✅/🟡 (a simular)
Falsos positivos	< 10%	🟡 (requiere tuning)
5. Conclusiones

La pulsera SOS demuestra que es posible implementar un sistema de asistencia personal en tiempo real usando hardware de bajo costo.

El procesamiento en tiempo real permite responder ante eventos críticos sin intervención del usuario.

La validación muestra que, aunque hay margen para mejorar la precisión y consumo, el sistema cumple con los objetivos básicos.

Las pruebas muestran la viabilidad de llevar este tipo de tecnología a contextos reales, con potencial de evolución hacia productos comerciales.
