#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProgressDialog>
#include "ui_WowArmoryImportDialog.h"
#include "core/modeling/Model.h"

class WowArmoryImportDialog : public QDialog
{
    Q_OBJECT

public:
    WowArmoryImportDialog(QWidget *parent = nullptr);
    ~WowArmoryImportDialog();

    struct ImportSettings {
        QString server;
        QString username;
        QString region;
        bool clearExistingModels;
        bool importCustomizations;
        bool importEquipment;
    };

    const ImportSettings& getImportSettings() const { return importSettings; }
    bool hasSuccessfulImport() const { return importSuccessful; }
    const QJsonObject& getCharacterData() const { return characterData; }

signals:
    void importCompleted(const QJsonObject& characterData, const ImportSettings& settings);

private slots:
    void onImportClicked();
    void onNetworkReplyFinished();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    Ui::WowArmoryImportDialogClass ui;
    QNetworkAccessManager* networkManager;
    QNetworkReply* currentReply;
    QProgressDialog* progressDialog;

    ImportSettings importSettings;
    QJsonObject characterData;
    bool importSuccessful;

    QString buildApiUrl(const QString& region, const QString& server, const QString& username);
    void processCharacterData(const QJsonObject& data);
    void showError(const QString& message);
};