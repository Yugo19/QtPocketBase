#include "PocketBaseClient.h"
#include "QtQml/qjsvalueiterator.h"

PocketBaseClient::PocketBaseClient(QObject *parent)
    : QObject{parent}
{
    subscriber = new CollectionSubscriber(this);

    QObject::connect(subscriber, &CollectionSubscriber::connetionEstablished, this, [this](){
        setConnected(true);
    });

    QObject::connect(subscriber, &CollectionSubscriber::connectedChanged, this, [this](){
        setConnected(subscriber->connected());
    });

    QTimer *timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, [this](){
        isHealthy();
        if (subscriber->connected() == false) {
            connect();
        }
    });

    timer->start(500);
    isHealthy();
}

void PocketBaseClient::addCollection(PocketBaseCollection *collection)
{
    request.setRoute("/collections");

    m_collectionList->append(collection);
}

PocketBaseCollectionPromise *PocketBaseClient::updateCollection(QString collectionId, PocketBaseCollection *updateCollection)
{
    request.setRoute("/collections");
    return request.update(collectionId, updateCollection->toJson());
}

PocketBaseCollectionPromise *PocketBaseClient::createCollection(PocketBaseCollection *newCollection)
{
    request.setRoute("/collections");
    return request.create(newCollection->toJson());
}

PocketBaseCollectionPromise *PocketBaseClient::deleteCollection(QString collectionId)
{
    request.setRoute("/collections");
    return request.deleteOne(collectionId);
}

PocketBaseCollectionPromise *PocketBaseClient::getCollection(QString collectionId)
{
    request.setRoute("/collections");
    return new PocketBaseCollectionPromise();
}

PocketBaseCollectionPromise *PocketBaseClient::getCollections()
{
    request.setRoute("/collections");
    return request.getList();
}

PocketBaseCollectionPromise *PocketBaseClient::importCollection(QJSValue json)
{
    return request.HttpPut("/collections/import", json.toVariant().toJsonObject());
}

PocketBaseCollectionPromise *PocketBaseClient::update(QString collectionName, QString id, QJSValue data, QJSValue options)
{
    QJsonObject allOptions;
    QJSValueIterator it(options);
    while (it.hasNext()) {
        it.next();
        allOptions.insert(it.name(), it.value().toString());
    }

    request.setRoute("/collections/" + collectionName + "/records");
    return request.update(id, data.toVariant().toJsonObject(), allOptions);
}

PocketBaseCollectionPromise *PocketBaseClient::isHealthy()
{
    if (m_apiUrl.isEmpty()) {
        setHealthy(false);
        return nullptr;
    }
    auto promise = request.HttpGet("/api/health");
    QObject::connect(promise, &PocketBaseCollectionPromise::onThen, this, [this](auto value){
        setHealthy(true);
    });
    QObject::connect(promise, &PocketBaseCollectionPromise::onError, this, [this](auto value){
        setHealthy(false);
    });
    return promise;
}

PocketBaseCollection *PocketBaseClient::collection(QString collectionName)
{
    if (m_collectionList == nullptr || m_collectionList->isEmpty()) goto newCollection;

    for (auto c : *m_collectionList) {
        try {
            if (c && c->name() == collectionName) {
                return c;
            }
        } catch (std::exception e) {
            qWarning() << e.what();
            delete c;
        }
    }

    newCollection:
    PocketBaseCollection *newCollection = new PocketBaseCollection();
    newCollection->setName(collectionName);
    addCollection(newCollection);
    return newCollection;
}

PocketBaseCollectionPromise *PocketBaseClient::authAdminWithPassword(QString identity, QString password)
{
    request.setRoute("/admins/auth-with-password");
    PocketBaseCollectionPromise *promise = request.authWithPassword(identity, password);
    QObject::connect(promise, &PocketBaseCollectionPromise::onThen, [=](QJSValueList response) {
        QJsonObject data = QJsonDocument::fromJson(QByteArray::fromStdString(response.toList().at(0).toString().toStdString())).object();
        setAuthToken(data.value("token").toString());
    });
    return promise;
}

PocketBaseCollectionPromise *PocketBaseClient::authWithCollection(QString collectionName, QString identity, QString password)
{
    request.setRoute("/collections/" + collectionName + "/auth-with-password");
    PocketBaseCollectionPromise *promise = request.authWithPassword(identity, password);
    QObject::connect(promise, &PocketBaseCollectionPromise::onThen, [=](QJSValueList response) {
        QJsonObject data = QJsonDocument::fromJson(QByteArray::fromStdString(response.toList().at(0).toString().toStdString())).object();
        setAuthToken(data.value("token").toString());
    });
    return promise;
}

PocketBaseCollectionPromise *PocketBaseClient::authRefreshCollection(QString collectionName)
{
    request.setRoute("/collections/" + collectionName + "/auth-refresh");
    PocketBaseCollectionPromise *promise = request.authRefresh();
    QObject::connect(promise, &PocketBaseCollectionPromise::onThen, [=](QJSValueList response) {
        QJsonObject data = QJsonDocument::fromJson(QByteArray::fromStdString(response.toList().at(0).toString().toStdString())).object();
        setAuthToken(data.value("token").toString());
    });
    return promise;
}

PocketBaseCollectionPromise *PocketBaseClient::createNewAdmin(QString email, QString identity, QString password)
{
    request.setRoute("/admins");
    QJsonObject admin;
    admin.insert("email", email);
    admin.insert("identity", identity);
    admin.insert("password", password);
    admin.insert("passwordConfirm", password);

    return request.create(admin);
}

void PocketBaseClient::logout()
{
    setAuthToken("");
}

void PocketBaseClient::subscribe(const QString pattern, const QJSValue callback, QString id)
{
    if (subscriber->connected() == false) {
        QTimer *timer = new QTimer(this);
        QObject::connect(timer, &QTimer::timeout, this, [=](){
            if (subscriber->connected() == true) {
                subscriber->subscribe(pattern, callback, id);
                timer->stop();
            }
        });
        timer->start(5000);
    } else
        subscriber->subscribe(pattern, callback, id);
}

void PocketBaseClient::unsubscribe(const QString id)
{
    subscriber->unsubscribe(id);
}

void PocketBaseClient::connect()
{
    subscriber->connect();
}


QString PocketBaseClient::apiUrl() const
{
    return m_apiUrl;
}

void PocketBaseClient::setApiUrl(const QString &newApiUrl)
{
    if (m_apiUrl == newApiUrl)
        return;
    m_apiUrl = newApiUrl;

    PocketBaseSettings::setApiUrl(newApiUrl);

    emit apiUrlChanged();
}

void PocketBaseClient::setCollectionList(QList<PocketBaseCollection *> *newCollectionList)
{
    if (m_collectionList == newCollectionList)
        return;
    m_collectionList = newCollectionList;
    emit collectionListChanged();
}

QList<PocketBaseCollection *> *PocketBaseClient::collectionList() const
{
    return m_collectionList;
}

QString PocketBaseClient::authToken() const
{
    return m_authToken;
}

void PocketBaseClient::setAuthToken(const QString &newAuthToken)
{
    if (m_authToken == newAuthToken)
        return;
    m_authToken = newAuthToken;

    PocketBaseSettings::setToken(newAuthToken);

    emit authTokenChanged();
}

bool PocketBaseClient::connected() const
{
    return m_connected;
}

void PocketBaseClient::setConnected(bool newConnected)
{
    if (m_connected == newConnected)
        return;
    m_connected = newConnected;
    emit connectedChanged();
}

bool PocketBaseClient::healthy() const
{
    return m_healthy;
}

void PocketBaseClient::setHealthy(bool newHealthy)
{
    if (m_healthy == newHealthy)
        return;
    m_healthy = newHealthy;
    emit healthyChanged();
}

bool PocketBaseClient::isAuth() const
{
    return !m_authToken.isEmpty();
}


