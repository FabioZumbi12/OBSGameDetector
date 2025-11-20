#ifndef TWITCHCHATBOT_H
#define TWITCHCHATBOT_H

#include <QObject>
#include <QString>
#include <string>

class TwitchChatBot : public QObject {
	Q_OBJECT

private:
	explicit TwitchChatBot(QObject *parent = nullptr);

	std::string getUserId(const std::string &token, const std::string &clientId);
	std::string getGameId(const std::string &gameName, const std::string &token, const std::string &clientId);
	void setChannelCategory(const std::string &broadcasterId, const std::string &gameId,
				       const std::string &token, const std::string &clientId, const std::string &gameName);

public:
	static TwitchChatBot &get();

	// Envia a mensagem via API Helix
	void sendMessage(const QString &message);
	void updateCategory(const QString &gameName);

signals:
	void categoryUpdateFinished(bool success, const QString &gameName);
};

#endif // TWITCHCHATBOT_H