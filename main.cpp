#include <Arduino.h>
#include <Wifi.h>
// enable button
#define button 13
int buttonState = 0;
int buttonOld;
// pins van de rechter motoren
#define MREnable1 4 // 1 & 2
#define MR1 16
#define MR2 17
#define MLEnable1 26 // 3 & 4
#define ML1 25  // hoog als vooruit
#define ML2 33 // hoog als achteruit
// pwm info
const int freq1 = 5000;
const int pwmChannel1 = 0;
const int pwmChannel2 = 2;
const int resolution = 8;
char prevDirection = 's';
char prevTurn = 'l';
char prevLine = 'r';
int speed = 230;
// reedcontact
#define reedPin 39
#define led 14
// ir sensoren
#define irLeft 19
#define irRight 18
// pins voor ultrasoon sensoren
const int trigObstacle = 23;   // Pins voor obstakel detecerende ultrasoon
const int echoObstacle = 34;         
const int trigDrop = 27;       // Pins voor de afgrond detecterende ultrasoon
const int echoDrop = 35;         
const int trigWall = 32;       // Pins voor de muur detecterende ultrasoon
const int echoWall = 36;
//variabelen voor ultrasoon sensoren
const int soundSpeed = 343.0; // Snelheid van het geluid (m/s)
int distanceObstacle = 0;
int distanceDrop = 0;
int distanceWall = 0;
int dropHeight = 100;

//wifi settings
const char* ssid     = "Tesla IoT"; //netwerk naam
const char* password = "fsL6HgjN";  //netwerk wachtwoord
WiFiServer server(80);
String header;
bool outputVooruitState = false;
bool outputLinksState = false;
bool outputRechtsState = false;
bool outputAchteruitState = false;
bool parkeren = false;
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

// functies
void motorControl(char, int);
void irSensors();
int ultraSonic(int, int);
void tunnelWall();
void reed();
void drop();
void obstakel();
void lijnen();
void sensorTest();
void motorToggle();
void buttonRead();
void parkerenFunctie();
void wifiInit();
void wifi();

void setup() {
  // pwm signal setup
  ledcSetup(pwmChannel1, freq1, resolution); // esp heeft geen analogwrite dus moeten we handmatig een pwm signaal instellen
  ledcSetup(pwmChannel2, freq1, resolution);
  ledcAttachPin(MREnable1, pwmChannel2);     // je moet het pwm signaal kopellen aan een pin
  ledcAttachPin(MLEnable1, pwmChannel1);
  // button
  pinMode(button, INPUT);
  attachInterrupt(button, buttonRead, RISING);
  // pinmodes motoren
  pinMode(MREnable1, OUTPUT);
  pinMode(MR1, OUTPUT);
  pinMode(MR2, OUTPUT);
  pinMode(MLEnable1, OUTPUT);
  pinMode(ML1, OUTPUT);
  pinMode(ML2, OUTPUT);
  // pinmode reedcontact
  pinMode(reedPin, INPUT);
  pinMode(led, OUTPUT);
  // pinmodes irsensoren
  pinMode(irLeft, INPUT);
  pinMode(irRight, INPUT);
  // pinmodes ultrasoonsensor
  pinMode(trigObstacle, OUTPUT);  // Zet trigger pin als uitgang
  pinMode(echoObstacle, INPUT);   // Zet echo pin als ingang
  pinMode(trigDrop, OUTPUT);
  pinMode(echoDrop, INPUT);
  pinMode(trigWall, OUTPUT);
  pinMode(echoWall, INPUT);
  // serial setup
  Serial.begin(115200);
  wifiInit();
}

void loop() {
  drop();
  obstakel();
  lijnen();
  tunnelWall();
  reed();
  while(parkeren){
    parkerenFunctie();
    wifi();
  }
  wifi();

}

void IRAM_ATTR buttonRead(){
  int buttonNew = 1;
  switch(buttonState){
    case 0:
    if (buttonNew == 1){
      buttonState = 1;
    }
    break;
    case 1: 
    if (buttonNew == 1){
      buttonState = 0;
    }
    break;
    motorControl(prevDirection, speed);
  }
}

// leest de ir sensoren uit en zorgt ervoor dat de motoren op de juiste manier aangestuurd worden
void lijnen(){
  Serial.println("ir");
  Serial.println(digitalRead(irLeft));
  Serial.println(digitalRead(irRight));
  // lijn rechts
  while(digitalRead(irLeft) == LOW && digitalRead(irRight) == HIGH){
    motorControl('l', speed);   // stuurt de acm naar links
    prevLine = 'r'; // zet prevLine als rechts om te onthouden waar de lijn als laatst was
    Serial.println("detect right, turn left");
  }
  // lijn links
  while (digitalRead(irLeft) == HIGH && digitalRead(irRight) == LOW){
    motorControl('r', speed);   
    prevLine = 'l';   
    Serial.println("detect left, turn right");
  }
  
  // geen lijn gedetecteerd
  if (digitalRead(irLeft) == LOW && digitalRead(irRight) == LOW){
    motorControl('f', speed);
    Serial.println("detect nothing");
  };

  if (digitalRead(irLeft) == HIGH && digitalRead(irRight) == HIGH){
    motorControl('b', speed);
    delay(400);
    if(prevLine == 'r'){
      motorControl('l', speed);
    } else {
      motorControl('r', speed);
    }
    delay(600);
    Serial.println("detect both");
  }
}
// gebruikt ultrasoon om afstand van de muur te bepalen
void tunnelWall(){
  Serial.println("tunnelWall()");
  distanceWall = ultraSonic(trigWall, echoWall);
  Serial.println(distanceWall);
  if (digitalRead(irLeft) == HIGH || digitalRead(irRight) == HIGH){
    return;
  }
  if (distanceWall <= 8 && distanceWall >= 4){
    motorControl('f', speed);
  }else if (distanceWall >= 8 && !(distanceWall >= 20)){
    motorControl('r', speed);
    delay(200);
    motorControl('f', speed);
    delay(200);
    motorControl('l', speed);
    delay(200);
    motorControl('f', speed);
  }else if (distanceWall <= 4 && distanceWall != 0){
    motorControl('l', speed);
    delay(200);
    motorControl('f', speed);
    delay(200);
    motorControl('r', speed);
    delay(200);
    motorControl('f', speed);
  }
}
// leest het reedcontact
void reed(){
  if(digitalRead(reedPin) == HIGH){
    digitalWrite(led, HIGH);
    Serial.println("high");
  }else {
    digitalWrite(led, LOW);
    Serial.println("low");
  }
}
// functie om ultrasoon te gebruiken om afstand te geven
int ultraSonic(int triggerPin, int echoPin){
  long echoTime = 0;
  int distance = 0;
  // Zend startpuls (trigger)
  digitalWrite(triggerPin, LOW);    // Make sure trigger pin is low
  delayMicroseconds(2);             // for at least 2µs
  digitalWrite(triggerPin, HIGH);   // Set trigger pin HIGH
  delayMicroseconds(10);            // for at least 10µs
  digitalWrite(triggerPin, LOW);    // Set LOW again
  // Meet lengte van de echo puls
  echoTime = pulseInLong(echoPin, HIGH);
  // en bereken hiermee de aftsand (in cm)
  distance = float(echoTime) / 2 * (soundSpeed / 10000.0);
  return distance;
}
// functie die alle ultrasoon sensoren afgaat en de afstand opslaat
void drop(){
  distanceDrop = ultraSonic(trigDrop, echoDrop);
  Serial.println("drop");
  Serial.println(distanceDrop);
  if (distanceDrop > 20 && distanceDrop < dropHeight){
    //achteruit
    motorControl('b', speed);
    delay(300);
    
    motorControl(prevTurn, speed);
    delay(1500);
    motorControl('f', speed);
  } else {
    motorControl('f', speed);
  }
}
// functie om obstakel te ontwijken
// de functie leest de ultrasoon uit die het obstakel moet detecteren, als dit zou is draait de auto naar de laatste lijn die is gedetecteert
void obstakel(){
  Serial.println("obstacle");
  distanceObstacle = ultraSonic(trigObstacle, echoObstacle);
  if (distanceObstacle < 10 && distanceObstacle != 0){
      motorControl(prevLine, speed);
      delay(1000); //seconde om te draaien
  }
}
// functie om motoren te besturen
// De functie gebruikt char currentDirection om de gewenste richting te bepalen en een waarde van 0 - 255 om de snelheid te bepalen
void motorControl(char currentDirection, int speed){
  if (buttonState == 0){
    currentDirection = 's';
  }
  if (currentDirection == prevDirection){
    return;
  }
  switch (currentDirection){
    case 'f':
      // rechts
      // motor 1 & 2
      ledcWrite(pwmChannel2, speed);
      digitalWrite(MR1, HIGH);
      digitalWrite(MR2, LOW);
      // links
      // motor 3 & 4
      ledcWrite(pwmChannel1, speed);
      digitalWrite(ML1, HIGH);
      digitalWrite(ML2, LOW);
      prevDirection = 'f';
    break;
    case 'b':
      // rechts
      // motor 1 & 2
      ledcWrite(pwmChannel2, speed);
      digitalWrite(MR1, LOW);
      digitalWrite(MR2, HIGH);
      // links
      // motor 3 & 4
      ledcWrite(pwmChannel1, speed);
      digitalWrite(ML1, LOW);
      digitalWrite(ML2, HIGH);
      prevDirection = 'b';
    break;
    case 'r':
      // rechts
      // motor 1 & 2
      ledcWrite(pwmChannel2, 255);
      digitalWrite(MR1, HIGH);
      digitalWrite(MR2, LOW);
      // links 
      // motor 3 & 4
      ledcWrite(pwmChannel1, 255);
      digitalWrite(ML1, LOW);
      digitalWrite(ML2, HIGH);
      prevDirection = 'r';
      prevTurn = 'r';
    break;
    case 'l':
      // rechts
      // motor 1 & 2
      ledcWrite(pwmChannel2, 255);
      digitalWrite(MR1, LOW);
      digitalWrite(MR2, HIGH);
      // links
      // motor 3 & 4      
      ledcWrite(pwmChannel1, 255);
      digitalWrite(ML1, HIGH);
      digitalWrite(ML2, LOW);
      prevDirection = 'l';
      prevTurn = 'l';
    break;
    case 's':
      // rechts
      // motor 1 & 2
      ledcWrite(pwmChannel2, 0);
      digitalWrite(MR1, LOW);
      digitalWrite(MR2, LOW);
      // links
      // motor 3 & 4
      ledcWrite(pwmChannel1, 0);
      digitalWrite(ML1, LOW);
      digitalWrite(ML2, LOW);
      prevDirection = 's';
    break;
    default:
      // rechts
      // motor 1 & 2
      ledcWrite(pwmChannel2, 0);
      digitalWrite(MR1, LOW);
      digitalWrite(MR2, LOW);
      // links
      // motor 3 & 4
      ledcWrite(pwmChannel1, 0);
      digitalWrite(ML1, LOW);
      digitalWrite(ML2, LOW);
      Serial.println("Default");
    break;
  }
}

void parkerenFunctie(){
  if(!parkeren){
    return;
  } else if(outputVooruitState){
    motorControl('f', speed);
  } else if(outputAchteruitState){
    motorControl('b', speed);
  } else if(outputLinksState){
    motorControl('l', speed);
  } else if(outputRechtsState){
    motorControl('r', speed);
  } else {
    motorControl('s', 0);
  }
}

void wifiInit(){  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void wifi(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            //check voor data in header
            if (header.indexOf("GET /Vooruit/true") >= 0) {
              outputVooruitState = true;
            } else if (header.indexOf("GET /Vooruit/false") >= 0) {
              outputVooruitState = false;
            } else if (header.indexOf("GET /Links/true") >= 0) {
              outputLinksState = true;
            } else if (header.indexOf("GET /Links/false") >= 0) {
              outputLinksState = false;
            } else if (header.indexOf("GET /Rechts/true") >= 0) {
              outputRechtsState = true;
            } else if (header.indexOf("GET /Rechts/false") >= 0) {
              outputRechtsState = false;
            } else if (header.indexOf("GET /Achteruit/true") >= 0) {
              outputAchteruitState = true;
            } else if (header.indexOf("GET /Achteruit/false") >= 0) {
              outputAchteruitState = false;
            } else if (header.indexOf("GET /Parkeren/true") >= 0) {
              parkeren = false;
            } else if (header.indexOf("GET /Parkeren/false") >= 0) {
              parkeren = true;
            }           
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            //CSS
            client.println("<style> html {font-family: Helvetica; text-align: center;} body {background-image: url('https://pbs.twimg.com/media/ExvB0YQWgAUU2nU?format=jpg&name=large');}h1 {width: 50%;margin-left: 25%;margin-right: 25%;}");
            client.println(".button {background-color: #195B6A;border: none;color: white;padding-top: 16px;padding-bottom: 16px;width: 160px;text-decoration: none;font-size: 30px;margin: 2px;cursor: pointer; border-radius: 8px;}");
            client.println(".button2 {background-color: #77878A;}.container {text-align: center; }");
            client.println(".container1 {text-align: center; display: inline-block; vertical-align: middle; margin-left: 1%; margin-right: 1%;}");
            client.println(".button:disabled {background-color: gray; cursor: not-allowed;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Controller</h1>");
            
            //BUTTONS ----------------------------------------------------------------------------
            if(parkeren){
              client.println("<p><a href=\"/Parkeren/true\"><button class=\"button button2\">Parkeren</button></a></p>");
            } else {
              client.println("<p><a href=\"/Parkeren/false\"><button class=\"button\">Parkeren</button></a></p>");
            }

            if(!parkeren){ //disabled button
              client.println("<p><a href=\"/Vooruit/true\"><button disabled class=\"button\">Vooruit</button></a></p>");
            } else if (!outputVooruitState){ //button
              client.println("<p><a href=\"/Vooruit/true\"><button class=\"button\">Vooruit</button></a></p>");
            } else {
              client.println("<p><a href=\"/Vooruit/false\"><button class=\"button button2\">Vooruit</button></a></p>");
            }

            client.println("<div class='container'>");
            client.println("<div class='container1'>");

            if(!parkeren){ //disabled button
              client.println("<p><a href=\"/Links/true\"><button disabled class=\"button\">Links</button></a></p>");
            } else if (!outputLinksState){ //button
              client.println("<p><a href=\"/Links/true\"><button class=\"button\">Links</button></a></p>");
            } else { //button2
              client.println("<p><a href=\"/Links/false\"><button class=\"button button2\">Links</button></a></p>");
            }

            client.println("</div>");
            client.println("<div class='container1'>");

            if(!parkeren){
              client.println("<p><a href=\"/Rechts/true\"><button disabled class=\"button\">Rechts</button></a></p>");
            } else if(!outputRechtsState){
              client.println("<p><a href=\"/Rechts/true\"><button class=\"button\">Rechts</button></a></p>");
            } else {
              client.println("<p><a href=\"/Rechts/false\"><button class=\"button button2\">Rechts</button></a></p>");
            }

            client.println("</div>");
            client.println("</div>");

            if(!parkeren){
              client.println("<p><a href=\"/Achteruit/true\"><button disabled class=\"button\">Achteruit</button></a></p>");
            } else if(!outputAchteruitState){
              client.println("<p><a href=\"/Achteruit/true\"><button class=\"button\">Achteruit</button></a></p>");
            } else {
              client.println("<p><a href=\"/Achteruit/false\"><button class=\"button button2\">Achteruit</button></a></p>");
            }
            //EIND BUTTONS ----------------------------------------------------------------------
           
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}