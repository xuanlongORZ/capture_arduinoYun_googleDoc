#include <Temboo.h>
#include <Process.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <Wire.h> //Ajout de la bibliothèque i2c pour utiliser le lcd
#include "rgb_lcd.h" //Ajout de la bibliothèque du lcd
#include "TembooAccount.h" // contains Temboo account information, as described below
#include "math.h"
//Pin difinitions
#define echoPin 12
#define trigPin 13

//valeurs for LCD 
#define PIN_NTC A0
rgb_lcd lcd;  //Constructeur rgb_lcd pour l'objet lcd
const int colorR = 255;
const int colorG = 255;
const int colorB = 255;

//valeurs for mesuring the distance
double duration, distance;
double moyen=0;
int count=0;
int maxDistance=0;
int minDistance=400;
int count_hors=0; 
float value[50];
int valuenum = 0;
int totalnum = 0;

//valeurs for uploading the data to the Cloud
int calls = 0;   // Execution count, so this doesn't run forever
int maxCalls = 1000;   // Maximum number of times the Choreo should be executed

//valeur for the service check
BridgeServer server;
int flag=0;

void setup() 
{
 Bridge.begin();
 Console.begin();
 Serial.begin (9600); 
 pinMode(trigPin, OUTPUT);
 pinMode(echoPin, INPUT);
 lcd.begin(16, 2); //Initialisation du lcd de 16 colonnes et 2 lignes    
 lcd.setRGB(colorR, colorG, colorB);
 // Listen for incoming connection only from localhost
 // (no one from the external network could connect)
 server.listenOnLocalhost();
 server.begin();
}


void loop(){
  BridgeClient client = server.accept();    // Get clients coming from server
  // Check if there is a new client
  if (client) {
    // Process request
    //Fonction: Checking Turn On or Turn Off operation
    String command = client.readStringUntil('/');
    // is "TurnOn" or "TurnOFF" command?
    if (command == "H" ||command == "h") {
      flag=1;
      client.println(F("State: Turn On (Upload mode)"));
      if(valuenum>0){
        client.println(F("   No.     Distance(cm): "));
        client.println(F("-------------------------"));
        for(int i=0;i<valuenum;i++){
            client.print(F("   "));
            client.print(i);
            client.print(F("       "));
            client.println(value[i]);
            client.println(F("-------------------------"));
        }
      }
      
    }else if(command == "L" || command =="l"){
      flag=0;
      client.println(F("State: Turn Off"));
    }else if(command =="F" || command == "f"){
      flag=-1;
      client.println(F("State: Turn On (Non-upload mode)"));
      if(valuenum>0){
        client.println(F("   No.     Distance(cm): "));
        client.println(F("-------------------------"));
        for(int i=0;i<valuenum;i++){
            client.print(F("   "));
            client.print(i);
            client.print(F("       "));
            client.println(value[i]);
            client.println(F("-------------------------"));
        }
      }
    }
    client.stop();// Close connection and free resources.
  }else if(flag==1){
    mesureDistance();
  }else if(flag==0){
    lcd.begin(16, 2); //Initialisation du lcd de 16 colonnes et 2 lignes   
    lcd.setRGB(colorR, colorG, colorB);
    lcd.print("    Turn Off"); //On écrit "Affichage : "
  }else if(flag==-1){
    mesureDistance();
    delay(175); // 50ms each iteration.
  }
  //If there is no new operation from mobile phone, 
  //the iteration of the mesuring will still run until we have a new operation
  
  delay(25); // 50ms each iteration.
  // 400cm is the limit of the sensor
  // generally we take speed of sound 340m/s, we can calculate: 4*2/340 = 0.0235s = 23.5ms
  // so 23.5ms is the min period for each delay
 
}

//Main Fonction: Mesuring the distance
void mesureDistance() { 
       double distance=0;
       // Envoie de l'onde
       digitalWrite(trigPin, LOW);
       delayMicroseconds(2);
       digitalWrite(trigPin, HIGH);
       delayMicroseconds(10);
       digitalWrite(trigPin, LOW);
       // Réception de l'écho
       duration = pulseIn(echoPin, HIGH);
       
       if (duration >= 23323.6 ||duration <= 116.6){
        count_hors++;
        if(count_hors>5) {
          lcd.begin(16, 2); //Initialisation du lcd de 16 colonnes et 2 lignes   
          lcd.setRGB(colorR, colorG, colorB);
          lcd.print("Beyond the limit"); //On écrit "Affichage : "
          count_hors = 0;
          //runappendrow(-1);
        }
       }else if(count<5){
        distance = duration/58.3;// Calcul de la distance
        if(maxDistance<=distance){
          maxDistance=distance;
        }
        if(minDistance>=distance){
          minDistance=distance;
        }
        
        if(maxDistance<=(minDistance+0.4)){
          moyen=distance+moyen;
          count++;
        }else{
          maxDistance=0;
          minDistance=400;
        }
      
       }else{
      
        moyen=(moyen/5)+0.65;
        if(moyen<6){
            moyen-=0.7;
        }else if(moyen<=30){
          moyen-=0.5;
        }else if(moyen>150 && moyen<250){
          moyen+=2;
        }else if(moyen>250){
          moyen+=4;
        }
        
        LCDprint(moyen);

        if(valuenum ==300) valuenum =0;
        value[valuenum] = moyen;
        valuenum ++;
        
        if(flag==1) runappendrow(moyen);
        moyen=0;
        count=0;
       }
}

void LCDprint(float moyen){
  if(moyen<=50){
        lcd.setRGB(255,0,0); 
      }else if(moyen>50 && moyen<=150){
        lcd.setRGB(0,255,0);    
      }else {
        lcd.setRGB(0,0,255);    
      }
      lcd.setCursor(0,0); //On commence à écrire en haut à gauche
      lcd.print("Distance: "); //On écrit "Affichage : "
      lcd.setCursor(9,0); //On se met sur la 13 ème colonne de la première ligne
      lcd.print(moyen); //On écrit le contenu de la variable cpt
      lcd.setCursor(14,0); //On commence à écrire en haut à gauche
      lcd.print("cm"); //On écrit "Affichage : "
}

//Fonction: Google spreadsheets connection 
void runappendrow(float sensorValue) {

  if (calls <= maxCalls) {
    
    TembooChoreo AppendRowChoreo;
    Console.println(1);
    // Invoke the Temboo client
    AppendRowChoreo.begin();
    
    // Set Temboo account credentials
    AppendRowChoreo.setAccountName(TEMBOO_ACCOUNT);
    AppendRowChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
    AppendRowChoreo.setAppKey(TEMBOO_APP_KEY);
    
    // Set Choreo inputs
    AppendRowChoreo.addInput("SpreadsheetTitle", "Télémetre à ultrason");
    AppendRowChoreo.addInput("RefreshToken", "1/2b85QpsPbIioMyJKtDX3x8y8imhF9nxe4IGAQe77XqU");
    AppendRowChoreo.addInput("RowData", "mesure start!");// add the RowData input item
    AppendRowChoreo.addInput("ClientSecret", "6DWaHeG-SnN_hyk2GDHleo1S");
    AppendRowChoreo.addInput("ClientID", "148196787179-tfqtmhq2r2gl1gqq1qp929ch3ul2ulmu.apps.googleusercontent.com");
    // Identify the Choreo to run
    AppendRowChoreo.setChoreo("/Library/Google/Spreadsheets/AppendRow");
    Console.println(2);
    String rowData(calls);
   // convert the time and sensor values to a comma separated string
    rowData += ",";
    rowData += sensorValue;
    AppendRowChoreo.addInput("RowData", rowData);// add the RowData input item
    calls++;
  Console.println(3);
   // run the Choreo and wait for the results
   // The return code (returnCode) will indicate success or failure 
    unsigned int returnCode = AppendRowChoreo.run();
    

    if (returnCode == 0) {
      Console.println("Success! Appended " + rowData);
      Console.println("");
    } else {
      // return code of anything other than zero means failure  
      // read and display any error messages
      while (AppendRowChoreo.available()) {
        char c = AppendRowChoreo.read();
        Console.print(c);
      }
    }
    AppendRowChoreo.close();
  }
}
 
















