#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h> // Adicionando a biblioteca TinyGPS++
#include <SD.h>
#include <FS.h>


#define CS_PIN 5
#define MOSI_PIN 23
#define MISO_PIN 19
#define SCK_PIN 18

const uint16_t OLED_Color_Black = 0x0000;
const uint16_t OLED_Color_Blue = 0x001F;
const uint16_t OLED_Color_Red = 0xF800;
const uint16_t OLED_Color_Green = 0x07E0;
const uint16_t OLED_Color_Cyan = 0x07FF;
const uint16_t OLED_Color_Magenta = 0xF81F;
const uint16_t OLED_Color_Yellow = 0xFFE0;
const uint16_t OLED_Color_White = 0xFFFF;

// As cores que realmente queremos usar
uint16_t OLED_Text_Color = OLED_Color_Yellow;
uint16_t OLED_Backround_Color = OLED_Color_Black;

const uint8_t OLED_pin_scl_sck = 14;
const uint8_t OLED_pin_sda_mosi = 13;
const uint8_t OLED_pin_cs_ss = 15;
const uint8_t OLED_pin_res_rst = 4;
const uint8_t OLED_pin_dc_rs = 16;

Adafruit_SSD1331 display = Adafruit_SSD1331(OLED_pin_cs_ss, OLED_pin_dc_rs, OLED_pin_sda_mosi, OLED_pin_scl_sck, OLED_pin_res_rst);

unsigned long update_display = 0;

unsigned long time_log = 0;

int incomingsensor;
int incomingdistance;
float incomingbat;

String success;


esp_now_peer_info peer_info[3]; // estrutura para guardar dados dos outros ESP
int count = 0;

SoftwareSerial Serial_software(18, 19); // Pinos para comunicação serial
TinyGPSPlus gps; // Inicializa o objeto TinyGPS++

int satellites = 0;
float decimalLatitude = 0.0;
float decimalLongitude = 0.0;
bool fix = false;



uint8_t BROADCST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_dados
{
    int distance;
    int sensor;
    float position;
} struct_message;

struct_dados incoming_readings;

#define TFT_GRAY 0xBDF7

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status == 0)
    {
        success = "Delivery Success :)";
    }
    else
    {
        success = "Delivery Fail :(";
    }
}
volatile boolean recording = false;

struct_message incomingReadings;


void start_recordings(){
    if(recording == false){
        SD.open("/data.csv", FILE_WRITE);
        SD.end();
        recording = true;
    }
    else{
        recording = false;
    }
    

    
}

int store_data(float latitude, float longitude, int distance, int sensor, float bat,int position){
    if(recording == false){
        return 0;
    }
    else{
        File file = SD.open("/data.csv", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return -1;
    }
    else{
        unsigned long seconds = millis() / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;

        String time = String(hours) + ":" + String(minutes % 60) + ":" + String(seconds % 60);

        file.print(time);
        file.print(";");
        file.print(latitude, 6);
        file.print(";");
        file.print(longitude, 6);
        file.print(";");
        file.print(position);
        file.print(";");
        file.print(distance);
        file.print(";");
        file.print(sensor);
        file.print(";");
        file.print(bat);
        file.println();
        file.close();
        return 1;
    }
    }
}




void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    Serial.print("Bytes received: ");
    Serial.println(len);
    incomingsensor = incomingReadings.sensor;
    Serial.println(incomingsensor);
    incomingdistance = incomingReadings.distance;
    Serial.println(incomingdistance);
    incomingbat = incomingReadings.position;
    Serial.println(incomingbat);
    store_data(decimalLatitude, decimalLongitude, incomingdistance, incomingsensor, incomingbat, 1);
}





void setup()
{
    update_display = millis();
    Serial.begin(9600);
    WiFi.mode(WIFI_STA);
    Serial_software.begin(9600);
    display.begin();
    display.setFont();
    display.fillScreen(OLED_Backround_Color);
    display.setTextColor(OLED_Text_Color);
    display.setTextSize(1);

    /* Tenta inicializar o espnow */
    if (esp_now_init())
    {
        Serial.println("Não foi possível iniciar");
        return;
    }
    else
    {
        Serial.println("Iniciado com sucesso");
    }

    /* Callback para enviar */
    esp_now_register_send_cb(OnDataSent);

    /* Callback para receber */
    esp_now_register_recv_cb(OnDataRecv);

    // Registrar peer
    memcpy(peer_info[0].peer_addr, BROADCST_ADDRESS, 6);
    peer_info[0].channel = 0;
    peer_info[0].encrypt = false;

    // Adiciona peer
    if (esp_now_add_peer(&peer_info[0]) != ESP_OK)
    {
        Serial.println("Falha ao adicionar peer");
        return;
    }
    Serial.println("Setup feito");


    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
    if (!SD.begin(CS_PIN)) {
        Serial.println("Card Mount Failed");
        return;
    }
    else
    {
        Serial.println("Card Mount Success");
    }

    //configure button interrupt
    pinMode(4, INPUT_PULLUP);
    attachInterrupt(4, start_recordings, FALLING);
}

void loop()
{
   

    while (Serial_software.available() > 0)
    {
        char gpsChar = Serial_software.read();
        Serial.write(gpsChar); // Ecoa os dados do GPS para depuração

        // Passa o caractere para o objeto TinyGPS++
        gps.encode(gpsChar);

        // Verifica se uma nova sentença foi recebida e, se sim, obtém os dados
        if (gps.location.isUpdated())
        {
            decimalLatitude = gps.location.lat();
            decimalLongitude = gps.location.lng();
            fix = gps.location.isValid();
            satellites = gps.satellites.value();

            Serial.print("Latitude: ");
            Serial.println(decimalLatitude, 6);
            Serial.print("Longitude: ");
            Serial.println(decimalLongitude, 6);
            Serial.print("Fix: ");
            Serial.println(fix ? "Yes" : "No");
            Serial.print("Satellites: ");
            Serial.println(satellites);
        }
    }

    // Atualiza o display OLED a cada 1 segundo
    if (millis() >= update_display + 1000)
    {
        display.fillScreen(OLED_Backround_Color);
        display.setCursor(0, 0);

        display.setTextColor(OLED_Color_Blue);
        display.print("Satellites: ");
        display.setTextColor(OLED_Color_Yellow);
        display.println(satellites);

        display.setTextColor(OLED_Color_Blue);
        display.print("Lat: ");
        display.setTextColor(OLED_Color_Yellow);
        display.println(decimalLatitude, 6);

        display.setTextColor(OLED_Color_Blue);
        display.print("Lon: ");
        display.setTextColor(OLED_Color_Yellow);
        display.println(decimalLongitude, 6);

        display.setTextColor(OLED_Color_Blue);
        display.print("Fix: ");
        display.setTextColor(fix ? OLED_Color_Green : OLED_Color_Red);
        display.println(fix ? "Yes" : "No");

        update_display = millis();
    }
}



