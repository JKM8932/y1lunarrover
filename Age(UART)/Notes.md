# Technical Overview: The Age Detection Pipeline

The Age Detection system operates via a specialized four-stage hardware and software processing pipeline. Its primary objective is to capture, condition, isolate, and decode an incoming 89kHz amplitude-modulated carrier signal containing asynchronous UART data transmission.

Below is a detailed breakdown of how each stage processes the signal from raw electromagnetic wave to interpreted binary message.

    [ Stage 1: Radio Receiver ] ──> [ Stage 2: Amplification ] ──> [ Stage 3: Demodulation ] ──> [ Stage 4: Binary Encoding ]

## 1. Radio Receiver (Signal Capture & Resonance)
The frontend of the system relies on an inductive magnetic loop antenna—specifically, a large coil of wire acting as a high-value inductor ($L$). To isolate the target signal from background electromagnetic interference (EMI), this inductor is paired with a tuning capacitor ($C$) and an internal resistor ($R$) to form a series RLC resonant circuit.The components are calculated and tuned precisely to a resonant frequency ($f_0$) of 89kHz, using the fundamental formula:

<p align="center">
<img width="290" height="160" alt="image" src="https://github.com/user-attachments/assets/ac435a77-baaf-4a7d-9487-f5fccdade911" />
</p>

When the 89kHz carrier signal passes through the coil, the circuit is at resonance, causing the inductive and capacitive reactances to cancel each other out. This minimizes the circuit's overall impedance, forcing the overall voltage and current to be at a maximum. As a result, every time an active UART transmission packet passes through the field, the receiver registers a distinct, detectable maximum peak voltage amplitude.

## 2. Amplification (Signal Conditioning)

The raw AC voltage induced across the RLC tank circuit during the receiver stage is highly faint and susceptible to attenuation. The amplification stage serves as a hardware conditioning layer to scale this weak analog signal up to a more noticeable and distinct voltage range.

By using an Op-Amp configuration, we can enlargen the signal's peak-to-peak amplitude while preserving its envelope structure. This boosting process ensures the signal has sufficient headroom and clarity, making it robust enough to be processed cleanly by the downstream demodulation hardware noise thresholds.


## 3. Demodulation (Envelope Extraction)

Once the signal is amplified, it still consists of the underlying data riding on top of the high-frequency 89kHz carrier wave. The purpose of the demodulation stage is to strip away this carrier frequency and extract the low-frequency signal (the original UART communication signal).

This will be achieved with an envelope detector. The circuit filters out the rapid 89kHz oscillations, effectively smoothing the signal down to its outer envelope. What remains is a clean, low-frequency baseband UART transmission pulses.

## 4. Binary Encoding (Digitization & Parsing)

The final stage is the conversion of the analog baseband signal into digital 0s and 1s. The demodulated envelope signal is passed into the Analog-To-Digital-Convertor (ADC) to interpret the voltage levels into rigid binary states.

The system maps the high-amplitude peaks (the presence of the carrier) and low-amplitude valleys (the absence of the carrier) into logical 1s and 0s that match standard asynchronous UART framing protocols (Start bit, Data bits, Parity, and Stop bit). Once digitized, the embedded firmware parses the binary stream, interprets the protocol payload, and extracts the final message data.
