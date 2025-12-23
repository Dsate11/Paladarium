import time
import sys
try:
    import serial
except Exception as e:
    print('pyserial no está instalado. Instálalo con: pip install pyserial')
    raise

PORT = 'COM3'
BAUD = 115200
LINE = 'wMiFibra,rZ75dCAn\n'  # <<< Cambia aquí si necesitas otra cadena

try:
    with serial.Serial(PORT, BAUD, timeout=1) as s:
        s.write(LINE.encode('utf-8'))
        time.sleep(0.6)
    print('[LOCAL] Línea enviada al puerto', PORT)
except Exception as e:
    print('[ERROR] No se pudo enviar:', e)
    sys.exit(1)
