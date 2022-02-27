#include "HttpClient.h"
#include "b64.h"
#ifdef PROXY_ENABLED // currently disabled as introduces dependency on Dns.h in Ethernet
#include <Dns.h>
#endif


// Initialize constants
const char *HttpClient::kUserAgent = "Ameba";
const char *HttpClient::kContentLengthPrefix = HTTP_HEADER_CONTENT_LENGTH ": ";

#ifdef PROXY_ENABLED // currently disabled as introduces dependency on Dns.h in Ethernet
HttpClient::HttpClient(Client &aClient, const char *aProxy, uint16_t aProxyPort)
    : iClient(&aClient), iProxyPort(aProxyPort)
{
    resetState();
    if (aProxy)
    {
        // Resolve the IP address for the proxy
        DNSClient dns;
        dns.begin(Ethernet.dnsServerIP());
        // Not ideal that we discard any errors here, but not a lot we can do in the ctor
        // and we'll get a connect error later anyway
        (void)dns.getHostByName(aProxy, iProxyAddress);
    }
}
#else
HttpClient::HttpClient(Client &aClient)
    : iClient(&aClient), iProxyPort(0)
{
    resetState();
}
#endif

void HttpClient::resetState()
{
    iState = eIdle;
    iStatusCode = 0;
    iContentLength = 0;
    iBodyLengthConsumed = 0;
    iContentLengthPtr = kContentLengthPrefix;
    iHttpResponseTimeout = kHttpResponseTimeout;
}

void HttpClient::stop()
{
    iClient->stop();
    resetState();
}

void HttpClient::beginRequest()
{
    iState = eRequestStarted;
}

int HttpClient::startRequest(const char *aServerName, uint16_t aServerPort, const char *aURLPath, const char *aHttpMethod, const char *aUserAgent)
{
    tHttpState initialState = iState;
    if ((eIdle != iState) && (eRequestStarted != iState))
    {
        return HTTP_ERROR_API;
    }

#ifdef PROXY_ENABLED
    if (iProxyPort)
    {
        if ((!(iClient->connect(iProxyAddress, iProxyPort))) > 0)
        {
            return HTTP_ERROR_CONNECTION_FAILED;
        }
    }
    else
#endif
    {
        if ((!(iClient->connect(aServerName, aServerPort))) > 0)
        {
            return HTTP_ERROR_CONNECTION_FAILED;
        }
    }

    int ret = sendInitialHeaders(aServerName, IPAddress(0, 0, 0, 0), aServerPort, aURLPath, aHttpMethod, aUserAgent);
    if ((initialState == eIdle) && (HTTP_SUCCESS == ret))
    {
        finishHeaders();
    }

    return ret;
}

int HttpClient::startRequest(const IPAddress &aServerAddress, const char *aServerName, uint16_t aServerPort, const char *aURLPath, const char *aHttpMethod, const char *aUserAgent)
{
    tHttpState initialState = iState;
    if ((eIdle != iState) && (eRequestStarted != iState))
    {
        return HTTP_ERROR_API;
    }

#ifdef PROXY_ENABLED
    if (iProxyPort)
    {
        if ((!(iClient->connect(iProxyAddress, iProxyPort))) > 0)
        {
            return HTTP_ERROR_CONNECTION_FAILED;
        }
    }
    else
#endif
    {
        if ((!(iClient->connect(aServerAddress, aServerPort))) > 0)
        {
            return HTTP_ERROR_CONNECTION_FAILED;
        }
    }

    int ret = sendInitialHeaders(aServerName, aServerAddress, aServerPort, aURLPath, aHttpMethod, aUserAgent);
    if ((initialState == eIdle) && (HTTP_SUCCESS == ret))
    {
        finishHeaders();
    }

    return ret;
}

int HttpClient::sendInitialHeaders(const char *aServerName, IPAddress aServerIP, uint16_t aPort, const char *aURLPath, const char *aHttpMethod, const char *aUserAgent)
{
    aServerIP = aServerIP;
    // Send the HTTP command, i.e. "GET /somepath/ HTTP/1.0"
    iClient->print(aHttpMethod);
    iClient->print(" ");
#ifdef PROXY_ENABLED
    if (iProxyPort)
    {
        iClient->print("http://");
        if (aServerName)
        {
            iClient->print(aServerName);
        }
        else
        {
            iClient->print(aServerIP);
        }
        if (aPort != kHttpPort)
        {
            iClient->print(":");
            iClient->print(aPort);
        }
    }
#endif
    iClient->print(aURLPath);
    iClient->println(" HTTP/1.1");
    if (aServerName)
    {
        iClient->print("Host: ");
        iClient->print(aServerName);
        if (aPort != kHttpPort)
        {
            iClient->print(":");
            iClient->print(aPort);
        }
        iClient->println();
    }
    // And user-agent string
    if (aUserAgent)
    {
        sendHeader(HTTP_HEADER_USER_AGENT, aUserAgent);
    }
    else
    {
        sendHeader(HTTP_HEADER_USER_AGENT, kUserAgent);
    }
    // We don't support persistent connections, so tell the server to
    // close this connection after we're done
    sendHeader(HTTP_HEADER_CONNECTION, "close");

    iState = eRequestStarted;
    return HTTP_SUCCESS;
}

void HttpClient::sendHeader(const char *aHeader)
{
    iClient->println(aHeader);
}

void HttpClient::sendHeader(const char *aHeaderName, const char *aHeaderValue)
{
    iClient->print(aHeaderName);
    iClient->print(": ");
    iClient->println(aHeaderValue);
}

void HttpClient::sendHeader(const char *aHeaderName, const int aHeaderValue)
{
    iClient->print(aHeaderName);
    iClient->print(": ");
    iClient->println(aHeaderValue);
}

void HttpClient::sendBasicAuth(const char *aUser, const char *aPassword)
{
    iClient->print("Authorization: Basic ");

    unsigned char input[3];
    unsigned char output[5]; // Leave space for a '\0' terminator so we can easily print
    int userLen = strlen(aUser);
    int passwordLen = strlen(aPassword);
    int inputOffset = 0;
    for (int i = 0; i < (userLen + 1 + passwordLen); i++)
    {
        if (i < userLen)
        {
            input[inputOffset++] = aUser[i];
        }
        else if (i == userLen)
        {
            input[inputOffset++] = ':';
        }
        else
        {
            input[inputOffset++] = aPassword[(i - (userLen + 1))];
        }

        if ((inputOffset == 3) || (i == (userLen + passwordLen)))
        {
            b64_encode(input, inputOffset, output, 4);
            output[4] = '\0';
            iClient->print((char *)output);
            inputOffset = 0;
        }
    }
    iClient->println();
}

void HttpClient::finishHeaders()
{
    iClient->println();
    iState = eRequestSent;
}

void HttpClient::endRequest()
{
    if (iState < eRequestSent)
    {
        finishHeaders();
    }
}

int HttpClient::responseStatusCode()
{
    if (iState < eRequestSent)
    {
        return HTTP_ERROR_API;
    }

    char c = '\0';
    do
    {
        iStatusCode = 0;
        iState = eRequestSent;

        unsigned long timeoutStart = millis();
        const char *statusPrefix = "HTTP/*.* ";
        const char *statusPtr = statusPrefix;
        while ((c != '\n') && ((millis() - timeoutStart) < iHttpResponseTimeout))
        {
            if (available())
            {
                c = read();
                // if (c != -1) {
                switch (iState)
                {
                case eRequestSent:
                    if ((*statusPtr == '*') || (*statusPtr == c))
                    {
                        statusPtr++;
                        if (*statusPtr == '\0')
                        {
                            iState = eReadingStatusCode;
                        }
                    }
                    else
                    {
                        return HTTP_ERROR_INVALID_RESPONSE;
                    }
                    break;
                case eReadingStatusCode:
                    if (isdigit(c))
                    {
                        iStatusCode = iStatusCode * 10 + (c - '0');
                    }
                    else
                    {
                        iState = eStatusCodeRead;
                    }
                    break;
                case eStatusCodeRead:
                    break;
                default:
                    break;
                };
                timeoutStart = millis();
                //}
            }
            else
            {
                delay(kHttpWaitForDataDelay);
            }
        }

        if ((c == '\n') && (iStatusCode < 200))
        {
            c = '\0'; // Clear c so we'll go back into the data reading loop
        }
    } while ((iState == eStatusCodeRead) && (iStatusCode < 200));

    if ((c == '\n') && (iState == eStatusCodeRead))
    {
        return iStatusCode;
    }
    else if (c != '\n')
    {
        return HTTP_ERROR_TIMED_OUT;
    }
    else
    {
        return HTTP_ERROR_INVALID_RESPONSE;
    }
}

int HttpClient::skipResponseHeaders()
{
    unsigned long timeoutStart = millis();
    while ((!endOfHeadersReached()) && ((millis() - timeoutStart) < iHttpResponseTimeout))
    {
        if (available())
        {
            (void)readHeader();
            timeoutStart = millis();
        }
        else
        {
            delay(kHttpWaitForDataDelay);
        }
    }

    if (endOfHeadersReached())
    {
        return HTTP_SUCCESS;
    }
    else
    {
        return HTTP_ERROR_TIMED_OUT;
    }
}

bool HttpClient::endOfBodyReached()
{
    if (endOfHeadersReached() && (contentLength() != kNoContentLengthHeader))
    {
        return (iBodyLengthConsumed >= contentLength());
    }
    return false;
}

int HttpClient::read()
{
    int ret = iClient->read();
    if (ret >= 0)
    {
        if (endOfHeadersReached() && (iContentLength > 0))
        {
            iBodyLengthConsumed++;
        }
    }
    return ret;
}

int HttpClient::read(uint8_t *buf, size_t size)
{
    int ret = iClient->read(buf, size);
    if (endOfHeadersReached() && iContentLength > 0)
    {
        if (ret >= 0)
        {
            iBodyLengthConsumed += ret;
        }
    }
    return ret;
}

int HttpClient::readHeader()
{
    char c = read();

    if (endOfHeadersReached())
    {
        return c;
    }

    switch (iState)
    {
    case eStatusCodeRead:
        if (*iContentLengthPtr == c)
        {
            iContentLengthPtr++;
            if (*iContentLengthPtr == '\0')
            {
                iState = eReadingContentLength;
                iContentLength = 0;
            }
        }
        else if ((iContentLengthPtr == kContentLengthPrefix) && (c == '\r'))
        {
            iState = eLineStartingCRFound;
        }
        else
        {
            iState = eSkipToEndOfHeader;
        }
        break;
    case eReadingContentLength:
        if (isdigit(c))
        {
            iContentLength = iContentLength * 10 + (c - '0');
        }
        else
        {
            iState = eSkipToEndOfHeader;
        }
        break;
    case eLineStartingCRFound:
        if (c == '\n')
        {
            iState = eReadingBody;
        }
        break;
    default:
        break;
    };

    if ((c == '\n') && (!endOfHeadersReached()))
    {
        iState = eStatusCodeRead;
        iContentLengthPtr = kContentLengthPrefix;
    }
    return c;
}

int HttpClient::mystartRequest(const char *aServerName, uint16_t aServerPort, const char *aURLPath, const char *aHttpMethod, const char *aUserAgent)
{
    tHttpState initialState = iState;
    if ((eIdle != iState) && (eRequestStarted != iState))
    {
        return HTTP_ERROR_API;
    }

#ifdef PROXY_ENABLED
    if (iProxyPort)
    {
        if ((!(iClient->connect(iProxyAddress, iProxyPort))) > 0)
        {
            return HTTP_ERROR_CONNECTION_FAILED;
        }
    }
    else
#endif
    {
        if ((!(iClient->connect(aServerName, aServerPort))) > 0)
        {
            return HTTP_ERROR_CONNECTION_FAILED;
        }
    }

    int ret = mysendInitialHeaders(aServerName, IPAddress(0, 0, 0, 0), aServerPort, aURLPath, aHttpMethod, aUserAgent);
    if ((initialState == eIdle) && (HTTP_SUCCESS == ret))
    {
        finishHeaders();
    }

    return ret;
}

int HttpClient::mysendInitialHeaders(const char *aServerName, IPAddress aServerIP, uint16_t aPort, const char *aURLPath, const char *aHttpMethod, const char *aUserAgent)
{
    aServerIP = aServerIP;
    // Send the HTTP command, i.e. "GET /somepath/ HTTP/1.0"
    iClient->print(aHttpMethod);
    iClient->print(" ");
#ifdef PROXY_ENABLED
    if (iProxyPort)
    {
        iClient->print("http://");
        if (aServerName)
        {
            iClient->print(aServerName);
        }
        else
        {
            iClient->print(aServerIP);
        }
        if (aPort != kHttpPort)
        {
            iClient->print(":");
            iClient->print(aPort);
        }
    }
#endif
    iClient->print(aURLPath);
    iClient->println(" HTTP/1.1");
    if (aServerName)
    {
        iClient->print("Host: ");
        iClient->print(aServerName);
        if (aPort != kHttpPort)
        {
            iClient->print(":");
            iClient->print(aPort);
        }
        iClient->println();
    }
    // And user-agent string
    if (aUserAgent)
    {
        sendHeader(HTTP_HEADER_USER_AGENT, aUserAgent);
    }
    else
    {
        sendHeader(HTTP_HEADER_USER_AGENT, kUserAgent);
    }
    // for persistent connections
    sendHeader(HTTP_HEADER_CONNECTION, "keep-alive");
    sendHeader(HTTP_HEADER_CONTENT_LENGTH, post_len);
    Serial.println(post_len);
    sendHeader(HTTP_CONTENT_TYPE, content_type);
    Serial.println(content_type);

    iState = eRequestStarted;
    return HTTP_SUCCESS;
}
int HttpClient::mysetPostData(char *post_start_d, char *post_end_d, uint8_t *post_content_d, int post_start_len_d,
                              int post_end_len_d, int post_content_len_d, char *content_type_d)
{
    post_start = post_start_d;
    post_end = post_end_d;
    post_content = post_content_d;
    post_start_len = post_start_len_d;
    post_end_len = post_end_len_d;
    post_content_len = post_content_len_d;
    post_len = post_start_len_d + post_content_len_d + post_end_len_d;
    content_type = content_type_d;
    Serial.println(post_len);
    printf("%d", post_len);
}

int HttpClient::mysetPostContent(uint8_t *post_content_d, int post_content_len_d, int post_len_d)
{
    post_content = post_content_d;
    post_content_len = post_content_len_d;
    post_len = post_len_d;
}

int HttpClient::mypost(const char *aServerName, const char *aURLPath, SdFatFile file, const char *aUserAgent)
{
    const int MY_BODY_SIZE = 1000;
    uint8_t buf_temp[MY_BODY_SIZE]; // 读取音频并发送的缓冲
    memset(buf_temp, 0, MY_BODY_SIZE);

    // 发送请求头
    int req_ret = mystartRequest(aServerName, kHttpPort, aURLPath, HTTP_METHOD_POST, aUserAgent);
    if (HTTP_SUCCESS != req_ret)
    {
        return req_ret;
    }

    // 发送请求体头部
    iClient->write((const uint8_t *)post_start, post_start_len);

    // 发送音频文件，可能分多次发送
    int read_bytes = file.read(buf_temp, MY_BODY_SIZE);
    iClient->write(buf_temp, read_bytes);
    while (read_bytes == MY_BODY_SIZE)
    {
        read_bytes = file.read(buf_temp, MY_BODY_SIZE);
        iClient->write(buf_temp, read_bytes);
    }

    // for general usage
    // int content_idx=0;
    // if (post_content_len > MY_BODY_SIZE && read_bytes == MY_BODY_SIZE)
    // {
    //     for (; content_idx < post_content_len - MY_BODY_SIZE; content_idx += MY_BODY_SIZE)
    //     {
    //         iClient->write(post_content + content_idx, MY_BODY_SIZE);
    //     }
    // }
    // iClient->write((const uint8_t *)(post_content + content_idx), post_content_len - content_idx);

    // 发送请求体尾部
    iClient->write((const uint8_t *)post_end, post_end_len);

    return HTTP_SUCCESS;
}
