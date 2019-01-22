#include <LiquidCrystal_I2C.h>
#include <LiquidCrystal.h>
#include <Servo.h>
#include <SPI.h>
#include <RFID.h>
#include <SoftwareSerial.h>
#include "LiquidCrystal.h"

#define SS_PIN 53
#define RST_PIN 49

//On créé l'objet écran avec les ports de données correspondants où sont branchés les fils
LiquidCrystal_I2C lcd(0x27, 20, 21);
//LiquidCrystal lcd(7,6,5,4,3,2);

//On créé l'objet SIM900 (shield GPRS) avec les ports de données correspondants où sont branchés les fils
SoftwareSerial SIM900(10,11);

////////////////////À ADAPTER/////////////////////////// /

String idCompteur = "c2"; //ID du compteur
//String phone_number="+33635390667";
String APN = "free";
String USER = "";
String PASSWORD = "";

/*
* free : "free", "", ""
* lebara : "fr.lebara.movi", "wap", "wap"
*/

////NE PAS TOUCHER LE RESTE SAUF POUR AMELIORATION//////

RFID rfid(SS_PIN, RST_PIN);

// Setup variables
    //Variables générales
    char *rep; //la réponse du serveur
    int V; //Le volume d'eau
    int temps;
    int sum = 0;
    int requestDelay = 12000; // 15000 à l'origine
    int sendRequest = 0;
    int responseRequest = 0;
    int period = 5; //En Minutes
    int periodTime; //En millis secondes
    int echec = 0; //Compteur d'échec de requêtes envoyées (Cas courant, OK)
    int echecShieldGPRS = 0; //Compteur d'échec de requête après réinitialisation du Shield (Cas Rare)
  
    //Identifiant Carte 
    String num;

    //Variables Compteur d'eau
    const byte PIN_SIGNAL = 18;
    int compteur = 0;

    //Autres constantes utilisées
    const unsigned int NB_IMP_L= 97;

    //pins Boutons et sortie relai pour valve
    const int bouton1 = 14;
    const int bouton2 = 15;
    const int pinVanne = 17;

    //Volumes relatifs aux deux boutons
    int VBouton1 = 1;
    int VBouton2 = 2;
    
void setup()
{ 
  pinMode(17,OUTPUT);
  digitalWrite(17,LOW);

  //LCD setup
  lcd.init(); 
  lcd.backlight();
  pinMode(8,OUTPUT);
  digitalWrite(8,HIGH);

  //Baud Rate Serial + SIM900
  SIM900.begin(19200);  
  Serial.begin(19200);

  //SHIELD GPRS ACTIVÉ EN PERMANENCE
  sendATcommand("AT+CSCLK=0", "OK", 1000, 1000);
  
  //Sleeping mode - ÉCONOMISE DE L'ÉNERGIE MAIS ENDORT LE SHIELD AU BOUT D'UN TEMPS
  //sendATcommand("AT+CSCLK=1", "OK", 1000, 1000);

  //RFID Shield Activation
  SPI.begin(); 
  rfid.init();

  lcd.setCursor(0,0);
  lcdprint("CONNECTING ...");
  lcd.setCursor(0, 1);
  lcdprint("PLEASE WAIT");

      int r, a;
      Serial.println("START");

      do{
          //Code SIM
          sendATcommand("AT+CPIN=\"1234\"\r", "OK", 500, 2000);
          //Mode GPRS, Connexion à internet
          a = sendATcommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r","OK", 500, 2000);
          test_allumage(a); //Test d'allumage du Shield
          
          //Configuration du WAP spécifique à l'opérateur          
          a = sendATcommand((const char*) String("AT+SAPBR=3,1,\"APN\",\""+APN+"\"\r").c_str(), "OK",500, 2000);

          sendATcommand((const char*) String("AT+SAPBR=3,1,\"USER\",\""+USER+"\"\r").c_str(), "OK",500, 2000);
          sendATcommand((const char*) String("AT+SAPBR=3,1,\"PWD\",\""+PASSWORD+"\"\r").c_str(), "OK",500, 2000);
          test_allumage(a);
          
          sendATcommand("AT+SAPBR=1,1\r","OK", 500, 1000);
          
          r = sendATcommand3("AT+SAPBR=2,1\r", 500, 3000);
      }    
      while(r != 1);
      
      delay(2000);
      temps = millis();
}

void loop()
{
  lcd.setCursor(0,0);
  lcdprint("  SUNWATERLIFE");
  lcd.setCursor(0, 1);
  lcdprint("    WELCOME!    ");
  
  if (rfid.isCard()) { //Si une carte est détectée
        if (rfid.readCardSerial()) { //Si on peut la lire
              readUID(); //On lit la carte 
                   
              if(selectVol()){
                  sendHTTP();
                  Serial.println("END");
              }
       }
      delay(1000);
  }
  rfid.halt();

  //Tentative de reconnexion automatique toutes les "period" minutes pour éviter une déconnexion. "period" défini plus haut.
  periodTime = period * 60000;
  //Tentative de reconnexion automatique toutes les 5 minutes
  if( (millis() - temps)%periodTime >= periodTime-500 && (millis() - temps)%periodTime <= periodTime+500){
              getConnection();
  }
}

void readUID(){
                num = "";
                for(int s=0;s<5;s++){
                    num = num + (String) rfid.serNum[s];
                }
                Serial.println("Numéro de Carte : "+num);
                lcd.clear();
                lcdprint("CARD NUMBER     ");
                lcd.setCursor(0, 1);
                lcdprints(num);
                lcd.setCursor(0, 0);
                delay(1000);
}

void sendHTTP(){
          Serial.println("START");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcdprint("SENDING REQUEST");
          lcd.setCursor(0, 1);
          
          String url = "AT+HTTPPARA=\"URL\",http://vps372383.ovh.net/services/credit.php?num=" + num + "&qt=" + V + "&idC=" + idCompteur + "\r";
          const char *URL = url.c_str();

          //HTTP init, Paramétrage requête, Définition de l'URL
          sendATcommand("AT+HTTPINIT\r","OK",1000, 3000); // delais changé (2000, 5000 à l'origine)
          sendATcommand("AT+HTTPPARA=\"CID\",1\r","OK", 1000, 3000); // delais changé (2000, 5000 à l'origine)
          sendATcommand(URL,"OK",1000, 3000); // delais changé (2000, 5000 à l'origine)

          //Boucle DoWHile pour l'envoi de la requête POST
          do{
                sendRequest = sendATcommand("AT+HTTPACTION=0\r","OK",requestDelay, 10000);
                echec++;
          } while( (echec<5) && (sendRequest == 0) );
          echec = 0;

          //Boucle DoWHile pour la lecture de la réponse
          do{ 
                responseRequest = sendATcommand2("AT+HTTPREAD\r", "YYY", "NNN", "EEE", 4000, 10000);
                echec++;
          } while( (echec<5) && (responseRequest == 0) );
          echec = 0;

          /* If we always have response = 0 (Total Failure), 
           * We reset the GPRS connection and we start only one time the HTTP request.
           * One time must be enough, or it means the problem doesn't comme from the system :
           * See the battery, the network, or the circuits.
           * The echecShieldGPRS var allows us not to enter an infinite loop if it still doesn't work.
           * This var is put back to 0 when we enter the request (see sendATcommand2() function),
           * to proceed to the smae manipulation if this problem of GPRS link happens again.
           * (echecShieldGPRS is used as a kind of mutex)
           */
          if(responseRequest == 0 && echecShieldGPRS == 0){
              echecShieldGPRS++;  
              getConnection();
              sendHTTP();
          }
                    
          sendATcommand("AT+HTTPTERM\r","OK",2000,2000);
}

bool selectVol(){

  V = 0;
  bool choix = false;
  
  pinMode(bouton1, INPUT);
  digitalWrite(bouton1, HIGH);
  pinMode(bouton2, INPUT);
  digitalWrite(bouton2, HIGH);
  
                //Boucle d'attente de selection 5L ou 10L
                Serial.println("Sélectionnez un volume d'eau");
                lcd.clear();
                lcd.setCursor(0, 0);       
                lcdprint("SELECT VOLUME  :");
                lcd.setCursor(0, 1);
                
                while(!choix){
                    
                    if(!digitalRead(bouton1)) //Si le bouton 2 est pressé, on sélectionne 5L
                    {
                        V = VBouton1;
                        Serial.println("Vous venez de choisir " + (String) V + "L");
                        choix = true;
                        lcdprint("VOL : 1 LITERS "); //NBRE de litres a changer en fonction de ce qui a été décidé plus haut
                    }

                    else if(!digitalRead(bouton2)) //Si c'est le 3, on sélectionne 10L
                    {
                        V = VBouton2;
                        Serial.println("Vous venez de choisir " + (String) V + "L");
                        choix = true;
                        lcdprint("VOL : 2 LITERS ");
                    }
                }
                sum = sum + V;
                delay(1000);
    return choix;
}

void water(){
  
      int imp = NB_IMP_L * V;
      Serial.println("DEBUT " + (String) V + "L");
  
      digitalWrite(pinVanne,HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcdprint("Filling ...     ");
      //lcd.setCursor(0, 1);

      int pas = imp/16;
      int nc = 0;
      
      while(imp>0){
    
          if(pulseIn(PIN_SIGNAL, HIGH) > 0){
                 imp--;
                 Serial.println(imp);
           }
           if(imp%pas == 0 && nc<16){ //imp%x == 0 <=> une barre toute les x impulsions
            //  lcd.write("|");
             nc++;
           }
      } 
      digitalWrite(pinVanne,LOW);
      lcd.clear();
}

/* From there, there are 3 function senATcommand(), senATcommand2() and senATcommand3()
 * All of them send an AT command, but the difference is how the response is analysed.
 */

/* This function allows to send the main AT commands to establish the GPRS links,
 * the authentication to the operator and the configuration of HTTP request.
 * It analyzed the response with an expected anwser "expected_answer1" which should be "OK".
 * If it's not, it's "ERROR".
 */

int8_t sendATcommand(const char* ATcommand, const char* expected_answer1, unsigned int d, unsigned int timeout){

    uint8_t x=0, answer=0;
    char response[150];
    unsigned long previous;
    int t=0;

    memset(response, '\0', 100); // Initialize the string
    delay(100);

    while( SIM900.available() > 0) SIM900.read(); // Clean the input buffer

    SIM900.print(ATcommand); // Send the AT command 
    delay(d);
    
    x = 0;
    previous = millis();

    // this loop waits for the answer
    Serial.print("-->");
    do{
        if(SIM900.available() != 0){ 
            response[x] = SIM900.read();
            Serial.print(response[x]);
            x++;
 // check if the desired answer is in the response of the module
        if (strstr(response, expected_answer1) != NULL) {
            answer = 1;
        }
    }
    if((millis() - previous) >= timeout-1000 && t==0){
      t++;
      Serial.println("timeout");
    }    
 // Waits for the asnwer with time out
 }while((answer == 0) && ((millis() - previous) < timeout)); 

    Serial.println();
    if(SIM900.available() == 0){
        answer = 2;
    }

    return answer;
}

/* This function is close to previous one but only for the reading of the HTTP response of the server.
 * It is different because the answer is not only "OK" or "ERROR" but is the result whether the user has enough  
 * credits, or not, or is unknown, or the request failed. This function is the heart of the program.
 */

int8_t sendATcommand2(const char* ATcommand, const char* e1, const char* e2, const char* e3, unsigned int d, unsigned int timeout){

    uint8_t x=0, answer = 0;
    char response[100];
    unsigned long previous;
    int t=0;

    memset(response, '\0', 100); // Initialize the string
    delay(100);

   /* If this request follows a failed one that needed to restart the connection of GPRS Shield,
    * using the getConnection() function, (see above in sendHTTP() request), 
    * we must put back the echecShieldGPRS var to 0.
    * So let's in any case put echecShieldGPRS = 0, whether its value is 1 or 0. 
    */
    echecShieldGPRS = 0; 
    
    while( SIM900.available() > 0) SIM900.read(); // Clean the input buffer

    SIM900.print(ATcommand); // Send the AT command 
    delay(d);

    x = 0;
    previous = millis();

    // this loop waits for the answer
    Serial.print("-->");
    do{
        if(SIM900.available() != 0){ 
            response[x] = SIM900.read();
            Serial.print(response[x]);
            x++;
        }
        // check if the desired answer is in the response of the module
        if (strstr(response, e1) != NULL) {
            answer = 1;
        }
        if (strstr(response, e2) != NULL) {
            answer = 2;
        }
        if (strstr(response, e3) != NULL) {
            answer = 3;
        }
        if((millis() - previous) >= timeout-1000 && t==0){
            t++;
            Serial.println("timeout");
        }
}while( ((millis() - previous) < timeout) && SIM900.available() != 0);

    Serial.print("answer = ");
    Serial.print(answer);
    int sep = 0;
    
    switch(answer){
        case 0:
            //On retente la lecture de la réponse
            Serial.println("WATER REQUEST ERROR");
            Serial.println("-->RETRY");
            break;

        case 1:
            Serial.println("WATER REQUEST ACCEPTED");
            clean_screen();
            lcd.setCursor(0, 0);
            lcdprint("WATER REQUEST");
            lcd.setCursor(0, 1);
            lcdprint("ACCEPTED");
            delay(3000);
            lcd.clear();
            lcd.setCursor(0, 0);
            for(uint8_t i=0;i<strlen(response);i++){
                if(response[i] == 62){
                  sep = i;
                 }
            }
            for(uint8_t h=sep;h<strlen(response)-2;h++){
                if( (response[h]>=65 && response[h]<=90) || (response[h]>=97 && response[h]<=122) || response[h]==32){
                    lcdprintc(response[h]);  
                }
            }
            lcd.setCursor(0, 1);
            lcdprint("CREDITS : ");
            for(uint8_t h=strlen(response)-25;h<strlen(response);h++){
                if( response[h]>=48 && response[h]<=57 ){
                    lcdprintc(response[h]);  
                }
            }
            delay(5000);
            water();
            lcd.clear();
            break;
            
        case 2:
            Serial.println("WATER REQUEST REFUSED");
            clean_screen();
            lcd.setCursor(0, 0);
            lcdprint("WATER REQUEST");
            lcd.setCursor(0, 1);
            lcdprint("REFUSED");
            delay(3000);
           lcd.clear();
            lcd.setCursor(0, 0);
            for(uint8_t h=sep+34;h<strlen(response)-2;h++){
                if( (response[h]>=65 && response[h]<=90) || (response[h]>=97 && response[h]<=122) || response[h]==32){
                    lcdprintc(response[h]);  
                }
            }
            lcd.setCursor(0, 1);
            lcdprint("CREDITS : ");
            for(uint8_t h=strlen(response)-25;h<strlen(response);h++){
                if( response[h]>=48 && response[h]<=57 ){
                    lcdprintc(response[h]);  
                }
            }
            delay(5000);
            lcd.clear();
            break;

        case 3:
        Serial.println("WATER REQUEST UNKNOWN");
        clean_screen();
        lcd.setCursor(0, 0);
        lcdprint("UNKNOWN");
        lcd.setCursor(0, 1);
        lcdprint("USER");
        delay(3000);
        lcd.clear();
        break;
    }
    return answer;
}

/* This last function is for the etablishement of the GPRS link and an IP adress.
 * To be sure that the link is etablished, the IP adress must be different than 0.0.0.0.
 * This function ensure that, reset the connection if it's not, and save the IP adress. 
 */

int sendATcommand3(const char* ATcommand, unsigned int d, unsigned int timeout){

    uint8_t x=0, lenIP=0;
    char response[100];
    unsigned long previous;
    char ip[15];
    int answer = 0;

    ip[0] = 'e';
    
    memset(response, '\0', 100); // Initialize the string
    delay(100);

    while( SIM900.available() > 0) SIM900.read(); // Clean the input buffer

    SIM900.print(ATcommand); // Send the AT command 
    delay(d);
    
    x = 0;
    previous = millis();

    // this loop waits for the answer
    Serial.print("-->");
    do{
        if(SIM900.available() != 0){ 
            response[x] = SIM900.read();
            Serial.print(response[x]);
            x++;
         }
 // Waits for the answer with time out
 } while((SIM900.available() != 0) && ((millis() - previous) < timeout));

    Serial.println();
    for(int h=26;h<=42;h++){
        if( (response[h]>=48 && response[h]<=57) || response[h] == 46){
            ip[lenIP] = response[h];
            lenIP++;                
        }
    }

    if(ip[0] != '0' && ip[0] != 'e'){
          clean_screen();
          lcd.setCursor(0, 0);
          lcdprint("IP ADRESS:      ");
          lcd.setCursor(0, 1);
          for(int j = 0;j<lenIP;j++){
              lcdprintc(ip[j]);  
          }
    }

    if(ip[0] == 'e'){
        answer = 2;
        Serial.println("! Module GSM/GPRS éteint !");
        clean_screen();
        lcd.setCursor(0, 0);
        lcdprint("GSM OFF. PRESS");
        lcd.setCursor(0, 1);
        lcdprint("GSM BUTTON. WAIT.");
    }
    else if(ip[0] == '0'){
        answer = 0;
        Serial.println("! Echec de connexion ! --> Retentative");
    } 
    else{
        answer = 1;
    }
    return answer;
}

void getConnection(){
        Serial.println("-->Test de Connexion");
        lcd.setCursor(0, 0);
        lcdprint("Connection Test");
        lcd.setCursor(0,1);
        lcdprint("Connecting ..");
        Serial.println("Connexion en cours ..");
        int a, r;
        r = sendATcommand3("AT+SAPBR=2,1\r", 500, 5000);
         
        while(r != 1){

            //Code SIM
            sendATcommand("AT+CPIN=\"1234\"\r", "OK", 500, 2000);
            //Mode GPRS, Connexion à internet
            a = sendATcommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r","OK", 500, 2000);
            test_allumage(a); //Test d'allumage du Shield
          
            //Configuration du WAP spécifique à l'opérateur          
            a = sendATcommand((const char*) String("AT+SAPBR=3,1,\"APN\",\""+APN+"\"\r").c_str(), "OK",500, 2000);

            sendATcommand((const char*) String("AT+SAPBR=3,1,\"USER\",\""+USER+"\"\r").c_str(), "OK",500, 2000);
            sendATcommand((const char*) String("AT+SAPBR=3,1,\"PWD\",\""+PASSWORD+"\"\r").c_str(), "OK",500, 2000);
            test_allumage(a);
          
            sendATcommand("AT+SAPBR=1,1\r","OK", 500, 1000);
          
            r = sendATcommand3("AT+SAPBR=2,1\r", 500, 3000);
        } 
        if(r == 1){
              Serial.println("Connexion OK");
        }
        delay(1000); 
}

void test_allumage(int a){
     if(a == 2){
            clean_screen();
            lcd.setCursor(0, 0);
            lcdprint("GSM MODULE OFF");
            lcd.setCursor(0, 1);
            lcdprint("PRESS GSM BUTTON");
     }
     else{
            clean_screen();
            lcd.setCursor(0, 0);
            lcdprint("CONNECTING ...");
            lcd.setCursor(0, 1);
            lcdprint("PLEASE WAIT");  
     }  
}

void clean_screen(){
        lcd.setCursor(0,0);
        lcdprint("                ");
        lcd.setCursor(0,1);
        lcdprint("                ");  
}

void lcdprint(char *s) {
  for (int  i = 0; i < strlen(s); i++)  lcd.print(s[i]);
}

void lcdprints(String s) {
  for (int  i = 0; i < s.length(); i++)  lcd.print(s[i]);
}

void lcdprintc(char c) {
  lcd.print(c);
}

