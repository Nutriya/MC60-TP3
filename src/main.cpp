#include <Arduino.h>

// Paramètres du filtre à moyenne glissante
const int M = 10;        // Nombre d'échantillons pour la moyenne
int16_t buffer[M] = {0}; // Buffer circulaire pour les échantillons
int bufferIndex = 0;     // Index pour le buffer
int32_t sum = 0;         // Somme des échantillons

// Broche du capteur et broche de sortie
const int sensorPin = 34;
const int outputPin = 25; // Choisissez une broche PWM appropriée pour votre carte

// Déclaration de la fonction de filtrage
int16_t filtre(int16_t x);

// Déclaration de la file d'attente
QueueHandle_t queue;

// Fonction d'interruption du timer pour la lecture analogique
void IRAM_ATTR onTimer()
{
  // Lecture de l'échantillon courant
  int16_t x = analogRead(sensorPin);

  // Application du filtre
  int16_t y = filtre(x);

  // Envoi de la valeur filtrée à la file d'attente
  xQueueSendFromISR(queue, &y, NULL);
}

// Tâche pour l'écriture PWM
void pwmWriteTask(void *parameter)
{
  int16_t y;
  while (true)
  {
    // Réception de la valeur filtrée depuis la file d'attente
    if (xQueueReceive(queue, &y, portMAX_DELAY) == pdPASS)
    {
      // Affichage de l'échantillon filtré
      Serial.println(y);

      // Mise à l'échelle et limitation de la valeur de sortie
      int outputValue = map(y, 0, 4095, 0, 255);    // Conversion de la plage de 0-4095 à 0-255 pour PWM
      outputValue = constrain(outputValue, 0, 255); // Limitation de la valeur entre 0 et 255

      // Génération du signal traité sur la broche de sortie
      analogWrite(outputPin, outputValue);
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

  // Création de la file d'attente
  queue = xQueueCreate(10, sizeof(int16_t));

  // Configuration du timer pour générer une interruption toutes les 1 ms
  // Par defaut les interruption  son géres par le processeur 1
  hw_timer_t *timer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 µs par tick)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true); // 1000 µs = 1 ms
  timerAlarmEnable(timer);

  // Création de la tâche sur le cœur 0 pour l'écriture PWM
  xTaskCreatePinnedToCore(
      pwmWriteTask,     // Fonction de la tâche
      "PwmWriteTask",   // Nom de la tâche
      2048,             // Taille de la pile
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
  // Mise à jour de la somme et du buffer circulaire
  sum -= buffer[bufferIndex];
  buffer[bufferIndex] = x;
  sum += x;

  // Mise à jour de l'index
  bufferIndex = (bufferIndex + 1) % M;

  // Calcul de la moyenne en utilisant un décalage de bits
  return sum >> 3; // Diviser par 8 (2^3) en utilisant un décalage de bits
}