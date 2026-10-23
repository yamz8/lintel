#pragma once

#include <QFuture>
#include <QObject>
#include <QUrl>

class Backend;
class FilePicker;
class Markups;

// Exports the open set with its markups from the window: asks where through
// the file picker, then writes in the background.
class ExportJob : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
    ExportJob(Backend *backend, Markups *markups, FilePicker *picker, QObject *parent = nullptr);
    // Quitting mid-export lets the export finish, so the file isn't lost.
    ~ExportJob() override;

    bool busy() const { return m_busy; }
    Q_INVOKABLE void start();
    void exportTo(const QUrl &url, bool markedOnly);

signals:
    void changed();
    void finished(const QString &path);
    void failed(const QString &message);

private:
    Backend *m_backend;
    Markups *m_markups;
    FilePicker *m_picker;
    bool m_busy = false;
    QFuture<void> m_future;
};
