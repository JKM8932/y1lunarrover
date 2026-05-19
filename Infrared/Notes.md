We observed that the infrared pulses came from the following bulbs on the back of the PCB in the rock:
<img width="590" height="343" alt="Screenshot 2026-05-19 at 3 54 23 PM" src="https://github.com/user-attachments/assets/39e82363-7e04-4e0f-96c0-7caaec7ba147" />
<img width="590" height="470" alt="oscilloscopeir" src="https://github.com/user-attachments/assets/732a075e-7b44-446e-a0c1-90e6323498cc" />





  The infrared pulses that these bulbs emit travel through a tiny hole made on the outer surface of the rock, which is where we are supposed to place the infrared detector above of:  
<img width="590" height="510" alt="Screenshot 2026-05-19 at 3 54 14 PM" src="https://github.com/user-attachments/assets/cbc75752-bc65-4c3e-abc3-1c1721b61c1a" />

  The magnitude of the pulses detected at the hole were really low, around 100mV~. Highly likely that a Schmitt Trigger or an amplifier is used to largen the pulse amplitude. Also attempted at making an algorithm to identify the infrared wavelength (312 or 547), though it was highly inaccurate. The premise is that we would calculate the average number of pulses in a time window to determine roughly which of 312 or 547 the pulses lean towards.
