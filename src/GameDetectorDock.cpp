#include "GameDetectorDock.h"
#include "GameDetector.h"
#include "TwitchChatBot.h"
#include <obs-data.h>
#include "ConfigManager.h"
#include <QComboBox>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QUrl>
#include <QDesktopServices>
#include <QTimer>
#include <obs.h>
#include <QHeaderView>
#include <QStyle>
#include <QCheckBox>

static QLabel *g_statusLabel = nullptr;

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(10, 10, 10, 10);
	mainLayout->setSpacing(10);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	g_statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	g_statusLabel->setWordWrap(true);
	mainLayout->addWidget(g_statusLabel);

	QFrame *separator1 = new QFrame();
	separator1->setFrameShape(QFrame::HLine);
	separator1->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator1);

	// Layout para a ação da Twitch
	QHBoxLayout *twitchActionLayout = new QHBoxLayout();
	twitchActionLabel = new QLabel(obs_module_text("Dock.TwitchAction"));
	twitchActionComboBox = new QComboBox();
	twitchActionComboBox->addItem(obs_module_text("Dock.TwitchAction.SendCommand"), 0);
	twitchActionComboBox->addItem(obs_module_text("Dock.TwitchAction.ChangeCategory"), 1);
	twitchActionLayout->addWidget(twitchActionLabel);
	twitchActionLayout->addWidget(twitchActionComboBox);
	mainLayout->addLayout(twitchActionLayout);

	connect(twitchActionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		onSettingsChanged();
		updateActionModeUI(index);
	});

	// Layout para o comando
	QHBoxLayout *commandLayout = new QHBoxLayout();
	commandLabel = new QLabel(obs_module_text("Dock.Command.GameDetected"));

	commandInput = new QLineEdit();
	commandInput->setPlaceholderText(obs_module_text("Dock.Command.GameDetected.Placeholder"));

	commandLayout->addWidget(commandLabel);
	commandLayout->addWidget(commandInput);
	mainLayout->addLayout(commandLayout);

	QHBoxLayout *noGameCommandLayout = new QHBoxLayout();
	noGameCommandLabel = new QLabel(obs_module_text("Dock.Command.NoGame"));

	noGameCommandInput = new QLineEdit();
	noGameCommandInput->setPlaceholderText(obs_module_text("Dock.Command.NoGame.Placeholder"));

	noGameCommandLayout->addWidget(noGameCommandLabel);
	noGameCommandLayout->addWidget(noGameCommandInput);
	mainLayout->addLayout(noGameCommandLayout);

	autoExecuteCheckbox = new QCheckBox(obs_module_text("Dock.AutoExecute"));
	mainLayout->addWidget(autoExecuteCheckbox);

	executeCommandButton = new QPushButton(obs_module_text("Dock.ExecuteCommand"));
	executeCommandButton->setEnabled(false);
	mainLayout->addWidget(executeCommandButton);
	connect(executeCommandButton, &QPushButton::clicked, this,
		&GameDetectorDock::onExecuteCommandClicked);

	// Conecta os sinais de mudança de texto ao nosso novo slot
	connect(commandInput, &QLineEdit::textChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(noGameCommandInput, &QLineEdit::textChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(autoExecuteCheckbox, &QCheckBox::checkStateChanged, this, &GameDetectorDock::onSettingsChanged);

	// Conecta os sinais do detector de jogos aos nossos novos slots
	connect(&GameDetector::get(), &GameDetector::gameDetected, this, &GameDetectorDock::onGameDetected);
	connect(&GameDetector::get(), &GameDetector::noGameDetected, this, &GameDetectorDock::onNoGameDetected);
	connect(&TwitchChatBot::get(), &TwitchChatBot::categoryUpdateFinished, this,
		&GameDetectorDock::onCategoryUpdateFinished);

	// Timer para salvar com delay
	saveDelayTimer = new QTimer(this);
	saveDelayTimer->setSingleShot(true);
	saveDelayTimer->setInterval(1000); // 1 segundo de delay
	connect(saveDelayTimer, &QTimer::timeout, this, &GameDetectorDock::saveDockSettings);

	// O botão de salvar foi removido, o salvamento agora é automático.

	mainLayout->addStretch(1);QLabel *developerLabel = new QLabel(
		"<small><a href=\"https://github.com/FabioZumbi12\" style=\"color: gray; text-decoration: none;\"><i>Developed by FabioZumbi12</i></a></small>");
	developerLabel->setOpenExternalLinks(true);
	mainLayout->addWidget(developerLabel);
	setLayout(mainLayout);
}

void GameDetectorDock::saveDockSettings()
{
	obs_data_t *settings = ConfigManager::get().getSettings();

	obs_data_set_string(settings, "twitch_command_message", commandInput->text().toStdString().c_str());
	obs_data_set_string(settings, "twitch_command_no_game", noGameCommandInput->text().toStdString().c_str());
	obs_data_set_bool(settings, "execute_automatically", autoExecuteCheckbox->isChecked());
	obs_data_set_int(settings, "twitch_action_mode", twitchActionComboBox->currentData().toInt());

	ConfigManager::get().save(settings);

	// Feedback visual no label de status
	QString originalStatus = g_statusLabel->text();
	g_statusLabel->setText(obs_module_text("Dock.SettingsSaved"));

	QTimer::singleShot(2000, this, [this]() {
		// Restaura o status original, verificando se não mudou nesse meio tempo
		if (g_statusLabel->text() == obs_module_text("Dock.SettingsSaved")) {
			g_statusLabel->setText(this->detectedGameName.isEmpty() ? obs_module_text("Status.Waiting") : QString(obs_module_text("Status.Playing")).arg(this->detectedGameName));
		}
	});
}

void GameDetectorDock::onSettingsChanged()
{
	saveDelayTimer->start(); // Reinicia o timer a cada alteração
}

void GameDetectorDock::onGameDetected(const QString &gameName, const QString &processName)
{
	this->detectedGameName = gameName;
	g_statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(gameName));

	executeCommandButton->setEnabled(true);
	if (autoExecuteCheckbox->isChecked()) {
		int actionMode = ConfigManager::get().getTwitchActionMode();
		if (actionMode == 0) { // Enviar comando
			executeGameCommand(gameName);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory(gameName);
		}
	}
}

void GameDetectorDock::onNoGameDetected()
{
	this->detectedGameName.clear();
	g_statusLabel->setText(obs_module_text("Status.Waiting"));
	executeCommandButton->setEnabled(false);

	// Executa o comando de "sem jogo"
	QString noGameCommand = noGameCommandInput->text();
	if (autoExecuteCheckbox->isChecked()) {
		int actionMode = ConfigManager::get().getTwitchActionMode();
		if (actionMode == 0) { // Enviar comando
			if (!noGameCommand.isEmpty())
			TwitchChatBot::get().sendMessage(noGameCommand);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory("Just Chatting");
		}
	} else {
		executeCommandButton->setEnabled(true);
	}
}

void GameDetectorDock::onExecuteCommandClicked()
{
	if (!detectedGameName.isEmpty()) {
		int actionMode = ConfigManager::get().getTwitchActionMode();
		if (actionMode == 0) { // Enviar comando
			executeGameCommand(detectedGameName);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory(detectedGameName);
		}
	} else {
		int actionMode = ConfigManager::get().getTwitchActionMode();
		if (actionMode == 1) { // Alterar categoria para "sem jogo"
			TwitchChatBot::get().updateCategory("Just Chatting");
		}
	}
}

void GameDetectorDock::loadSettingsFromConfig()
{
	// Bloqueia sinais para não disparar onSettingsChanged durante o carregamento
	commandInput->blockSignals(true);
	noGameCommandInput->blockSignals(true);
	autoExecuteCheckbox->blockSignals(true);
	twitchActionComboBox->blockSignals(true);

	commandInput->setText(ConfigManager::get().getCommand());
	noGameCommandInput->setText(ConfigManager::get().getNoGameCommand());
	autoExecuteCheckbox->setChecked(ConfigManager::get().getExecuteAutomatically());
	twitchActionComboBox->setCurrentIndex(ConfigManager::get().getTwitchActionMode());

	commandInput->blockSignals(false);
	noGameCommandInput->blockSignals(false);
	autoExecuteCheckbox->blockSignals(false);
	twitchActionComboBox->blockSignals(false);

	// Garante que a UI reflita o estado inicial
	updateActionModeUI(twitchActionComboBox->currentIndex());
}

void GameDetectorDock::executeGameCommand(const QString &gameName)
{
	QString commandTemplate = commandInput->text();
	if (commandTemplate.isEmpty()) return;

	QString command = commandTemplate.replace("{game}", gameName);
	TwitchChatBot::get().sendMessage(command);
}

void GameDetectorDock::updateActionModeUI(int index)
{
	bool isApiMode = (index == 1);

	commandLabel->setVisible(!isApiMode);
	commandInput->setVisible(!isApiMode);
	noGameCommandLabel->setVisible(!isApiMode);
	noGameCommandInput->setVisible(!isApiMode);

	// Altera o texto do botão de execução manual
	executeCommandButton->setText(isApiMode ? obs_module_text("Dock.ExecuteAction")
						: obs_module_text("Dock.ExecuteCommand"));
}

void GameDetectorDock::onCategoryUpdateFinished(bool success, const QString &gameName)
{
	if (success) {
		QString statusText = QString(obs_module_text("Dock.CategoryUpdated")).arg(gameName);
		g_statusLabel->setText(statusText);
	} else {
		QString statusText = QString(obs_module_text("Dock.CategoryUpdateFailed"));
		g_statusLabel->setText(statusText);
	}

	// Retorna ao status normal após alguns segundos
	QTimer::singleShot(3000, this, [this]() {
		g_statusLabel->setText(this->detectedGameName.isEmpty() ? obs_module_text("Status.Waiting") : QString(obs_module_text("Status.Playing")).arg(this->detectedGameName));
	});
}

GameDetectorDock::~GameDetectorDock()
{
	// O Qt parent/child system geralmente cuida da limpeza.
}