#include <HttpClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "FatFs_SD.h"
#include "RecordWav.h"
#include "AudioCodec.h"

#define RECORDBTN PUSH_BTN
#define SAMPLERATE 16000

#define BUFFERSIZE 512
int16_t buffer[BUFFERSIZE] = {0};

int record_counter = 0;     //录制次数
char record_file_name[20];
char absolute_filename[128];

FatFsSD fs;
RecordWav recWav;

// WiFi
char ssid[] = "SSID";     //  your network SSID (name)
char pass[] = "password"; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;          // your network key Index number (needed only for WEP)

// Http POST
// Name of the server we want to connect to
const char kHostname[] = "asr.hazhuzhu.com";
const char kPath[] = "/ameba_asr.php";
// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 3 * 1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;
int status = WL_IDLE_STATUS;

char Content_Type[] = "multipart/form-data; boundary=----WebKitFormBoundarypNjgoVtFRlzPquKE";
// 请求体头部及尾部数据
char post_start[] = "------WebKitFormBoundarypNjgoVtFRlzPquKE\r\nContent-Disposition: form-data; name=\"file\"; filename=\"ameba_recording\"\r\nContent-Type: application/octet-stream\r\n\r\n";
char post_end[] = "\r\n------WebKitFormBoundarypNjgoVtFRlzPquKE--\r\n";
const int post_start_len = strlen(post_start);
const int post_end_len = strlen(post_end);

int err = 0;

WiFiClient c;
HttpClient http(c);

// Callback function to read Microphone data from audio codec
void readCBFunc()
{
    if (Codec.readAvaliable())
    {
        Codec.readDataPage(buffer, BUFFERSIZE);
        recWav.writeAudioData(buffer, BUFFERSIZE);
    }
}

void setup()
{
    Serial.begin(115200);
    fs.begin();

    // recorder 初始化
    pinMode(RECORDBTN, INPUT);

    Codec.setSampleRate(SAMPLERATE);
    recWav.setSampleRate(SAMPLERATE);
    Codec.setReadCallback(readCBFunc);
    Codec.begin(TRUE, FALSE);

    // 设置 POST 相关变量
    uint8_t *post_content = NULL;
    http.mysetPostData(post_start, post_end, post_content, post_start_len, post_end_len, 0, Content_Type);

    // WiFi 初始化
    while (status != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);
        // wait 5 seconds for connection:
        delay(5000);
    }
    Serial.println("\n\rConnected to wifi");
    printWifiStatus();
}

void loop()
{
    if ((digitalRead(RECORDBTN) == HIGH) && (!recWav.fileOpened()))
    {
        // 按下按钮录制音频
        sprintf(record_file_name, "%d.wav", record_counter);
        sprintf(absolute_filename, "%s%s", fs.getRootPath(), record_file_name);
        Serial.println("Recording started");
        recWav.openFile(absolute_filename);
    }
    else if ((digitalRead(RECORDBTN) == LOW) && (recWav.fileOpened()))
    {   
        // 松开按钮停止录制
        Serial.println("Recording stopped");
        recWav.closeFile();
        // 文件保存后再打开文件,获取大小
        SdFatFile record_file = fs.open(absolute_filename);
        int record_file_len = record_file.size();
        printf("size:%d", record_file_len);

        // 设置 Post Content 相关变量
        http.mysetPostContent(NULL, record_file_len, post_start_len + record_file_len + post_end_len);
        // POST 音频数据
        err = http.mypost(kHostname, kPath, record_file);
        // 关闭文件句柄
        record_file.close();

        // 读取解析 response
        if (err == 0)
        {
            Serial.println("startedRequest ok");

            err = http.responseStatusCode();
            if (err >= 0)
            {
                Serial.print("Got status code: ");
                Serial.println(err);

                // Usually you'd check that the response code is 200 or a
                // similar "success" code (200-299) before carrying on,
                // but we'll print out whatever response we get

                err = http.skipResponseHeaders();
                if (err >= 0)
                {
                    int bodyLen = http.contentLength();
                    Serial.print("Content length is: ");
                    Serial.println(bodyLen);
                    Serial.println();
                    Serial.println("Body returned follows:");

                    // Now we've got to the body, so we can print it out
                    unsigned long timeoutStart = millis();
                    char c;
                    // Whilst we haven't timed out & haven't reached the end of the body
                    while ((http.connected() || http.available()) && ((millis() - timeoutStart) < kNetworkTimeout))
                    {
                        if (http.available())
                        {
                            c = http.read();
                            // Print out this character
                            Serial.print(c);

                            bodyLen--;
                            // We read something, reset the timeout counter
                            timeoutStart = millis();
                        }
                        else
                        {
                            // We haven't got any data, so let's pause to allow some to arrive
                            delay(kNetworkDelay);
                        }
                    }
                }
                else
                {
                    Serial.print("Failed to skip response headers: ");
                    Serial.println(err);
                }
            }
            else
            {
                Serial.print("Getting response failed: ");
                Serial.println(err);
            }
        }
        else
        {
            Serial.print("Connect failed: ");
            Serial.println(err);
        }
        http.stop();

        record_counter++;
    }
    delay(100);
}

void printWifiStatus()
{
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}
