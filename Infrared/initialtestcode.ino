const int sensorPin = A0; 

const float sampleRate = 2000.0;   // Hz 

const int N = 200;                 // 100 ms window 

const float f312 = 312.0; 

const float f547 = 547.0; 

float avg312 = 0; 

float avg547 = 0; 

const float alpha = 0.1;      // lower = smoother, try 0.1 to 0.3 

 

// confidence ratios 

const float ratio312 = 2.0; 

const float ratio547 = 1.2;  // easier for 547 to win 

const float min312 = 2500; 

const float min547 = 1500; 

int stable312 = 0; 

int stable547 = 0; 

float samples[N]; 

float detectPower(float freq) { 

  float real = 0; 

  float imag = 0; 

  for (int i = 0; i < N; i++) { 

    float angle = 2.0 * PI * freq * i / sampleRate; 

    real += samples[i] * cos(angle); 

    imag -= samples[i] * sin(angle); 

  } 

  return real * real + imag * imag; 

} 

void setup() { 

  Serial.begin(115200); 

} 

void loop() { 

  unsigned long sampleInterval = 1000000.0 / sampleRate; 

  // Take samples at fixed rate 

  for (int i = 0; i < N; i++) { 

    unsigned long start = micros(); 

    samples[i] = analogRead(sensorPin); 

    while (micros() - start < sampleInterval) { 

      // wait 

    } 

  } 

  // Remove DC / ambient light 

  float mean = 0; 

  for (int i = 0; i < N; i++) { 

    mean += samples[i]; 

  } 

  mean /= N; 

  for (int i = 0; i < N; i++) { 

    samples[i] -= mean; 

  } 

  float p312 = detectPower(f312); 

  float p547 = detectPower(f547); 

  avg312 = avg312 * (1.0 - alpha) + p312 * alpha; 

  avg547 = avg547 * (1.0 - alpha) + p547 * alpha; 

  Serial.print("avg312: "); 

  Serial.print(avg312); 

  Serial.print(" avg547: "); 

  Serial.println(avg547); 

 

  if (avg312 > avg547 * ratio312 && avg312 > min312) { 

    stable312++; 

    stable547 = 0; 

  } 

  else if (avg547 > avg312 * ratio547 && avg547 > min547) { 

    stable547++; 

    stable312 = 0; 

  } 

  else { 

    stable312 = 0; 

    stable547 = 0; 

  } 

  if (stable312 >= 4) { // becaues previously leaning towards this, making it harder to classify therefore need more certainty 

    Serial.println("CONFIDENT: 312 Hz"); 

    stable312 = 0; 

    stable547 = 0; 

  } 

  if (stable547 >= 3) { 

    Serial.println("CONFIDENT: 547 Hz"); 

    stable312 = 0; 

    stable547 = 0; 

  } 

  delay(100); 

} 
