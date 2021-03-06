#include "include/imgserver/restclient.h"
#include <iostream>
#include <QByteArray>
#include <QFile>

const QString RestClient::ORG_NAME = "Image Code";
const QString RestClient::APP_NAME = "RestClient";

const QString RestClient::KEY_REST_URL = "rest/url";
const QString RestClient::KEY_SERVER_CERT_PATH = "server/certificate";
const QString RestClient::KEY_CLIENT_LOGIN = "client/login";
const QString RestClient::KEY_CLIENT_PASSWORD = "client/password";
const QString RestClient::KEY_CLIENT_REALM = "client/realm";
const QString RestClient::KEY_CLIENT_VERIFY_PEER = "client/ssl/verify_peer";
const QString RestClient::DEFAULT_REST_URL = "http://localhost:8080/imgserver/rest";


RestClient::RestClient()
{
    manager = new QNetworkAccessManager(this);
    settings = new QSettings(RestClient::ORG_NAME, RestClient::APP_NAME);
    sslConfig = prepareSslConfig();
    connect(manager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), this, SLOT(setCredentials(QNetworkReply*, QAuthenticator*)));
}

RestClient::~RestClient()
{
    delete manager;
    delete settings;
}

/**
 * @brief RestClient::readReply Debug method to dump network reply to std out. Prints reply headers and reply body.
 * @param reply Reply to be dumped on std out.
 */
void RestClient::readReply(QNetworkReply *reply)
{
    std::cout << "REPLY headers:" << std::endl;
    QList<QPair<QByteArray, QByteArray>> headers = reply->rawHeaderPairs();
    for (int i = 0; i < headers.size(); i++)
    {
        std::cout << headers.at(i).first.data() << "\t" << headers.at(i).second.data() << std::endl;
    }

    std::cout << "REPLY body:" << std::endl;
    std::cout << reply->readAll().data() << std::endl;
}

/**
 * @brief RestClient::createClient Creates new client recoed on remote server (in the databse).
 * @param versionMajor Major version of the client.
 * @param versionMinor Minor version of the client.
 */
void RestClient::createClient(int versionMajor, int versionMinor)
{
    QByteArray xmlReq;
    QXmlStreamWriter stream(&xmlReq);
    stream.writeStartDocument();
    stream.writeStartElement("clientVersion");
    stream.writeAttribute("major", std::to_string(versionMajor).c_str());
    stream.writeAttribute("minor", std::to_string(versionMinor).c_str());
    stream.writeEndElement();
    stream.writeEndDocument();

    QNetworkRequest req = QNetworkRequest(QUrl(settings->value(RestClient::KEY_REST_URL, RestClient::DEFAULT_REST_URL).toString() + "/client"));
    req.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/xml");
    manager->post(req, xmlReq);
}

/**
 * @brief RestClient::uploadImage Uploads local image to the remote server
 * @param imageName Name of the image on the remote server
 * @param imagePath Local path to the image
 */
void RestClient::uploadImage(QString imageName, QString imagePath)
{
    prepareImageUpload(imageName, imagePath);
}

/**
 * @brief RestClient::prepareImageUpload Sends initial POST resquest to the remote server, asking for image upload. Request conatins image name and it's SHA256 hash.
 * @param imageName Name of the image on the remote server
 * @param imagePath Local path to the image
 */
void RestClient::prepareImageUpload(QString imageName, QString imagePath)
{
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) return; //TODO throw error
    imageContent = file.readAll();
    QByteArray imageSha256 = QCryptographicHash::hash(imageContent, QCryptographicHash::Sha256);
    qDebug() << "SHA256: " << imageSha256.toHex();

    QByteArray xmlReq;
    QXmlStreamWriter stream(&xmlReq);
    stream.writeStartDocument();
    stream.writeStartElement("image");
    stream.writeAttribute("name", imageName);
    stream.writeAttribute("sha256", imageSha256.toHex());
    stream.writeEndElement();
    stream.writeEndDocument();

    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(doImageUpload(QNetworkReply*)));
    QNetworkRequest req = QNetworkRequest(QUrl(settings->value(RestClient::KEY_REST_URL, RestClient::DEFAULT_REST_URL).toString() + "/image"));
    req.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/xml");
    req.setSslConfiguration(sslConfig);
    manager->post(req, xmlReq);
}

/**
 * @brief RestClient::doImageUpload Callback method which does actual image upload via another POST request, containg image itself.
 * Before doing that, parses previous reply heads to find out upload link.
 * @param reply Reply from previous request. It has to contain "Link" header with URL where image should be uploaded.
 */
void RestClient::doImageUpload(QNetworkReply *reply)
{
    //qDebug() << "ERROR: " << reply->errorString();
    QString uploadLink = parseLinkHeader(reply->rawHeaderPairs());
    qDebug() << "upload link: " << uploadLink;

    QNetworkRequest req = QNetworkRequest(QUrl(uploadLink));
    req.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "image/*;charset=UTF-8");
    manager->disconnect(); // disconnect form signal to avoid infinite loop - this method is slot for finished reply signal
    manager->post(req, imageContent);
}

/**
 * @brief RestClient::parseLinkHeader Parses "Link" header
 * @param headers HTTP headers from the reply.
 * @return Value of the "Link" header.
 */
QString RestClient::parseLinkHeader(QList<QPair<QByteArray, QByteArray>> headers)
{
    QString uploadLink = nullptr;
    for (QList<QPair<QByteArray, QByteArray>>::iterator iter = headers.begin(); iter != headers.end(); ++iter)
    {
        if (QString::compare("Link", iter->first.data(), Qt::CaseInsensitive) == 0) {
            QString headerLink = iter->second.data();
            int linkBegin = headerLink.indexOf("<");
            int linkEnd = headerLink.indexOf(">");
            if (linkBegin >= 0 && linkEnd > 0)
            {
                uploadLink = headerLink.mid(linkBegin + 1, linkEnd - linkBegin - 1);
            }
            break;
        }
    }
    //TODO if uploadLink == null throw error
    return uploadLink;
}

/**
 * @brief RestClient::prepareSslConfig Configures parameters for SSL/TLS connection.
 * @return QSslConfiguration instance with appropriate setup.
 */
QSslConfiguration RestClient::prepareSslConfig()
{
    QSslConfiguration sslConfig(QSslConfiguration::defaultConfiguration());
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    QFile certFile(settings->value(RestClient::KEY_SERVER_CERT_PATH).toString());
    certFile.open(QIODevice::ReadOnly);
    QList<QSslCertificate> caList = sslConfig.caCertificates();
    QSslCertificate serverCert(&certFile, QSsl::Pem);
    caList.append(serverCert);
    sslConfig.setCaCertificates(caList);

    sslConfig.setPeerVerifyMode(settings->value(RestClient::KEY_CLIENT_VERIFY_PEER).toBool() ? QSslSocket::VerifyPeer : QSslSocket::VerifyNone);
    return sslConfig;
}

/**
 * @brief RestClient::setCredentials Sets client credential to authenticate on remote sever.
 * @param reply Reply request for authentication.
 * @param auth QAuthentication instance with set up credentials.
 */
void RestClient::setCredentials(QNetworkReply *reply, QAuthenticator *auth)
{
    auth->setUser(settings->value(RestClient::KEY_CLIENT_LOGIN).toString());
    auth->setPassword(settings->value(RestClient::KEY_CLIENT_PASSWORD).toString());
    auth->setRealm(settings->value(RestClient::KEY_CLIENT_REALM).toString());
}
