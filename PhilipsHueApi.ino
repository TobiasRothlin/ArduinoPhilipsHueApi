//----------------------------------------------------------------------------------------
//File: PhilipsHueApi.ino
//
//Autor: Tobias Rothlin
//Versions:
//          1.0  Inital Version
//----------------------------------------------------------------------------------------

//Include Arduino Libary
#include <SPI.h>
#include <Ethernet.h>
#include <String.h>

//Request buffer size for incomming .json getHueState()
#define REQ_BUF_SZ   500

//Hue API parameters specific to Hue Bridge
const char hueHubIP[] = "192.168.1.14";  //Hue Bridge IP
const char hueUsername[] = "HdFwM9X9b1tOQHTkZ4sEe8B8JWNUPFjD6FyxZIbl";  //Hue Bridge username
const int hueHubPort = 80; //Always port 80

//External pins
int pinSwitchIn = 5;
int pinStatusLed = 6;

//System variables
int periodicCheckInterval = 10000;
int numberOfChecks = 0;
int durationLongpress = 500;

//Hue status variables
boolean hueLampIsOn;
int hueLampBrightness;
long hueLampHueValue;

//Network settings Arduino shield
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x7C, 0x0F }; //MAC address
IPAddress ip(192, 168, 1, 23); // Arduino IP
EthernetClient client;


//Initial Setup
void setup()
{
  //Set pin modes
  pinMode(10, OUTPUT); //Set pin 10 to High used by the ethernet shield and SPI.h lib
  digitalWrite(10, HIGH);

  pinMode(pinSwitchIn, INPUT);
  pinMode(pinStatusLed, OUTPUT);

  //Setup Serial Output @ Bauderate 9600
  Serial.begin(9600);

  //Setup the Ethernet
  Ethernet.begin(mac, ip);

  //Get the current state of the second Hue lamp
  getHueState(2);
}

void loop()
{
  //Delay to slow down polling frequency
  delay(1);

  //Indicates Arduino is in polling Mode -> Idel light (not nessesary)
  digitalWrite(pinStatusLed, HIGH);

  //Polling checks if the switch is presses
  if (digitalRead(pinSwitchIn) == 1)
  {
    //Turns off idel light
    digitalWrite(pinStatusLed, LOW);

    //Gets current state of the second Hue lamp
    getHueState(2);

    //Measures the duration of the button press
    int pressduration = 0;
    while (digitalRead(pinSwitchIn) == 1 && pressduration <= 100 )
    {
      delay(1);
      pressduration++;
    }

    //if the Hue lamp is turend on -> turn off Hue lamp
    if (hueLampIsOn == 1)
    {
      HueTurnOff(2);
    }
    //else -> turn on Hue lamp
    else
    {
      //if the button press is longer than 100ms -> the lamp will dim (Night light mode)
      if (pressduration > 100)
      {
        HueTurnOnDim(2,50);
      }
      //else -> turn on Hue lamp
      else
      {
        HueTurnOn(2);
      }
    }
    //Waits until the button is released
     while (digitalRead(pinSwitchIn) == 1 )
    {
      delay(1);
    }
  }
}

//Turns the Hue lamp on at max brightness
void HueTurnOn(int lightNum)
{
  String command = "{\"on\": true , \"bri\": 254}";
  setHue(lightNum, command);
}

//Turns the Hue lamp off
void HueTurnOff(int lightNum)
{
  String command = "{\"on\": false}";
  setHue(lightNum, command);
}

//Slowly increses the the brightness to the value bri
void HueTurnOnDim(int lightNum, int bri)
{
  String command = "{\"on\": true, \"bri\": " + String(bri) + ",\"transitiontime\": 200}";
  setHue(lightNum, command);
}

//The generic function to send commands to the Hue bridge
void setHue(int lightNum, String command) //int lightNum -> The lightNumber on the Hue bridge
//String command -> The .json with the light parameters.
//                  The exact form is shown in the API documantaion
//                  or seen in the functions above(HueTurnOn/HueTurnOnDim)
{
  if (client.connect(hueHubIP, hueHubPort))
  {
    //Used to terminate the connection after 100 loops
    int NumberOfLoops = 0;

    while (client.connected())
    {
      //send the data to the network shield ontop the Arduino
      client.print("PUT /api/");
      client.print(hueUsername);
      client.print("/lights/");
      client.print(lightNum);
      client.println("/state HTTP/1.1");
      client.println("keep-alive");
      client.print("Host: ");
      client.println(hueHubIP);
      client.print("Content-Length: ");
      client.println(command.length());
      client.println("Content-Type: text/plain;charset=UTF-8");
      client.println();
      client.println(command);

      NumberOfLoops++;

      if (NumberOfLoops == 100) {
        break;//Terminates the connection after 100 loops can depend on the speed of the network connection
      }

    }
    client.stop();
  }

}

//Get Hue state updates the variables
boolean getHueState(int lightNum)
{
  String Jason_req;
  String getJason;
  char Buf[500] = {""};
  int MaxLenght = 43;
  int currentLenght = 0;

  if (client.connect(hueHubIP, hueHubPort)) //Sends the request to the hue bridge
  {
    client.print("GET /api/");
    client.print(hueUsername);
    client.print("/lights/");
    client.print(lightNum);
    client.println("  HTTP/1.1");
    client.print("Host: ");
    client.println(hueHubIP);
    client.println("Content-type: application/json");
    client.println("keep-alive");
    client.println();

    //Saves the response .json in a String
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        Jason_req.toCharArray(Buf, Jason_req.length());
        if (strstr(Buf, "state") > 1) {
          currentLenght++;
          if (currentLenght < MaxLenght) {
            getJason = getJason + c;
          }
        }
        else {
          Jason_req = Jason_req + c;
        }
      }
    }


    String State = "";
    String bri = "";

    //Extracts the state of the Hue lamp
    int startPos = getJason.indexOf("on");
    int endPos = getJason.indexOf(",");
    State = getJason.substring(startPos + 4, endPos );
    getJason = getJason.substring(endPos + 1);

    //Sets the state of the Hue lamp
    if (State == "true") {
      hueLampIsOn = true;
    }
    else {
      hueLampIsOn = false;
    }

    //Extracts the brightness of the Hue lamp
    startPos = getJason.indexOf("bri");
    endPos = getJason.indexOf(",");
    bri = getJason.substring(startPos + 5, endPos );
    getJason = getJason.substring(endPos + 1);
    hueLampBrightness = bri.toInt();

    //Terminates the connection;
    client.stop();
    Jason_req = "";
    return true;
  }
  else
    return false;
}
