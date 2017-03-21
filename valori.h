// Impostiamo i pin di interrupt richiesti dall'encoder. Per una Arduino Uno,
// e associato microcontrollore ATMega328P-PU, la scelta è obbligata poiché le
// linee dedicate agli interrupt esterni sono 2: uscite digitali 2 e 3 nel caso
// di una Arduino Uno corrispondenti ai pin 4 e 5 del microcontrollore.
#define INTERRUPT1 2
#define INTERRUPT2 3

// Impostiamo le condizioni predefinite all'atto dell'accensione le quali
// vedono un bpm=100 su un usuale 4/4 e tempo marcato con una semiminima
// (nota da 1/4).
#define BPM_DEFAULT     100
#define BATTUTE_DEFAULT 4

// Utilizziamo due led per la marcatura visiva del tempo in quarti. Il led
// rosso lo utilizziamo per la marcatura dell'1, il verde per marcare tutte
// le altre battute. Utilizziamo i pin 11 e 13 del microcontrollore
// ATMega328P-PU, equivalente, per coloro che dovessero utilizzare una
// Arduino Uno, alle uscite digitali 5 e 7.
#define LED_ROSSO 5
#define LED_VERDE 7

// Indichiamo, in millesimi di secondo, il tempo massimo di accensione
// (lampeggio) dei led e la durata massima della nota emessa dal buzzer.
#define LED_IN_ON 40
#define BEEP_TIME 40
