from machine import ADC
import time

#ADC connected to GP26
adc = ADC(26)

#calibration settings
CALIBRATION_TIME = 5
SAMPLES = 50

#Detection thresholds above the calibrated baseline
WEAK_MARGIN = 500
DETECT_MARGIN = 1200

def read_adc_average():
    readings = []
    for _ in range(SAMPLES):
        readings.append(adc.read_u16())
        time.sleep_ms(2)

    avg = sum(readings) // len(readings)
    spread = max(readings) - min(readings)
    return avg, spread

#automatic calibration
#assumes no rock/ultrasonic signal is present
print("Calibrating... keep rock OFF/away")

values = []
start = time.time()

while time.time() - start < CALIBRATION_TIME:
    avg, spread = read_adc_average()
    values.append(avg)
    print("Calibrating:", avg)

baseline = sum(values) // len(values)

weak_threshold = baseline + WEAK_MARGIN
detect_threshold = baseline + DETECT_MARGIN

print("Calibration complete")
print("Baseline:", baseline)
print("Weak threshold:", weak_threshold)
print("Detect threshold:", detect_threshold)
print("----------------------")

#detection loop
#continuously detect and classify ultraasonic signal strength
while True:
    avg, spread = read_adc_average()
    voltage = avg * 3.3 / 65535

    print("ADC avg:", avg)
    print("Voltage:", round(voltage, 3), "V")
    print("Spread:", spread)

    if avg >= detect_threshold:
        print("40kHz SIGNAL DETECTED")
    elif avg >= weak_threshold:
        print("Weak / uncertain signal")
    else:
        print("No 40kHz signal")

    print("----------------------")
    time.sleep(0.5)
