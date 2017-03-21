// 
#include <Wire.h>
//
#include <DS1307RTC.h>
//
#include <TimeLib.h>

#include <LiquidCrystal_I2C.h>

// Al fine di alleggerire e rendere più leggibile il sorgente principale, spostiamo su file
// dedicati alcune variabili e grandezze che verranno poi utilizzati in questo file. Ciò è
// possibile grazie alla direttiva 'include' la quale permette di implementare file esterni
// allo stesso modo di come si farebbe per una libreria.
#include "frequenze.h"
#include "valori.h"

// Impostiamo il dispaly LCD su bus I2C: indirizzo 0x3F, modello 16 caratteri per 2 righe.
LiquidCrystal_I2C lcd(0x3F,16,2);

// Creiamo un'istanza della classe DS1307RTC per il Real Time Clock (orologio con funzione di
// allarme grazie all'integrato DS1307) che utilizziamo al fine di avere un orologio integrato
// nel nostro metronomo. Non è indispensabile, ma ci permette di vedere da quanto tempo stiamo
// praticando e/o da quanto tempo stiamo provando quel dato esercizio. La classe è definita
// nella libreria allegata DS1307RTC e richiamata attraverso l'header 'DS1307RTC.h'.
DS1307RTC RealTimeClock;

// Le variabile condivise tra le ISR (Interrupt Service Routine) e usuali funzioni dovrebbero
// essere dichiarate sempre 'volatile'. Tale dichiarazione fa "capire" al compilatore che le
// suddette variabili potrebbero cambiare in ogni istante. Questo è proprio il caso delle ISR
// le quali potrebbero essere richiamate da un interrupt in qualsiasi momento con la conseguenza
// di cambiare, in maniera sincrona rispetto alla usuale esecuzione del codice, le variabili
// interessate alla chiamata. Una di queste è proprio la variabile 'bpm' la quale deve variare
// nel momento in cui ruotiamo l'encoder.
volatile int bpm=BPM_DEFAULT;
// Utilizziamo la variabile 'read_value' per memorizzare lo stato del registro PIND (datasheet
// allegato, pagina 124, paragrafo 18.4.10).
volatile byte read_value=0;
// Le due variabili che seguono, 'A_state' e 'B_state', verranno utilizzate nelle 2 ISR per la
// verifica del verso di rotazione dell'encoder. Ad esempio, ricordando che i terminali di un
// encoder in genere sono identificati dalle lettere 'A' e 'B', se ruotiamo l'asse dell'encoder
// in un verso ci aspettiamo un fronte di salita sul pin 'A', se lo ruotiamo nel verso opposto
// il fronte di salita è atteso sul pin 'B'. Poiché i valori delle due variabili verranno
// condivise tra le 2 ISR, gioco forza dovranno essere dichiarate 'volatile'.
volatile byte A_state = 0;
volatile byte B_state = 0;
// Il numero massimo di battute in una misura che andremo a marcare con il nostro metronomo sarà
// pari a 7 (ad esempio 7/4) questo vuol dire che una variabile di tipo 'byte', visto che spazia
// tra 0 e 255, può tranquillamente coprire il range di cui sopra. Così facendo risparmieremo 1
// byte: ricordiamo infatti che il tipo 'int' occupa 16 bit - ovvero 2 byte - mentre il tipo 'byte'
// occupa solo 8 bit, appunto 1 byte.
byte beat_value=0;
// Analoghe considerazioni per la variabile 'beats' che indica il numero di battute contate fino
// a quel momento.
byte beats=0;
// Nella variabile che segue memorizzeremo il valore della conversione dal bpm a ms.
unsigned long time_ms=0;
// Utilizzeremo la variabile 'start_time' per entrare in una istruzione condizionale (un 'if')
// e far partire il tempo del conteggio attraverso l'uso della funzione 'millis()', leggere
// più avanti. Poiché tale variabile assumerà valori 0 o 1 il tipo 'byte' è più che sufficiente.
byte start_time=0;
// Utilizziamo la variabile 'previous_time' per memorizzare il valore della funzione 'millis()'
// nel momento in cui entriamo nell'istruzione condizionale della variabile precedente. Tale
// valore verrà utilizzato per una verifica del tempo trascorso.
unsigned long previous_time=0;
// Variabile utilizzata per permettere il conteggio del numero delle battute, abilitare il
// buzzer e far accendere i led.
byte count=0;


// *************************** //
// ********** SETUP ********** //
// *************************** //
void setup()
{
  /*---------- Inizializziamo il display LCD ----------*/
  lcd.init();
  // Attiviamo la retroilluminazione.
  lcd.backlight();
  
  /*------- Inizializziamo i pin di I/O -------*/
  pinMode(INTERRUPT1,INPUT);
  pinMode(INTERRUPT2,INPUT);
  pinMode(LED_ROSSO,OUTPUT);
  pinMode(LED_VERDE,OUTPUT);
  
  // Il micro della Arduino Uno presenta delle resistenze interne di pull-up (collegano
  // l'uscita al positivo di alimentazione, in genere 5V o 3,3V). Con la direttiva HIGH
  // attiviamo queste resistenze su quel dato pin.
  digitalWrite(INTERRUPT1,HIGH);
  digitalWrite(INTERRUPT2,HIGH);
  digitalWrite(LED_ROSSO,LOW);
  digitalWrite(LED_VERDE,LOW);  
  // A seconda del tipo di applicazioni, alcune azioni dovrebbero essere rilevate e interpretate
  // dal sistema quanto più velocemente possibile, senza tenerlo impegnato per lunghi periodi
  // di tempo. Un'azione, che all'atto pratico si quantifica in una variazione di segnale, può
  // essere rilevata secondo due tipiche modalità: 'polling' e 'interrupt'. Il 'polling' indica
  // una verifica continua sull'ingresso che riceve il segnale in questione e ciò significa
  // realizzare una routine che periodicamente vada in esecuzione bloccando ciò che si stava
  // facendo in quel momento. Sicuramente una modalità poco efficiente poiché, se pensiamo ad
  // un sistema con poche risorse, significa riservarne alcune solo per una verifica periodica
  // che potrebbe non dare mai alcun risultato.
  // La seconda soluzione vede l'utilizzo degli 'interrupt' che, come vedremo, sarà molto più
  // veloce e non necessitiamo di codice con verifiche periodiche. Il microcontrollore a bordo
  // della Arduino Uno, ovvero l'ATMega328P-PU, presenta due tipi di interrupt:
  // 
  // - External Interrupt;
  // - Pin Change Interrupt;
  //
  // Due sono i pin riservati agli INTERRUPT ESTERNI, di preciso il 4 (INT0) e il 5 (INT1)
  // corrispondenti ai pin numero 2 e 3 sul connettore digitale della Arduino Uno. Invece i
  // pin che sul microcontrollore riportano la sigla PCINT0 fino a PCINT15 sono assegnati ai
  // Pin Change Interrupt.
  // La differenza, per diverse applicazioni, può essere abbstanza consistente, infatti i pin
  // che fanno parte dei Pin Change Interrupt possono rilevare, ovvero vengono triggerati,
  // solo in presenza di un cambio di condizione logica sul corrispondente pin, mentre i pin
  // associati agli interrupt esterni hanno diverse modalità che adesso andremo a riportare.
  // Nel progetto che segue faremo riferimento solo all'uso degli interrupt esterni i quali
  // possono essere triggerati in presenza di:
  //
  // - RISING: sul fronte di salita del segnale, ovvero quando il pin passa da una condizione
  //           logica 0 a una condizione logica 1;
  //
  // - FALLING: corrispondente al fronte di discesa, ovvero quando il segnale passa da una
  //            condizione logica 1 a una condizione logica 0;
  //
  // - CHANGE: l'interrupt viene triggerato in presenza di un cambio di stato,
  //           qualunque esso sia;
  //
  // - LOW: che triggera l’interrupt quando il pin è posto al livello logico basso.
  //
  // Esiste anche la condizione HIGH, opposta alla LOW, ma il µC della Arduino Uno non la
  // supporta.
  // Premesso ciò vediamo come utilizzare gli interrupt esterni nel µC in uso. Utilizzando
  // l'IDE Arduino il tutto si riduce alla chiamata di una funzione il cui prototipo vede:
  //
  // attachInterrupt(digitalPinToInterrupt(interruptPin), ISR, Modalità)
  // 
  // Laddove 'digitalPinToInterrupt(interruptPin)' indica il pin che si vuole utilizzare per
  // sollevare un interrupt. Il secondo parametro, ISR, è la chiamata alla Interrupt Service
  // Routine (Gestione di servizio dell'interrupt), ovvero alla funzione che si vuole venga
  // eseguita nel momento in cui viene rilevato un interrupt esterno. Infine il terzo parametro
  // indica una delle modalità riportate poco sopra e supportate dal µC, naturalmente da
  // scegliere in funzione dei propri obiettivi.
  // Nel nostro caso chiameremo le due ISR 'updateEncoderA' e 'updateEncoderB' nel passaggio
  // da una condizione logica bassa a una condizione logica alta sul pin 2 della Arduino Uno
  // (pin 4 del micro) e sul pin 3 (pin 5 del micro), utilizzando la direttiva 'RISING' della
  // funzione 'attachInterrupt()'.
  attachInterrupt(digitalPinToInterrupt(INTERRUPT1),updateEncoderA,RISING);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT2),updateEncoderB,RISING);
  // Appare scontato osservare come utilizzeremo l'encoder incrementale con il suo pulsante
  // integrato per svolgere tutte le funzioni: l'inserimento dei dati, la navigazione e la
  // selezione dei parametri da modificare.  
  //*------------ Impostiamo delle variabili di default ----------*/
  bpm=BPM_DEFAULT;
  beat_value=BATTUTE_DEFAULT;
  
  delay(100);
}


// ************************ //
// ********* LOOP ********* //
// ************************ //
void loop()
{
  // Chiamiamo la funzione che visualizzerà, e aggiornerà, l'attuale stato sul display LCD
  showValues();
  // Se la variabile 'count' diventa alta eseguiamo le azioni necessarie che vedono la
  // accensione del led rosso o verde in funzione della posizione nel marcare la battuta,
  // e il tono del buzzer (alto se marca l'1, basso per tutto il resto).
  if (count)
  {
    // Verifichiamo la posizione all'interno della misura.
    if (beats < beat_value)
    {
      // Se la battuta marcata è la numero 1 allora tono alto del buzzer e accensione del led
      // rosso. 
      if (beats==0)
      {
        //
        // tone(8, NOTE_G7, BEEP_TIME);
        //
        digitalWrite(LED_ROSSO,HIGH);       
      }
      // Se invece la battuta marcata è diversa dalla numero 1 allora tono basso del buzzer e
      // accensione del led verde.
      else
      {
        //
        // tone(8, NOTE_C5, BEEP_TIME);
        //
        digitalWrite(LED_VERDE,HIGH);
      }
      
      // Incrementiamo il numero di battuta...
      beats++;
      // ...e se questa diventa maggiore o uguale alle battute impostate per ogni misura,
      // azzeriamo la variabile corrispondente. Ricordiamo che il conteggio parte da 0.
      if (beats >= beat_value)
        beats=0;
    }
  }
  //
  verificaTempo();
}


// ################## //
// #### FUNZIONI #### //
// ################## //
//
void showValues()
{
  // Visualizziamo i bpm, pulsazioni per ogni minuto.
  lcd.setCursor(0,0);
  lcd.print("Tempo: ");
  lcd.print(bpm);
  //
  lcd.setCursor(13,0);
  // Visualizziamo il numero di battute impostate per una misura.
  lcd.print("B:");
  lcd.print(beat_value);
  // Visualizziamo l'orario mantenuto dall'RTC.
  tmElements_t tm;
    RealTimeClock.read(tm);
    lcd.setCursor(8,1);
    print2digits(tm.Hour);
    lcd.print(":");
    print2digits(tm.Minute);
    lcd.print(":");
    print2digits(tm.Second);
}

// 
void print2digits(int number)
{
  if (number >= 0 && number < 10)
  {
    lcd.print('0');
  }
  lcd.print(number);
}

//
void verificaTempo()
{
  unsigned int time_ms=0;
  // Convertiamo i bpm in millesimi di secondo. Ricordiamo che la sigla bpm indica le battute
  // per minuto. Quindi, ad esempio, 60 bpm indicano 60 battute al minuto ovvero una battuta al
  // secondo (60 battute / 60 secondi = 1), quindi una battuta ogni 1000 millesimi di secondo.
  // Ancora, 100 bpm indicano 100 battute al minuto ovvero 100/60=1,667 battute al secondo e
  // corrispondenti a "(60/bpm)*1000" millesimi di secondo, ovvero 600ms tra una battuta e la
  // successiva. In definitiva la semplice formula per convertire i bpm in ms è la seguente:
  time_ms=60000.0/bpm;
  //
  if (start_time == 0)
  {
    count=0;
    start_time=1;
    // Definizione della funzione 'millis()': conta il numero di millisecondi da quando lo
    // sketch è stato lanciato (ovvero da quando parte l'esecuzione all'interno del uC).
    // La funzione 'millis()' ritorna un valore di tipo 'unsigned long' il che vuol dire
    // che, su un microcontrollore come quello in uso, è una grandezza caratterizzata da
    // 4 byte (32 bit) e pertanto per una variabile senza segno il range di valori è pari
    // a 2^32-1, ovvero:
    //
    // 0 ------> 4.294.967.295
    //
    // Poiché il massimo valore è pari a poco meno di 4,3 miliardi di millisecondi
    // corrispondenti a:
    //
    // 4.294.967.295 / 1000 = 4.294.967,295 secondi, ovvero:
    // 4.294.967,295 / 60   = 71.582,78 minuti, ovvero:
    // 71.582,78     / 60   = 1.193,05 ore, ovvero:
    // 1.193,05      / 24   = 49,7 giorni!
    //
    // allora la funzione va in roll-over ogni 50 giorni circa, questo ipotizzando che il
    // metronomo sia perennemente acceso e che una persona si alleni per tale periodo senza
    // mai fermarsi, cosa palesemente poco credibile!
    // Per questo motivo è stata omessa la funzione di manipolazione del roll-over della
    // suddetta funzione. Facciamo presente che in luogo della funzione 'millis()' si può
    // pensare di usare la funzione 'micros()' la quale, eseguendo calcoli simili, evidenzierà
    // un roll-over ogni 72 minuti circa.
    previous_time=millis();
  }
  else
  {
    //
    if (millis()-previous_time >= LED_IN_ON)
    {
      // Il led verde è collegato al pin 13 del microcontrollore, ovvero il pin PD7, pertanto
      // con del valore contenuto nel registro PIND ne facciamo un and bit a bit con il numero
      // '0x80' (in binario: 100000000, ovvero una maschera di bit che isola solo il pin PD7).
      // Se il risultato è 1 vuol dire che il led verde è acceso. Poiché siamo entrati in un
      // 'if' che indica il superamento del tempo di accensione del led, allora provvederemo
      // al suo spegnimento.
      if (PIND & 0x80) digitalWrite(LED_VERDE,LOW);
      // Analoghe considerazioni per lo stato del led rosso il quale, lo ricordiamo, marca l'1
      // della misura. Il led rosso è collegato al pin 11 del microcontrollore ovvero al pin
      // marcato PD5 pertanto la maschera di bit non potrà che isolare tale pin e di valore pari
      // a '0x20' (in binario: 00100000).
      else if (PIND & 0x20) digitalWrite(LED_ROSSO,LOW);
    }
    //
    if (millis()-previous_time >= time_ms)
    {
      previous_time=0;
      start_time=0;
      count=1;
    }
   else return;
  }    
  return;
}

// ################################### //
// #### INTERRUPT SERVICE ROUTINE #### //
// ################################### //
// Analizziamo la dinamica degli interrupt e la chiamata alle due ISR riportate di seguito. In
// posizione di "riposo" entrambi i terminali dell'encoder, quindi sia 'A' che 'B', si trovano in
// condizione logica alta (HIGH, massima tensione positiva - +5V - come è facile verificare dal
// datasheet allegato dell'encoder utilizzato). Se a questo punto ruotiamo l'encoder di 1 solo
// passo in senso orario (CW, ClockWise) osserviamo come va a massa (LOW, tensione 0V) prima il
// terminale 'A' dell'encoder seguito dal terminale 'B'. Continuando a ruotare l'asse dell'encoder
// al fine di raggiungere il "click" (o passo) successivo, i contatti interni vedranno il terminale
// 'A' passare in condizione logica 1 (HIGH, massima tensione positiva) determinando così un fronte
// di salita del segnale (RISING) che verrà intercettato dal microcontrollore e originerà il primo
// interrupt. Poiché il fronte di salita avviene sul terminale 'A' dell'encoder, ovvero sul pin 4
// del microcontrollore, l'interrupt chiamerà l'ISR 'updateEncoderA()'. All'atto della chiamata il
// terminale 'B' dell'encoder è ancora a 0 (e con esso anche il pin 5 del microcontrollore) pertanto
// il registro PIND avrà uno 0 in PIND3 e un 1 in PIND2. Questa situazione farà si che nella ISR
// 'updateEncoderA()' venga eseguita l'istruzione 'else if ()' impostando così la variabile 'B_state'
// a 1. Nel terminare la rotazione dell'encoder, all'atto del "click", il terminale 'A' rimane in
// condizione logica 1 mentre il terminale 'B' vede un fronte di salita. Poiché il terminale 'B'
// dell'encoder è collegato al pin 5 del microcontrollore ecco che il fronte di salita su tale pin
// originerà un altro interrupt che richiamerà, questa volta, l'ISR 'updateEncoderB()'. All'atto
// della nuova chiamata il registro PIND vede 'PIND2=1' e 'PIND3=1' nonché la variabile 'B_state=1'
// pertanto verrà eseguita l'istruzione 'if ()' la quale non fa altro che aumentare di una unità la
// variabile 'bpm'. Un analogo ragionamento possiamo fare se ruotassimo di un passo l'encoder in senso
// antiorario.
//
// È importante ricordare che 1 sola ISR alla volta può essere lanciata e che durante la sua esecuzione
// risulta che:
//
// - La funzione 'millis()' non incrementa il proprio conteggio;
//
// - La funzione 'delay()', poiché necessita di un interrupt per poter funzione, durante la esecuzione
//   di una ISR non funziona;
//
// - La funzione 'micros()' inizialmente funziona ma poi il suo comportamento diventa errato dopo un
//   paio di chiamate alle ISR;
//
// - La funzione 'delayMicroseconds()', non utilizzando alcun contatore, continuerà a funzionare
//   regolarmente.
//
// Osserviamo come l'orologio non venga assolutamente influenzato nel conteggio e questo perché è un
// conteggio a se stante: esiste un integrato dedicato, con tanto di batteria tampone, che va avanti
// indipendentemente dal fatto che possa venir meno l'alimentazione e/o dal numero di interrupt
// esterni/interni verificatisi.
//
void updateEncoderA()
{
  // Siamo entrati nella ISR, pertanto abbiamo catturato un'interrupt esterno. Per assicurarci
  // un operazione atomica disabilitiamo temporaneamente, e globalmente, gli interrupt previo
  // uso dell'istruzione 'cli()'. Quando si utilizza questa istruzione per disabilitare gli
  // interrupt, essi verranno immediatamente disattivati. Questo vuol dire che nessun interrupt
  // verrà eseguito dopo tale istruzione, anche se si verifica in contemporanea con l'istruzione
  // medesima!
  cli();
  // Leggiamo lo stato degli otto pin associati agli input/output PORTD quindi creiamo un and
  // bit a bit (operatore '&') al fine di estrapolare solo i valori delle porte PD2 e PD3 cioè
  // i pin 4 e 5 del microcontrollore (ovvero i pin digitali 2 e 3 sulla Arduino Uno). Questo
  // risultato lo otteniamo utilizzando la maschera '00001100' (in esadecimale 0xC). Dal punto
  // di vista pratico andiamo a leggere il contenuto del registro PIND (pagina 124, paragrafo
  // 18.4.10 del datasheet allegato) il quale contiene lo stato attuale dei pin del uC facenti
  // capo alle sigle PD0...PD7 (vedere lo schema allegato dell'Arduino Uno o fare riferimento
  // al datasheet del microncontrollore a pagina 14, paragrafo 5.1).
  read_value=PIND & 0xC;
  // Verifichiamo se entrambi i pin associati ai due interrupt esterni si trovino in uno stato
  // alto (condiziona logica 1) nel qual caso o meno check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
  if (read_value == 0b00001100 && A_state)
  {
    bpm--;
    // Azzeriamo le due variabili di appoggio in attesa di un successivo segnale esterno.
    // Poiché le due variabili vengono utilizzate tra le due ISR, all'inizio del sorgente
    // le abbiamo dichiarate 'volatile'.
    B_state=0;
    A_state=0;
  }
  // Se il pin B dell'encoder è invece al livello basso impostiamo la variabile XXXX a 1signal that we're expecting pinB to signal the transition to detent from free rotation
  else if (read_value == 0b00000100) B_state=1;
  // La funzione 'sei()' abilita nuovamente gli interrupt affinché il microcontrollore possa
  // catturarne di nuovi all'occorrenza (esterni e/o interni che siano). Questo fa intuire il
  // motivo in base al quale la ISR debba essere la più corta possibile e fare solo ed
  // esclusivamente lo stretto necessario. Quando si utilizza l'istruzione 'sei()' per attivare
  // gli interrupt, l'istruzione successiva a 'sei()' verrà eseguita prima di qualsiasi altro
  // interrupt in sospeso.
  sei();
}

// Considerazioni analoghe scritte per la precedente ISR.
void updateEncoderB()
{
  cli();
  read_value=PIND & 0xC;
  if (read_value == 0b00001100 && B_state)
  {
    bpm++;
    B_state=0;
    A_state=0;
  }
  else if (read_value == 0b00001000) A_state=1;
  sei();
}
