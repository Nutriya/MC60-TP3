#include <Arduino.h>

// Paramètres du filtre à moyenne glissante
const int M = 4; // Nombre d'échantillons pour la moyenne
int16_t y_prev = 0; // Valeur filtrée précédente
int16_t sum = 0; // Somme des échantillons

// Broche du capteur et broche de sortie
const int sensorPin = 34; // Broche analogique pour la lecture du signal
const int outputPin = 25; // Broche PWM pour la sortie du signal filtré

// Déclaration de la fonction de filtrage
int16_t filtre(int16_t x);

// Déclaration de la file d'attente
QueueHandle_t queue;

uint32_t cycleCount;

// Tableau pour stocker les échantillons
int16_t samples[M];
int sampleIndex = 0;

// Fonction d'interruption du timer pour la lecture analogique
void IRAM_ATTR onTimer()
{
  // Lecture de l'échantillon courant
  int16_t x = analogRead(sensorPin);

  int16_t y = filtre(x);

  // Envoi de l'échantillon à la file d'attente
  xQueueSendFromISR(queue, &y, NULL);
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
      // Affichage de l'échantillon filtré
      Serial.print(x);
      Serial.print("\tto\t");

      // Mise à l'échelle et limitation de la valeur de sortie
      int outputValue = map(x, 0, 4095, 0, 255);    // Conversion de la plage de 0-4095 à 0-255 pour PWM
      outputValue = constrain(outputValue, 0, 255); // Limitation de la valeur entre 0 et 255

      // Génération du signal traité sur la broche de sortie
      ledcWrite(0, outputValue); // Utilisation de ledcWrite pour la sortie PWM sur ESP32

      Serial.println(outputValue);

      // Affichage du nombre de cycles CPU
      Serial.print("Cycles CPU: ");
      Serial.println(cycleCount);
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
  timerAlarmWrite(timer, 400, true); // 1000 µs = 1 ms
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
int16_t filtre(int16_t x)
{
      // Mesure du nombre de cycles CPU avant le filtrage
      uint32_t startCycleCount = ESP.getCycleCount();
      
      // Ajout de l'échantillon au tableau
      sum -= samples[sampleIndex]; // Soustraire l'ancien échantillon de la somme
      samples[sampleIndex] = x; // Mettre à jour l'échantillon
      sum += x; // Ajouter le nouvel échantillon à la somme
      sampleIndex = (sampleIndex + 1) % M;


      // Application du filtre à moyenne glissante
      int16_t y = sum >> 2  ;

      // Mesure du nombre de cycles CPU après le filtrage
      uint32_t endCycleCount = ESP.getCycleCount();
      cycleCount = endCycleCount - startCycleCount;

  return y;
}