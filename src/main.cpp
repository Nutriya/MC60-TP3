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

void IRAM_ATTR onTimer()
{
  // Lecture de l'échantillon courant
  int16_t x = analogRead(sensorPin);

  // Application du filtre
  int16_t y = filtre(x);

  // Affichage de l'échantillon filtré
  Serial.println(y);

  // Mise à l'échelle et limitation de la valeur de sortie
  int outputValue = map(y, 0, 4095, 0, 255);    // Conversion de la plage de 0-4095 à 0-255 pour PWM
  outputValue = constrain(outputValue, 0, 255); // Limitation de la valeur entre 0 et 255

  // Génération du signal traité sur la broche de sortie
  analogWrite(outputPin, outputValue);
}

void setup()
{
  // Initialisation de la communication série
  Serial.begin(115200);

  // Configuration de la broche du capteur et de la broche de sortie
  pinMode(sensorPin, INPUT);
  pinMode(outputPin, OUTPUT);

  // Configuration du timer pour générer une interruption toutes les 1 ms
  timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 µs par tick)
  timerAttachInterrupt(timerBegin(0, 80, true), &onTimer, true);
  timerAlarmWrite(timerBegin(0, 80, true), 1000, true); // 1000 µs = 1 ms
  timerAlarmEnable(timerBegin(0, 80, true));
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