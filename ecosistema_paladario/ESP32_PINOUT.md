# Pinout ESP32 — Paladarium

## Tabla resumen

| Pin ESP32 | Nombre simbólico (código)    | Función hardware         |
|-----------|------------------------------|--------------------------|
| D33       | CALEFACCION_GPIO             | Relé 4 (Calefacción)     |
| D25       | BOMBA_LLUVIA_GPIO            | Relé 1 (Bomba de lluvia) |
| D26       | BOMBA_CASCADA_GPIO           | Relé 2 (Bomba cascada)   |
| D27       | VENTILADOR_GPIO              | Relé 3 (Ventilador)      |
| D15       | DHT_GPIO                     | Sensor DHT11             |

---

## Diagrama de conexiones (visual)

```
                                    +----------------------+
                                    |     ESP32 DevKit     |
                          ________  +----------------------+
                         |        |  GPIO    | Dispositivo 
            Rele 4 <---- |  D33   | -------- | CALEFACCION  
            Rele 1 <---- |  D25   | -------- | BOMBA LLUVIA 
            Rele 2 <---- |  D26   | -------- | BOMBA CASCADA
            Rele 3 <---- |  D27   | -------- | VENTILADOR   
             DHT11 <---- |  D15   | -------- | SENSOR DHT11 
                         |________|
```

> **Nota:** Utiliza la referencia de pinout físico del modelo de ESP32 devkit que utilices para ubicar físicamente los GPIO referenciados.

---

## Recomendaciones y Advertencias

- Verifica la capacidad de corriente de cada pin antes de realizar cualquier conexión de carga.
- Los pines destinados a relés deben ser empleados con módulos de relé que admitan el voltaje de control TTL (3.3V en ESP32).
- Evita usar los pines listados para otras funciones para prevenir conflictos o comportamientos inesperados.
- Antes de energizar el sistema, comprueba dos veces las conexiones para evitar cortocircuitos o daños a la placa.
- No conectes cargas inductivas o de alto voltaje directamente al ESP32. Utiliza siempre relés o drivers adecuados.
- Consulta la hoja de datos de tu versión de la placa ESP32 y del sensor DHT11 para confirmar compatibilidades y conexiones de alimentación.

---

## Coincidencia con el código (src/main.c)

La definición de pines en `src/main.c` debe mantener la correlación 1 a 1 con la tabla presente en este documento. Cualquier modificación DEBE reflejarse en ambos lados para evitar inconsistencias o fallos de operación.

Ejemplo de definición:

```c
#define CALEFACCION_GPIO      33
#define BOMBA_LLUVIA_GPIO     25
#define BOMBA_CASCADA_GPIO    26
#define VENTILADOR_GPIO       27
#define DHT_GPIO              15
```

---

## Notas adicionales
- Ante cualquier duda sobre cambios de hardware, actualiza este documento junto con el firmware.
- Realiza pruebas funcionales antes de la puesta en marcha definitiva del paladarium.

---

Autor: Dsate11
Última actualización: 2025-12-25
