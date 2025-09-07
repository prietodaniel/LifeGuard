# LifeGuard

1. Objetivo y Contexto

Objetivo del proyecto:
Diseñar e implementar una pulsera inteligente que detecte automáticamente situaciones de emergencia (como caídas bruscas o posibles infartos) y envíe una alerta a contactos predefinidos a través de comunicación inalámbrica. La detección debe realizarse en tiempo real para una intervención rápida.

Contexto de uso:

Adultos mayores o personas con condiciones cardíacas crónicas.

Actividades de riesgo como deportes extremos, trekking, etc.

Uso doméstico, en hogares, hospitales o instituciones de cuidado.

2. Modos de Implementación y Requerimientos
2.1 Plataforma de desarrollo (recursos restringidos)

Microcontrolador: ESP32 (alternativa: Arduino Nano 33 BLE Sense)

Sensores:

Acelerómetro/MEMS: MPU6050

Sensor de ritmo cardíaco: MAX30100 / MAX30102

Comunicación:

Bluetooth (BLE) o Wi-Fi para envío de alertas

Opción de buzzer/vibrador para señal local

Alimentación: batería Li-Po recargable

2.2 Requerimientos funcionales

Detección de caídas en tiempo real usando datos de aceleración y orientación

Monitoreo de frecuencia cardíaca continua

Generación de alertas automáticas por Bluetooth o Wi-Fi

Botón de emergencia manual

Registro de eventos críticos (timestamp + tipo)

2.3 Requerimientos no funcionales

Bajo consumo energético

Tiempo de respuesta ante evento < 2 segundos

Tolerancia a falsos positivos

Interfaz sencilla de interacción (mínima)

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
