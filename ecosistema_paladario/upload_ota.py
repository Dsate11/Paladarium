#!/usr/bin/env python3
import sys
import requests
from pathlib import Path

def upload_firmware(ip, firmware_path):
    url = f"http://{ip}/update"
    
    print(f"Subiendo firmware a {url}...")
    print(f"Archivo: {firmware_path}")
    
    try:
        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()
            
        headers = {'Content-Type': 'application/octet-stream'}
        response = requests.post(url, data=firmware_data, headers=headers, timeout=60)
        
        if response.status_code == 200:
            print("✓ Firmware subido exitosamente!")
            print("El ESP32 se reiniciará en unos segundos...")
            return 0
        else:
            print(f"✗ Error: {response.status_code}")
            print(response.text)
            return 1
            
    except FileNotFoundError:
        print(f"✗ Error: No se encontró el archivo {firmware_path}")
        return 1
    except requests.exceptions.RequestException as e:
        print(f"✗ Error de conexión: {e}")
        return 1
    except Exception as e:
        print(f"✗ Error: {e}")
        return 1

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Uso: python upload_ota.py <IP> <ruta_firmware.bin>")
        sys.exit(1)
    
    ip = sys.argv[1]
    firmware_path = sys.argv[2]
    
    sys.exit(upload_firmware(ip, firmware_path))
