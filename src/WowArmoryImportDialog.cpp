#include "stdafx.h"
#include "WowArmoryImportDialog.h"
#include <QMessageBox>
#include <QJsonParseError>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QPushButton>
#include "core/utility/Logger.h"

WowArmoryImportDialog::WowArmoryImportDialog(QWidget *parent)
    : QDialog(parent), networkManager(nullptr), currentReply(nullptr), progressDialog(nullptr), importSuccessful(false)
{
    ui.setupUi(this);

    networkManager = new QNetworkAccessManager(this);
    
    // Override the default OK button to use our custom import logic
    ui.buttonBox->button(QDialogButtonBox::Ok)->setText("Import");
    disconnect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui.buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &WowArmoryImportDialog::onImportClicked);
    
    connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

WowArmoryImportDialog::~WowArmoryImportDialog()
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
    }
    if (progressDialog) {
        progressDialog->deleteLater();
    }
}

void WowArmoryImportDialog::onImportClicked()
{
    // Validate input
    QString region = ui.comboBoxRegion->currentText().trimmed();
    QString server = ui.lineEditServer->text().trimmed();
    QString username = ui.lineEditUsername->text().trimmed();

    if (server.isEmpty() || username.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter both server name and character name.");
        return;
    }

    // Collect settings
    importSettings.region = region;
    importSettings.server = server;
    importSettings.username = username;
    importSettings.clearExistingModels = ui.checkBoxClearExisting->isChecked();
    importSettings.importCustomizations = ui.checkBoxImportCustomizations->isChecked();
    importSettings.importEquipment = ui.checkBoxImportEquipment->isChecked();

    // Build API URL
    QString apiUrl = buildApiUrl(region, server, username);
    
    core::Log::message("Fetching character data from: " + apiUrl);

    // Setup progress dialog
    progressDialog = new QProgressDialog("Fetching character data...", "Cancel", 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    // Make network request
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setHeader(QNetworkRequest::UserAgentHeader, "WMVx/0.5.1");

    currentReply = networkManager->get(request);
    connect(currentReply, &QNetworkReply::finished, this, &WowArmoryImportDialog::onNetworkReplyFinished);
    connect(currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), 
            this, &WowArmoryImportDialog::onNetworkError);
    
    connect(progressDialog, &QProgressDialog::canceled, [this]() {
        if (currentReply) {
            currentReply->abort();
        }
    });
}

void WowArmoryImportDialog::onNetworkReplyFinished()
{
    if (progressDialog) {
        progressDialog->hide();
        progressDialog->deleteLater();
        progressDialog = nullptr;
    }

    if (!currentReply) {
        return;
    }

    if (currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error: %1").arg(currentReply->errorString());
        showError(errorMsg);
        currentReply->deleteLater();
        currentReply = nullptr;
        return;
    }

    QByteArray data = currentReply->readAll();
    currentReply->deleteLater();
    currentReply = nullptr;

    // Parse JSON response
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QString errorMsg = QString("Failed to parse JSON response: %1").arg(parseError.errorString());
        showError(errorMsg);
        return;
    }

    if (!jsonDoc.isObject()) {
        showError("Invalid JSON response format.");
        return;
    }

    characterData = jsonDoc.object();

    // Check if the response contains an error
    if (characterData.contains("code") && characterData.contains("detail")) {
        QString errorMsg = QString("API Error: %1 - %2")
            .arg(characterData["code"].toString())
            .arg(characterData["detail"].toString());
        showError(errorMsg);
        return;
    }

    // Validate required fields
    if (!characterData.contains("character") || !characterData.contains("items")) {
        showError("Invalid character data received from API.");
        return;
    }

    core::Log::message("Successfully fetched character data for: " + 
                      characterData["character"].toObject()["name"].toString());

    importSuccessful = true;
    emit importCompleted(characterData, importSettings);
    accept();
}

void WowArmoryImportDialog::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    // Error handling is done in onNetworkReplyFinished()
}

QString WowArmoryImportDialog::buildApiUrl(const QString& region, const QString& server, const QString& username)
{
    QString baseUrl = QString("https://%1.api.blizzard.com/profile/wow/character/%2/%3/appearance")
        .arg(region)
        .arg(server.toLower())
        .arg(username.toLower());

    QUrlQuery query;
    query.addQueryItem("namespace", QString("profile-%1").arg(region));
    query.addQueryItem("locale", "en_US");

    QUrl url(baseUrl);
    url.setQuery(query);
    
    return url.toString();
}

void WowArmoryImportDialog::showError(const QString& message)
{
    QMessageBox::critical(this, "Import Error", message);
    core::Log::message("WoW Armory Import Error: " + message);
}