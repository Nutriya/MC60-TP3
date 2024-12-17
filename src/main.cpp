#include <Arduino.h>

// Paramètres du filtre à moyenne glissante
const int M = 16;           // Nombre d'échantillons pour la moyenne
int16_t y_prev = 0;         // Valeur filtrée précédente
int16_t sum = 0;            // Somme des échantillons

#define SENSOR_PIN GPIO_NUM_25
#define OUTPUT_PIN GPIO_NUM_26

// Déclaration de la fonction de filtrage
int16_t filtre(int16_t x);

// Déclaration de la file d'attente
QueueHandle_t queue;

uint32_t cycleCount;

// Tableau pour stocker les échantillons
int16_t samples[M];
int sampleIndex = 0;

// Fonction d'interruption du timer pour la lecture analogique
void IRAM_ATTR onTimer() {
  // Lecture de l'échantillon courant
  int16_t x = analogRead(SENSOR_PIN) - 2048; // Ajustement pour inclure les valeurs négatives

  int16_t y = filtre(x);

  // Envoi de l'échantillon à la file d'attente
  xQueueSendFromISR(queue, &y, NULL);
}

// Tâche pour l'écriture DAC
void dacWriteTask(void *parameter) {
  int16_t x;
  while (true) {
    // Réception de l'échantillon depuis la file d'attente
    if (xQueueReceive(queue, &x, portMAX_DELAY) == pdPASS) {
      // Mise à l'échelle et limitation de la valeur de sortie
      int outputValue = map(x, -2048, 2047, 0, 255); // Conversion de la plage de -2048 à 2047 à 0-255 pour DAC
      outputValue = constrain(outputValue, 0, 255); // Limitation de la valeur entre 0 et 255

      // Génération du signal traité sur la broche de sortie
      dacWrite(OUTPUT_PIN, outputValue); // Utilisation de dacWrite pour la sortie DAC sur ESP32
    }
  }
}

void setup() {
  // Initialisation de la communication série
  Serial.begin(115200);

  // Configuration de la broche du capteur et de la broche de sortie
  pinMode(SENSOR_PIN, INPUT);

  // Création de la file d'attente
  queue = xQueueCreate(30, sizeof(int16_t));

  // Configuration du timer pour générer une interruption toutes les 1 ms
  hw_timer_t *timer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 µs par tick)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 150, true); // 150 µs 
  timerAlarmEnable(timer);

  // Création de la tâche sur le cœur 0 pour l'écriture DAC
  /**
   * On choise d'attribuer la ta coeur 0 puisque les interruptions sont traité par la coeur 1 par defaut
   */
  xTaskCreatePinnedToCore(
      dacWriteTask,     // Fonction de la tâche
      "DacWriteTask",   // Nom de la tâche
      2048,             // Taille de la pile
      NULL,             // Paramètre de la tâche
      1,                // Priorité de la tâche
      NULL,             // Handle de la tâche
      0);               // Numéro du cœur (0)
}

void loop() {
  // Rien à faire dans la boucle principale
}

// Définition de la fonction de filtrage
int16_t filtre(int16_t x) {
  // Mesure du nombre de cycles CPU avant le filtrage
  uint32_t startCycleCount = ESP.getCycleCount();
      
  // Ajout de l'échantillon au tableau
  sum -= samples[sampleIndex]; // Soustraire l'ancien échantillon de la somme
  samples[sampleIndex] = x; // Mettre à jour l'échantillon
  sum += x; // Ajouter le nouvel échantillon à la somme
  sampleIndex = (sampleIndex + 1) % M;

  // Application du filtre à moyenne glissante
  int16_t y = sum / M;

  // Mesure du nombre de cycles CPU après le filtrage
  uint32_t endCycleCount = ESP.getCycleCount();
  cycleCount = endCycleCount - startCycleCount;

  return y;
}