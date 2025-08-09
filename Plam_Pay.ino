#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <qrcode.h>

// SIM800C RX/TX
#define SIM800_RX D3
#define SIM800_TX D4
SoftwareSerial sim800(SIM800_RX, SIM800_TX);

// TFT pins (adjust based on your wiring)
#define TFT_CS    4
#define TFT_RST   16
#define TFT_DC    5

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

QRCode qrcode;

// SMS buffer
String smsBuffer = "";
bool newSMS = false;

// Store parsed messages
struct Transaction {
  String txnId;
  String amount;
};

#define MAX_TRANSACTIONS 5
Transaction transactions[MAX_TRANSACTIONS];
int txnCount = 0;
float totalAmount = 0.0;

void setup() {
  Serial.begin(9600);
  sim800.begin(9600);

  // LCD init
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);
  tft.setTextWrap(false);
  tft.setTextColor(ST77XX_WHITE);

  printToLCD("Initializing...");
  delay(1000);

  // SIM800 init
  sim800.println("AT");
  delay(1000);
  sim800.println("AT+CMGF=1"); // Text mode
  delay(1000);
  sim800.println("AT+CNMI=1,2,0,0,0"); // Direct SMS to serial
  delay(1000);

  Serial.println("Ready to receive SMS...");
  printToLCD("Waiting for SMS...");

  QR_GEN();
}

void loop() {


  while (sim800.available()) {
    char c = sim800.read();
    smsBuffer += c;
    if (c == '\n') {
      if (smsBuffer.indexOf("Received") > 0 || smsBuffer.indexOf("credited") > 0 || smsBuffer.indexOf("Credited") > 0 || smsBuffer.indexOf("UPI Ref:") > 0) {

        newSMS = true;
      }
      if (newSMS) {
        parseAndStoreSMS(smsBuffer);
        updateLCD();
        delay(20000);
        QR_GEN();
        smsBuffer = "";
        newSMS = false;
      } else {
        smsBuffer = "";
      }
    }
  }
}



void QR_GEN(){
   tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  String data = "upi://pay?pa=8971488169@ibl&pn=Rahul&cu=INR";  // Replace with any string
  const uint8_t version = 3;

  // Move buffer to local scope with fixed size
  uint8_t qrcodeData[qrcode_getBufferSize(version)];

  qrcode_initText(&qrcode, qrcodeData, version, 0, data.c_str());

  int size = qrcode.size;
  int pixelSize = 4;
  int offsetX = (tft.width() - size * pixelSize) / 2;
  int offsetY = (tft.height() - size * pixelSize) / 2;

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      uint16_t color = qrcode_getModule(&qrcode, x, y) ? ST77XX_WHITE : ST77XX_BLACK;
      tft.fillRect(offsetX + x * pixelSize, offsetY + y * pixelSize, pixelSize, pixelSize, color);
    }
  }
}


// Extract amount and txn id, and store + update total
void parseAndStoreSMS(String sms) {
  String amount = "";
  String txn = "";

  // ===== Extract amount =====
  int amountIndex = -1;
  if ((amountIndex = sms.indexOf("INR")) != -1) {
    amountIndex += 3;
  } else if ((amountIndex = sms.indexOf("Rs.")) != -1) {
    amountIndex += 3;
  } else if ((amountIndex = sms.indexOf("Rs")) != -1) {
    amountIndex += 2;
  }

  if (amountIndex != -1) {
    while (amountIndex < sms.length() && !isDigit(sms.charAt(amountIndex))) {
      amountIndex++;
    }
    int endIndex = amountIndex;
    while (endIndex < sms.length() && (isDigit(sms.charAt(endIndex)) || sms.charAt(endIndex) == '.' || sms.charAt(endIndex) == ',')) {
      endIndex++;
    }
    amount = sms.substring(amountIndex, endIndex);
    amount.replace(",", "");
  }

  // ===== Extract Txn ID =====
  int txnIndex = -1;
  if ((txnIndex = sms.indexOf("Txn ID:")) != -1) {
    txnIndex += 7;
  } else if ((txnIndex = sms.indexOf("txn ID:")) != -1) {
    txnIndex += 7;
  } else if ((txnIndex = sms.indexOf("Ref:")) != -1) {
    txnIndex += 6;
  }

  if (txnIndex != -1) {
    while (txnIndex < sms.length() && sms.charAt(txnIndex) == ' ') {
      txnIndex++;
    }
    int endIndex = txnIndex;
    while (endIndex < sms.length() && sms.charAt(endIndex) != ' ' && sms.charAt(endIndex) != '\n') {
      endIndex++;
    }
    txn = sms.substring(txnIndex, endIndex);
  }

  // Fallbacks
  if (txn.length() < 2) txn = "no";
  if (amount.length() == 0) amount = "0.00";

  Serial.println("Txn No: " + txn + " | Amount: Rs." + amount);

  // Convert amount to float and update total
  float amt = amount.toFloat();
  totalAmount += amt;

  // Store transaction
  if (txnCount < MAX_TRANSACTIONS) {
    transactions[txnCount].txnId = txn;
    transactions[txnCount].amount = amount;
    txnCount++;
  } else {
    // Shift older transactions
    for (int i = 1; i < MAX_TRANSACTIONS; i++) {
      transactions[i - 1] = transactions[i];
    }
    transactions[MAX_TRANSACTIONS - 1].txnId = txn;
    transactions[MAX_TRANSACTIONS - 1].amount = amount;
  }
}

// LCD display
void updateLCD() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);

  for (int i = 0; i < txnCount; i++) {
    tft.setTextColor(ST77XX_GREEN);
    tft.print(i + 1);
    tft.print(". ");
    tft.setTextColor(ST77XX_WHITE);
    tft.print(transactions[i].amount);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(" | ");
    tft.setTextColor(ST77XX_WHITE);
    tft.println(transactions[i].txnId);
  }
  tft.setTextColor(ST77XX_CYAN);
  tft.println(F("---------------------"));
  tft.setTextColor(ST77XX_GREEN);
  tft.print(F("Total Rs.: "));
  tft.println(totalAmount, 2);
  tft.setTextColor(ST77XX_WHITE);
}

// Helper
void printToLCD(String msg) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.println(msg);
}
