#include <Arduino.h>

// Paramètres du filtre récursif
const float alpha = 0.1; // Coefficient de lissage (ajustez selon vos besoins)
int16_t y_prev = 0;      // Valeur filtrée précédente

// Broche du capteur et broche de sortie
const int sensorPin = 34; // Broche analogique pour la lecture du signal
const int outputPin = 25; // Broche PWM pour la sortie du signal filtré

// Déclaration de la fonction de filtrage
int16_t filtre(int16_t x, int16_t x_prev[], int M);

// Déclaration de la file d'attente
QueueHandle_t queue;

// Nombre d'échantillons pour le filtre
const int numSamples = 2;
int16_t samples[numSamples];
int sampleIndex = 0;

// Fonction d'interruption du timer pour la lecture analogique
void IRAM_ATTR onTimer()
{
  // Lecture de l'échantillon courant
  int16_t x = analogRead(sensorPin);

  // Envoi de l'échantillon à la file d'attente
  xQueueSendFromISR(queue, &x, NULL);
}

// Tâche pour l'écriture PWM
void pwmWriteTask(void *parameter)
{
  int16_t x;
  while (true)
  {
    // Réception de l'échantillon depuis la file d'attente
    if (xQueueReceive(queue, &x, portMAX_DELAY) == pdPASS)
    {
      // Ajout de l'échantillon au tableau
      samples[sampleIndex] = x;
      sampleIndex = (sampleIndex + 1) % numSamples;

      // Application du filtre avec les 5 dernières valeurs
      int16_t y = filtre(x, samples, numSamples);

      // Affichage de l'échantillon filtré
      Serial.print(y);
      Serial.print("\tto\t");

      // Mise à l'échelle et limitation de la valeur de sortie
      int outputValue = map(y, 0, 4095, 0, 255);    // Conversion de la plage de 0-4095 à 0-255 pour PWM
      outputValue = constrain(outputValue, 0, 255); // Limitation de la valeur entre 0 et 255

      // Génération du signal traité sur la broche de sortie
      ledcWrite(0, outputValue); // Utilisation de ledcWrite pour la sortie PWM sur ESP32

      Serial.println(outputValue);
    }
  }
}

void setup()
{
  // Initialisation de la communication série
  Serial.begin(115200);

  // Configuration de la broche du capteur et de la broche de sortie
  pinMode(sensorPin, INPUT);
  pinMode(outputPin, OUTPUT);

  // Configuration de la sortie PWM sur ESP32
  ledcSetup(0, 5000, 8); // Canal 0, fréquence 5 kHz, résolution 8 bits
  ledcAttachPin(outputPin, 0); // Attacher la broche de sortie au canal 0

  // Création de la file d'attente
  queue = xQueueCreate(10, sizeof(int16_t));

  // Configuration du timer pour générer une interruption toutes les 1 ms
  hw_timer_t *timer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 µs par tick)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true); // 1000 µs = 1 ms
  timerAlarmEnable(timer);

  // Création de la tâche sur le cœur 0 pour l'écriture PWM
  xTaskCreatePinnedToCore(
      pwmWriteTask,     // Fonction de la tâche
      "PwmWriteTask",   // Nom de la tâche
      2048*3,           // Taille de la pile
      NULL,             // Paramètre de la tâche
      1,                // Priorité de la tâche
      NULL,             // Handle de la tâche
      0);               // Numéro du cœur (0)
}

void loop()
{
  // Rien à faire dans la boucle principale
}

// Définition de la fonction de filtrage
int16_t filtre(int16_t x, int16_t x_prev[], int M)
{
  // Application du filtre avec les 5 dernières valeurs
  int16_t y = alpha * x + (1 - alpha) * y_prev;
  for (int i = 1; i < M; i++) {
    y += alpha * x_prev[(sampleIndex - i + M) % M];
  }
  y_prev = y; // Mise à jour de la valeur filtrée précédente
  return y;
}