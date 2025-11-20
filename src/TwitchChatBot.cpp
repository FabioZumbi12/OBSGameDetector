#include "TwitchChatBot.h"
#include "ConfigManager.h"
#include <obs-module.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <curl/curl.h>
#include <string>
#include <memory>

TwitchChatBot &TwitchChatBot::get()
{
	static TwitchChatBot instance;
	return instance;
}

TwitchChatBot::TwitchChatBot(QObject *parent) : QObject(parent)
{
	curl_global_init(CURL_GLOBAL_ALL);
}

static size_t twitch_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string *)userp)->append((char *)contents, size * nmemb);
	return size * nmemb;
}

std::string TwitchChatBot::getUserId(const std::string &token, const std::string &clientId)
{
	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao inicializar cURL para getUserId.");
		return "";
	}

	std::string readBuffer;
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
	headers = curl_slist_append(headers, ("Client-Id: " + clientId).c_str());

	curl_easy_setopt(curl, CURLOPT_URL, "https://api.twitch.tv/helix/users");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitch_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK || http_code != 200) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao obter User ID. cURL error: %s, HTTP code: %ld, Response: %s",
		     curl_easy_strerror(res), http_code, readBuffer.c_str());
		return "";
	}

	QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(readBuffer).toUtf8());
	if (doc.isObject()) {
		QJsonObject obj = doc.object();
		if (obj.contains("data") && obj["data"].isArray()) {
			QJsonArray dataArray = obj["data"].toArray();
			if (!dataArray.isEmpty()) {
				QJsonObject userObject = dataArray.first().toObject();
				if (userObject.contains("id") && userObject["id"].isString()) {
					return userObject["id"].toString().toStdString();
				}
			}
		}
	}

	blog(LOG_ERROR, "[GameDetector/Twitch] Resposta da API de usuários não continha o ID esperado.");
	return "";
}

std::string TwitchChatBot::getGameId(const std::string &gameName, const std::string &token, const std::string &clientId)
{
	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao inicializar cURL para getGameId.");
		return "";
	}

	char *escaped_name = curl_easy_escape(curl, gameName.c_str(), static_cast<int>(gameName.length()));
	if (!escaped_name) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao escapar nome do jogo.");
		curl_easy_cleanup(curl);
		return "";
	}
	std::string url = "https://api.twitch.tv/helix/games?name=" + std::string(escaped_name);
	curl_free(escaped_name);

	std::string readBuffer;
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
	headers = curl_slist_append(headers, ("Client-Id: " + clientId).c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitch_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK || http_code != 200) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao obter Game ID para '%s'. cURL error: %s, HTTP code: %ld, Response: %s",
		     gameName.c_str(), curl_easy_strerror(res), http_code, readBuffer.c_str());
		return "";
	}

	QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(readBuffer).toUtf8());
	if (doc.isObject()) {
		QJsonObject obj = doc.object();
		if (obj.contains("data") && obj["data"].isArray()) {
			QJsonArray dataArray = obj["data"].toArray();
			if (!dataArray.isEmpty()) {
				// A API pode retornar múltiplos jogos se o nome for ambíguo.
				// Vamos assumir que o primeiro é o correto.
				QJsonObject gameObject = dataArray.first().toObject();
				if (gameObject.contains("id") && gameObject["id"].isString()) {
					return gameObject["id"].toString().toStdString();
				}
			}
		}
	}

	blog(LOG_WARNING, "[GameDetector/Twitch] Jogo '%s' não encontrado na API da Twitch.", gameName.c_str());
	return "";
}

void TwitchChatBot::setChannelCategory(const std::string &broadcasterId, const std::string &gameId,
				       const std::string &token, const std::string &clientId, const std::string &gameName)
{
	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao inicializar cURL para setChannelCategory.");
		return;
	}

	std::string url = "https://api.twitch.tv/helix/channels?broadcaster_id=" + broadcasterId;
	std::string postData = "{\"game_id\":\"" + gameId + "\"}";

	std::string readBuffer;
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
	headers = curl_slist_append(headers, ("Client-Id: " + clientId).c_str());
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitch_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res == CURLE_OK && (http_code >= 200 && http_code < 300)) {
		blog(LOG_INFO, "[GameDetector/Twitch] Categoria do canal alterada com sucesso para '%s'.", gameName.c_str());
		emit categoryUpdateFinished(true, QString::fromStdString(gameName));
	} else {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao alterar categoria. cURL error: %s, HTTP code: %ld, Response: %s",
		     curl_easy_strerror(res), http_code, readBuffer.c_str());
		emit categoryUpdateFinished(false, QString::fromStdString(gameName));
	}
}

void TwitchChatBot::sendMessage(const QString &message)
{
	QString token = ConfigManager::get().getToken();
	QString clientId = ConfigManager::get().getClientId();
	std::string tokenStr = token.toStdString();
	std::string clientIdStr = clientId.toStdString();

	if (tokenStr.empty() || clientIdStr.empty()) {
		blog(LOG_WARNING, "[GameDetector/Twitch] Token ou Client ID não configurado.");
		return;
	}

	// Obter o ID do usuário (broadcaster/sender)
	std::string userIdStr = getUserId(tokenStr, clientIdStr);
	if (userIdStr.empty()) {
		blog(LOG_ERROR,
		     "[GameDetector/Twitch] Não foi possível obter o ID do usuário. Verifique o token e o Client ID.");
		return;
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao inicializar cURL para sendMessage.");
		return;
	}

	QJsonObject body;
	body["broadcaster_id"] = QString::fromStdString(userIdStr);
	body["sender_id"] = QString::fromStdString(userIdStr);
	body["message"] = message;

	QJsonDocument doc(body);
	std::string postData = doc.toJson(QJsonDocument::Compact).toStdString();

	std::string readBuffer;
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, ("Authorization: Bearer " + tokenStr).c_str());
	headers = curl_slist_append(headers, ("Client-Id: " + clientIdStr).c_str());
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, "https://api.twitch.tv/helix/chat/messages");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitch_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res == CURLE_OK && (http_code >= 200 && http_code < 300)) {
		// blog(LOG_INFO, "[GameDetector/Twitch] Mensagem enviada com sucesso. Resposta: %s", readBuffer.c_str());
	} else {
		blog(LOG_ERROR, "[GameDetector/Twitch] Falha ao enviar mensagem. cURL error: %s, HTTP code: %ld, Response: %s",
		     curl_easy_strerror(res), http_code, readBuffer.c_str());
	}
}

void TwitchChatBot::updateCategory(const QString &gameName)
{
	QString token = ConfigManager::get().getToken();
	QString clientId = ConfigManager::get().getClientId();
	std::string tokenStr = token.toStdString();
	std::string clientIdStr = clientId.toStdString();

	if (tokenStr.empty() || clientIdStr.empty()) {
		blog(LOG_WARNING, "[GameDetector/Twitch] Token ou Client ID não configurado para alterar categoria.");
		return;
	}

	// 1. Obter o ID do usuário (broadcaster)
	std::string userIdStr = getUserId(tokenStr, clientIdStr);
	if (userIdStr.empty()) {
		blog(LOG_ERROR, "[GameDetector/Twitch] Não foi possível obter o ID do usuário para alterar categoria.");
		return;
	}

	// 2. Obter o ID do jogo
	std::string gameId = getGameId(gameName.toStdString(), tokenStr, clientIdStr);
	if (gameId.empty()) {
		// Se o jogo não for encontrado, podemos opcionalmente tentar definir como "Just Chatting" ou outra categoria padrão.
		// ID de "Just Chatting" é "509658"
		std::string fallbackGame = "Just Chatting";
		blog(LOG_INFO, "[GameDetector/Twitch] Jogo '%s' não encontrado. Tentando definir como '%s'.", gameName.toUtf8().constData(), fallbackGame.c_str());
		gameId = getGameId(fallbackGame, tokenStr, clientIdStr);
		if (gameId.empty()) return; // Falha ao obter ID do fallback
	}

	// 3. Atualizar o canal
	setChannelCategory(userIdStr, gameId, tokenStr, clientIdStr, gameName.toStdString());
}