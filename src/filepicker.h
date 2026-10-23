// The file picker interface follows Omacut (MIT, David Heinemeier Hansson),
// so tests can hand the app a fake picker instead of the portal.
#pragma once

#include <QObject>
#include <QUrl>

class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    virtual void openPdf(const QString &title) = 0;
    // Offers to save a PDF, with a choice between every sheet and only the
    // marked-up ones when offerMarkedOnly is set.
    virtual void savePdf(const QUrl &suggestedUrl, const QString &title, bool offerMarkedOnly) = 0;

signals:
    void openSelected(const QUrl &url);
    void saveSelected(const QUrl &url, bool markedOnly);
    void failed(const QString &message);
};
